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
#include "cJSON.h"
#include "wifi_manager.h"

#define TAG             "fota"
#define OTA_CHUNK_SIZE  512
#define OTA_CHUNK_SLEEP 50    /* ms — yield to SDIO write task between flash writes */
#define VER_BUF_SIZE    512

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

/* Collects HTTP response body for the version.json fetch */
typedef struct {
    char *buf;
    int   len;
    int   max;
} ver_collect_t;

static esp_err_t ver_http_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
    ver_collect_t *vc = (ver_collect_t *)evt->user_data;
    int copy = evt->data_len;
    if (vc->len + copy >= vc->max) return ESP_OK; /* truncate silently */
    memcpy(vc->buf + vc->len, evt->data, copy);
    vc->len += copy;
    return ESP_OK;
}

typedef struct {
    esp_ota_handle_t  ota_handle;
    uint8_t          *buf;
    int               buf_fill;
    int               total_written;
    int               content_length;
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
        if (ctx->error) return ESP_FAIL;

        const uint8_t *src    = (const uint8_t *)evt->data;
        int            remain = evt->data_len;

        while (remain > 0) {
            int space = OTA_CHUNK_SIZE - ctx->buf_fill;
            int copy  = (remain < space) ? remain : space;
            memcpy(ctx->buf + ctx->buf_fill, src, copy);
            ctx->buf_fill += copy;
            src           += copy;
            remain        -= copy;

            if (ctx->buf_fill == OTA_CHUNK_SIZE) {
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

    /* 2. Heap — need chunk buf + OTA write buffers */
    size_t heap_free = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Internal DRAM largest block: %u B", (unsigned)heap_free);
    if (heap_free < 25000) {
        notify(FOTA_ERROR, 0, "Low heap — retry later");
        vTaskDelete(NULL);
        return;
    }

    /* 3. Version check — retry up to 3 times with 10 s gap.
     *    WiFi reconnects quickly after an SDIO blip but DNS takes a few more
     *    seconds; retrying avoids a spurious getaddrinfo failure. */
    notify(FOTA_CHECKING, 0, "Checking for updates...");

    char ver_buf[VER_BUF_SIZE];
    esp_err_t err = ESP_FAIL;
    int http_status = 0;

    for (int vtry = 0; vtry < 3; vtry++) {
        if (vtry > 0) {
            ESP_LOGW(TAG, "Version check retry %d/3 in 10s...", vtry + 1);
            vTaskDelay(pdMS_TO_TICKS(10000));
            if (!wifi_manager_connected()) continue;
        }

        memset(ver_buf, 0, sizeof(ver_buf));
        ver_collect_t vc = { .buf = ver_buf, .len = 0, .max = VER_BUF_SIZE - 1 };

        esp_http_client_config_t ver_cfg = {
            .url                   = FOTA_VERSION_URL,
            .timeout_ms            = 15000,
            .crt_bundle_attach     = esp_crt_bundle_attach,
            .max_redirection_count = 5,
            .event_handler         = ver_http_handler,
            .user_data             = &vc,
        };
        esp_http_client_handle_t ver_client = esp_http_client_init(&ver_cfg);
        if (!ver_client) continue;
        err = esp_http_client_perform(ver_client);
        http_status = esp_http_client_get_status_code(ver_client);
        esp_http_client_cleanup(ver_client);

        if (err == ESP_OK && http_status == 200 && vc.len > 0) break;
        ESP_LOGW(TAG, "Version fetch attempt %d failed: %s status=%d len=%d",
                 vtry + 1, esp_err_to_name(err), http_status, vc.len);
    }

    if (err != ESP_OK || http_status != 200 || ver_buf[0] == '\0') {
        ESP_LOGE(TAG, "Version check failed after 3 attempts");
        notify(FOTA_ERROR, 0, "Version check failed");
        vTaskDelete(NULL);
        return;
    }

    cJSON *root = cJSON_Parse(ver_buf);
    if (!root) {
        notify(FOTA_ERROR, 0, "Bad version JSON");
        vTaskDelete(NULL);
        return;
    }
    cJSON *jver = cJSON_GetObjectItem(root, "version");
    cJSON *jurl = cJSON_GetObjectItem(root, "url");
    if (!cJSON_IsString(jver) || !cJSON_IsString(jurl)) {
        cJSON_Delete(root);
        notify(FOTA_ERROR, 0, "Missing version/url");
        vTaskDelete(NULL);
        return;
    }

    const char *remote_ver = jver->valuestring;
    const char *local_ver  = esp_app_get_description()->version;
    char fw_url[256];
    snprintf(fw_url, sizeof(fw_url), "%s", jurl->valuestring);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "local=%s remote=%s", local_ver, remote_ver);

    if (!version_is_newer(remote_ver, local_ver)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Already up to date (%s)", local_ver);
        notify(FOTA_UP_TO_DATE, 100, msg);
        vTaskDelete(NULL);
        return;
    }

    /* 4. Download in 512-byte chunks — throttled to keep SDIO alive */
    notify(FOTA_DOWNLOADING, 0, "Downloading...");

    uint8_t *chunk_buf = malloc(OTA_CHUNK_SIZE);
    if (!chunk_buf) {
        notify(FOTA_ERROR, 0, "OOM");
        vTaskDelete(NULL);
        return;
    }

    esp_err_t ota_final = ESP_FAIL;

    for (int attempt = 0; attempt < 5; attempt++) {
        if (attempt > 0) {
            char rmsg[24];
            snprintf(rmsg, sizeof(rmsg), "Retry %d/5...", attempt);
            notify(FOTA_DOWNLOADING, 0, rmsg);
            /* 30 s: enough for SDIO cascades from prior attempt to fully settle */
            vTaskDelay(pdMS_TO_TICKS(30000));
            if (!wifi_manager_connected()) continue;
        }

        const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
        if (!update_part) { ESP_LOGE(TAG, "No OTA partition"); continue; }

        esp_ota_handle_t ota_handle = 0;
        err = esp_ota_begin(update_part, OTA_SIZE_UNKNOWN, &ota_handle);
        if (err != ESP_OK) { ESP_LOGE(TAG, "ota_begin: %s", esp_err_to_name(err)); continue; }

        ota_ctx_t ctx = {
            .ota_handle     = ota_handle,
            .buf            = chunk_buf,
            .buf_fill       = 0,
            .total_written  = 0,
            .content_length = 0,
            .error          = false,
            .cb             = s_cb,
        };

        esp_http_client_config_t dl_cfg = {
            .url                   = fw_url,
            .timeout_ms            = 120000,
            .crt_bundle_attach     = esp_crt_bundle_attach,
            .buffer_size           = 16384,  /* GitHub CDN x-amz-id-2 headers exceed 8 KB */
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
