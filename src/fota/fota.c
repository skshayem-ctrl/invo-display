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

volatile bool fota_active = false;

static volatile fota_state_t s_state = FOTA_IDLE;
static volatile bool s_cancel = false;
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

/* ── Version check via version.json ─────────────────────────────────────── */

typedef struct {
    char buf[256];
    int  buf_len;
    bool error;
} ver_ctx_t;

static esp_err_t ver_http_handler(esp_http_client_event_t *evt)
{
    ver_ctx_t *ctx = (ver_ctx_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int copy = evt->data_len;
        if (ctx->buf_len + copy < (int)sizeof(ctx->buf) - 1) {
            memcpy(ctx->buf + ctx->buf_len, evt->data, copy);
            ctx->buf_len += copy;
            ctx->buf[ctx->buf_len] = '\0';
        } else {
            ctx->error = true;
        }
    }
    return ESP_OK;
}

/* Parse "version":"x.y.z" from JSON (no cJSON dependency). */
static bool parse_version_json(const char *json, char *ver_out, int ver_len)
{
    const char *p = strstr(json, "\"version\"");
    if (!p) return false;
    p += 9;
    while (*p && (*p == ' ' || *p == ':' || *p == '"')) p++;
    int i = 0;
    while (*p && *p != '"' && i < ver_len - 1) ver_out[i++] = *p++;
    ver_out[i] = '\0';
    return i > 0;
}

static bool fetch_remote_version(const char *local_ver, char *remote_ver_out, int out_len)
{
    ver_ctx_t vctx = {0};
    esp_http_client_config_t cfg = {
        .url               = FOTA_VER_URL,
        .timeout_ms        = 30000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size       = 4096,
        .event_handler     = ver_http_handler,
        .user_data         = &vctx,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;

    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || vctx.error || vctx.buf_len == 0) {
        ESP_LOGE(TAG, "version.json fetch failed: %s", esp_err_to_name(err));
        return false;
    }

    bool ok = parse_version_json(vctx.buf, remote_ver_out, out_len);
    if (ok) {
        ESP_LOGI(TAG, "local=%s remote=%s", local_ver, remote_ver_out);
    } else {
        ESP_LOGE(TAG, "version.json parse failed: %s", vctx.buf);
    }
    return ok;
}

/* ── Binary download ─────────────────────────────────────────────────────── */

/* IDF 5.x magic in esp_app_desc_t (at binary offset 32) */
#define ESP_APP_DESC_MAGIC  0xABCD5432u

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
        if (ctx->error || s_cancel) return ESP_FAIL;

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

/* ── FOTA task ───────────────────────────────────────────────────────────── */

static void fota_task(void *arg)
{
    if (!wifi_manager_connected()) {
        notify(FOTA_NO_WIFI, 0, "No WiFi");
        vTaskDelete(NULL);
        return;
    }

    /* Pause weather fetches before touching SDIO with any HTTPS traffic */
    fota_active = true;

    notify(FOTA_CHECKING, 0, "Checking for updates...");

    size_t heap_free = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Internal DRAM largest block: %u B", (unsigned)heap_free);
    if (heap_free < 25000) {
        fota_active = false;
        notify(FOTA_ERROR, 0, "Low heap — retry later");
        vTaskDelete(NULL);
        return;
    }

    /* Wait for any in-flight weather/AQI fetch to fully finish and DNS to recover.
     * AQI HTTP timeout is ~30s; blip cascade + DNS recovery adds ~15s on top. */
    for (int i = 0; i < 60 && !s_cancel; i++) vTaskDelay(pdMS_TO_TICKS(500));
    if (s_cancel) { fota_active = false; s_state = FOTA_IDLE; vTaskDelete(NULL); return; }

    const char *local_ver = esp_app_get_description()->version;

    /* Step 1: Fetch version.json — tiny response, clean TCP teardown.
     * This avoids downloading the binary at all when already up to date,
     * eliminating the 60-second SDIO cascade caused by aborting a CDN stream. */
    char remote_ver[33] = {0};
    bool version_ok = false;
    for (int attempt = 0; attempt < 3 && !version_ok; attempt++) {
        if (attempt > 0) vTaskDelay(pdMS_TO_TICKS(10000));
        if (!wifi_manager_connected()) continue;
        version_ok = fetch_remote_version(local_ver, remote_ver, sizeof(remote_ver));
    }

    if (!version_ok) {
        fota_active = false;
        notify(FOTA_ERROR, 0, "Version check failed");
        vTaskDelete(NULL);
        return;
    }

    if (!version_is_newer(remote_ver, local_ver)) {
        fota_active = false;
        char msg[64];
        snprintf(msg, sizeof(msg), "Already up to date (%s)", local_ver);
        notify(FOTA_UP_TO_DATE, 100, msg);
        vTaskDelete(NULL);
        return;
    }

    /* Step 2: Newer version confirmed.
     * Wait for version.json TCP teardown retransmits to finish (exponential
     * backoff: 1.4s→3s→6s→12s) and DNS to fully recover before touching github.com. */
    for (int i = 0; i < 60 && !s_cancel; i++) vTaskDelay(pdMS_TO_TICKS(500));
    if (s_cancel) { fota_active = false; s_state = FOTA_IDLE; vTaskDelete(NULL); return; }

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
            for (int i = 0; i < 60 && !s_cancel; i++) vTaskDelay(pdMS_TO_TICKS(500));
            if (s_cancel || !wifi_manager_connected()) continue;
        }

        const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
        if (!update_part) { ESP_LOGE(TAG, "No OTA partition"); continue; }

        esp_ota_handle_t ota_handle = 0;
        esp_err_t err = esp_ota_begin(update_part, OTA_SIZE_UNKNOWN, &ota_handle);
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
            .url                   = FOTA_BIN_URL,
            .timeout_ms            = 120000,
            .crt_bundle_attach     = esp_crt_bundle_attach,
            .buffer_size           = 16384,
            .buffer_size_tx        = 4096,
            .keep_alive_enable     = true,
            .keep_alive_idle       = 10,
            .keep_alive_interval   = 5,
            .max_redirection_count = 10,
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
            if (s_cancel) break;
            continue;
        }

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

    if (s_cancel) {
        fota_active = false;
        s_state = FOTA_IDLE;
        vTaskDelete(NULL);
        return;
    }

    if (ota_final != ESP_OK) {
        fota_active = false;
        notify(FOTA_ERROR, 0, "Download failed (5 retries)");
        vTaskDelete(NULL);
        return;
    }

    notify(FOTA_DONE, 100, "Done! Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    vTaskDelete(NULL);
}

void fota_cancel(void) { s_cancel = true; }

void fota_start(fota_cb_t cb)
{
    if (s_state == FOTA_CHECKING || s_state == FOTA_DOWNLOADING ||
        s_state == FOTA_VERIFYING) return;
    s_cancel = false;
    s_cb    = cb;
    s_state = FOTA_IDLE;
    BaseType_t ret = xTaskCreate(fota_task, "fota", 20480, NULL, 2, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "fota_task create failed (%d) — heap too low?", (int)ret);
        if (s_cb) s_cb(FOTA_ERROR, 0, "Task create failed");
    }
}

fota_state_t fota_get_state(void)       { return s_state; }
const char  *fota_current_version(void) { return esp_app_get_description()->version; }
