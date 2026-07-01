#include "fota.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "wifi_manager.h"

#define TAG             "fota"
#define OTA_CHUNK_SIZE  512
#define OTA_CHUNK_SLEEP 50   /* ms — yield to SDIO write task between flash writes */

/* IDF 5.x magic in esp_app_desc_t (at binary offset 32) */
#define ESP_APP_DESC_MAGIC  0xABCD5432u

static volatile fota_state_t s_state = FOTA_IDLE;
static fota_cb_t s_cb;

static void notify(fota_state_t state, int pct, const char *msg)
{
    s_state = state;
    if (s_cb) s_cb(state, pct, msg);
}

static bool version_is_newer(const char *remote, const char *local)
{
    int rm=0, rn=0, rp=0, lm=0, ln=0, lp=0;
    sscanf(remote, "%d.%d.%d", &rm, &rn, &rp);
    sscanf(local,  "%d.%d.%d", &lm, &ln, &lp);
    if (rm != lm) return rm > lm;
    if (rn != ln) return rn > ln;
    return rp > lp;
}

/* esp_app_desc_t starts at binary offset 32, 'version' field is at offset 16
 * within that struct → absolute binary offset 48. */
static bool parse_app_version(const uint8_t *buf, int len, char *out, int out_len)
{
    if (len < 80) return false;
    uint32_t magic;
    memcpy(&magic, buf + 32, sizeof(magic));
    if (magic != ESP_APP_DESC_MAGIC) {
        ESP_LOGW(TAG, "app_desc magic mismatch: 0x%08lx", (unsigned long)magic);
        return false;
    }
    snprintf(out, out_len, "%.*s", 32, (const char *)(buf + 48));
    return true;
}

typedef struct {
    esp_ota_handle_t  ota_handle;
    const char       *local_ver;
    uint8_t          *buf;
    int               buf_fill;
    int               total_written;
    int               content_length;
    bool              version_parsed;
    bool              bail_early;   /* same version — intentional abort */
    bool              error;
    fota_cb_t         cb;
} ota_ctx_t;

static esp_err_t ota_http_handler(esp_http_client_event_t *evt)
{
    ota_ctx_t *ctx = (ota_ctx_t *)evt->user_data;

    switch (evt->event_id) {

    case HTTP_EVENT_ON_HEADER:
        if (strcasecmp(evt->header_key, "Content-Length") == 0)
            ctx->content_length = atoi(evt->header_value);
        break;

    case HTTP_EVENT_ON_DATA: {
        if (ctx->error || ctx->bail_early) return ESP_FAIL;

        const uint8_t *src    = (const uint8_t *)evt->data;
        int            remain = evt->data_len;

        while (remain > 0) {
            int space = OTA_CHUNK_SIZE - ctx->buf_fill;
            int copy  = (remain < space) ? remain : space;
            memcpy(ctx->buf + ctx->buf_fill, src, copy);
            ctx->buf_fill += copy;
            src           += copy;
            remain        -= copy;

            /* Version check inline: read from the first 80 bytes of the binary.
             * This avoids a separate version.json TLS connection entirely. */
            if (!ctx->version_parsed && ctx->buf_fill >= 80) {
                char remote_ver[33] = {0};
                if (!parse_app_version(ctx->buf, ctx->buf_fill,
                                       remote_ver, sizeof(remote_ver))) {
                    ESP_LOGE(TAG, "Failed to parse remote version from binary");
                    ctx->error = true;
                    return ESP_FAIL;
                }
                ctx->version_parsed = true;
                ESP_LOGI(TAG, "local=%s remote=%s", ctx->local_ver, remote_ver);

                if (!version_is_newer(remote_ver, ctx->local_ver)) {
                    ctx->bail_early = true;
                    return ESP_FAIL; /* abort download cleanly */
                }
                if (ctx->cb) ctx->cb(FOTA_DOWNLOADING, 0, "Downloading...");
            }

            if (ctx->buf_fill == OTA_CHUNK_SIZE) {
                if (!ctx->version_parsed) {
                    ctx->error = true;
                    return ESP_FAIL;
                }
                esp_err_t err = esp_ota_write(ctx->ota_handle, ctx->buf, ctx->buf_fill);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "ota_write: %s", esp_err_to_name(err));
                    ctx->error = true;
                    return ESP_FAIL;
                }
                ctx->total_written += ctx->buf_fill;
                ctx->buf_fill = 0;

                int pct = ctx->content_length > 0
                    ? (int)((float)ctx->total_written / ctx->content_length * 100.0f)
                    : 0;
                char msg[32];
                snprintf(msg, sizeof(msg), "Downloading... %d%%", pct);
                if (ctx->cb) ctx->cb(FOTA_DOWNLOADING, pct, msg);
                ESP_LOGI(TAG, "OTA chunk: %d/%d (%d%%)",
                         ctx->total_written, ctx->content_length, pct);

                /* Let C6 SDIO FIFO drain before the next chunk arrives */
                vTaskDelay(pdMS_TO_TICKS(OTA_CHUNK_SLEEP));
            }
        }
        break;
    }

    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP transport error");
        break;

    default: break;
    }
    return ESP_OK;
}

static void fota_task(void *arg)
{
    /* 1. WiFi */
    if (!wifi_manager_connected()) {
        notify(FOTA_NO_WIFI, 0, "No WiFi");
        vTaskDelete(NULL);
        return;
    }

    notify(FOTA_CHECKING, 0, "Checking for updates...");

    /* 2. Heap */
    size_t heap_free = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Internal DRAM largest block: %u B", (unsigned)heap_free);
    if (heap_free < 25000) {
        notify(FOTA_ERROR, 0, "Low heap — retry later");
        vTaskDelete(NULL);
        return;
    }

    /* 3. Let any in-flight weather/NTP traffic clear the SDIO queue */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* 4. Chunk buffer */
    uint8_t *chunk_buf = malloc(OTA_CHUNK_SIZE);
    if (!chunk_buf) {
        notify(FOTA_ERROR, 0, "OOM");
        vTaskDelete(NULL);
        return;
    }

    const char *local_ver  = esp_app_get_description()->version;
    esp_err_t   ota_final  = ESP_FAIL;
    bool        up_to_date = false;

    /* 5. Download binary — version is checked inline from the first 80 bytes.
     *    Only one TLS connection total: no separate version.json fetch. */
    for (int attempt = 0; attempt < 5 && !up_to_date; attempt++) {
        if (attempt > 0) {
            char rmsg[24];
            snprintf(rmsg, sizeof(rmsg), "Retry %d/5...", attempt);
            notify(FOTA_DOWNLOADING, 0, rmsg);
            vTaskDelay(pdMS_TO_TICKS(30000));
            if (!wifi_manager_connected()) continue;
        }

        const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
        if (!update_part) { ESP_LOGE(TAG, "No OTA partition"); continue; }

        esp_ota_handle_t ota_handle = 0;
        esp_err_t err = esp_ota_begin(update_part, OTA_SIZE_UNKNOWN, &ota_handle);
        if (err != ESP_OK) { ESP_LOGE(TAG, "ota_begin: %s", esp_err_to_name(err)); continue; }

        ota_ctx_t ctx = {
            .ota_handle     = ota_handle,
            .local_ver      = local_ver,
            .buf            = chunk_buf,
            .buf_fill       = 0,
            .total_written  = 0,
            .content_length = 0,
            .version_parsed = false,
            .bail_early     = false,
            .error          = false,
            .cb             = s_cb,
        };

        esp_http_client_config_t dl_cfg = {
            .url                   = FOTA_BIN_URL,
            .timeout_ms            = 120000,
            .crt_bundle_attach     = esp_crt_bundle_attach,
            .buffer_size           = 16384,  /* GitHub CDN headers can exceed 8 KB */
            .buffer_size_tx        = 4096,   /* redirect URL with X-Amz-* params */
            .keep_alive_enable     = true,
            .keep_alive_idle       = 10,
            .keep_alive_interval   = 5,
            .max_redirection_count = 10,     /* GitHub → S3 redirect chain */
            .event_handler         = ota_http_handler,
            .user_data             = &ctx,
        };

        esp_http_client_handle_t client = esp_http_client_init(&dl_cfg);
        if (!client) {
            ESP_LOGE(TAG, "http_client_init failed");
            esp_ota_abort(ota_handle);
            continue;
        }

        err = esp_http_client_perform(client);
        esp_http_client_cleanup(client);

        if (ctx.bail_early) {
            esp_ota_abort(ota_handle);
            char msg[64];
            snprintf(msg, sizeof(msg), "Already up to date (%s)", local_ver);
            notify(FOTA_UP_TO_DATE, 100, msg);
            up_to_date = true;
            break;
        }

        if (ctx.error || err != ESP_OK) {
            ESP_LOGE(TAG, "Download failed: %s ctx.error=%d", esp_err_to_name(err), ctx.error);
            esp_ota_abort(ota_handle);
            continue;
        }

        /* Flush trailing partial chunk */
        if (ctx.buf_fill > 0) {
            err = esp_ota_write(ota_handle, chunk_buf, ctx.buf_fill);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "final ota_write: %s", esp_err_to_name(err));
                esp_ota_abort(ota_handle);
                continue;
            }
            ctx.total_written += ctx.buf_fill;
        }

        if (ctx.content_length > 0 && ctx.total_written < ctx.content_length) {
            ESP_LOGE(TAG, "Incomplete: %d/%d", ctx.total_written, ctx.content_length);
            esp_ota_abort(ota_handle);
            continue;
        }

        err = esp_ota_end(ota_handle);
        if (err != ESP_OK) { ESP_LOGE(TAG, "ota_end: %s", esp_err_to_name(err)); continue; }

        err = esp_ota_set_boot_partition(update_part);
        if (err != ESP_OK) { ESP_LOGE(TAG, "set_boot_partition: %s", esp_err_to_name(err)); continue; }

        ota_final = ESP_OK;
        break;
    }

    free(chunk_buf);

    if (up_to_date) {
        vTaskDelete(NULL);
        return;
    }

    if (ota_final != ESP_OK) {
        notify(FOTA_ERROR, 0, "Download failed (5 retries)");
        vTaskDelete(NULL);
        return;
    }

    notify(FOTA_DONE, 100, "Done! Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    vTaskDelete(NULL);
}

void fota_start(fota_cb_t cb)
{
    if (s_state == FOTA_CHECKING || s_state == FOTA_DOWNLOADING ||
        s_state == FOTA_VERIFYING) return;
    s_cb    = cb;
    s_state = FOTA_IDLE;
    xTaskCreate(fota_task, "fota", 16384, NULL, 2, NULL);
}

fota_state_t fota_get_state(void)       { return s_state; }
const char  *fota_current_version(void) { return esp_app_get_description()->version; }
