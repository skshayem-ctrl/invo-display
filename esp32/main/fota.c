#include "fota.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "cJSON.h"
#include "wifi_manager.h"

#define TAG          "fota"
#define VER_BUF_SIZE 512

static volatile fota_state_t s_state = FOTA_IDLE;
static fota_cb_t             s_cb;

static void notify(fota_state_t state, int pct, const char *msg)
{
    s_state = state;
    if (s_cb) s_cb(state, pct, msg);
}

static bool version_is_newer(const char *remote, const char *local)
{
    int rm = 0, rn = 0, rp = 0;
    int lm = 0, ln = 0, lp = 0;
    sscanf(remote, "%d.%d.%d", &rm, &rn, &rp);
    sscanf(local,  "%d.%d.%d", &lm, &ln, &lp);
    if (rm != lm) return rm > lm;
    if (rn != ln) return rn > ln;
    return rp > lp;
}

static void fota_task(void *arg)
{
    /* ── 1. WiFi check ──────────────────────────────────────────── */
    if (!wifi_manager_connected()) {
        notify(FOTA_NO_WIFI, 0, "No WiFi connection");
        vTaskDelete(NULL);
        return;
    }

    /* ── 2. Fetch version.json ──────────────────────────────────── */
    notify(FOTA_CHECKING, 0, "Checking for updates...");

    esp_http_client_config_t ver_cfg = {
        .url               = FOTA_VERSION_URL,
        .timeout_ms        = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&ver_cfg);

    char ver_buf[VER_BUF_SIZE] = {0};
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        notify(FOTA_ERROR, 0, "Server unreachable");
        vTaskDelete(NULL);
        return;
    }
    esp_http_client_fetch_headers(client);
    int rd = esp_http_client_read(client, ver_buf, sizeof(ver_buf) - 1);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (rd <= 0) {
        notify(FOTA_ERROR, 0, "Empty response");
        vTaskDelete(NULL);
        return;
    }

    /* ── 3. Parse JSON ──────────────────────────────────────────── */
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
    const char *fw_url     = jurl->valuestring;
    const char *local_ver  = esp_app_get_description()->version;

    ESP_LOGI(TAG, "local=%s remote=%s", local_ver, remote_ver);

    if (!version_is_newer(remote_ver, local_ver)) {
        cJSON_Delete(root);
        char msg[64];
        snprintf(msg, sizeof(msg), "Already up to date (%s)", local_ver);
        notify(FOTA_UP_TO_DATE, 100, msg);
        vTaskDelete(NULL);
        return;
    }

    /* ── 4. Download + flash (chunked, up to 3 retries) ────────── */
    notify(FOTA_DOWNLOADING, 0, "Downloading...");

    /* Wait for SDIO to settle after the version-check TLS session closes */
    vTaskDelay(pdMS_TO_TICKS(8000));

    esp_http_client_config_t dl_cfg = {
        .url                 = fw_url,
        .timeout_ms          = 120000, /* 2 min — enough for full 1.6MB download */
        .crt_bundle_attach   = esp_crt_bundle_attach,
        .buffer_size         = 16384,
        .buffer_size_tx      = 4096,
        .keep_alive_enable   = true,
        .keep_alive_idle     = 5,
        .keep_alive_interval = 5,
    };
    esp_https_ota_config_t ota_cfg = {
        .http_config = &dl_cfg,   /* single connection, no per-chunk TLS handshakes */
    };

    esp_err_t ota_final_err = ESP_FAIL;

    for (int attempt = 0; attempt < 5; attempt++) {
        if (attempt > 0) {
            char rmsg[24];
            snprintf(rmsg, sizeof(rmsg), "Retry %d/5...", attempt);
            notify(FOTA_DOWNLOADING, 0, rmsg);
            vTaskDelay(pdMS_TO_TICKS(15000));
        }

        if (!wifi_manager_connected()) {
            continue;
        }

        esp_https_ota_handle_t ota_handle = NULL;
        err = esp_https_ota_begin(&ota_cfg, &ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ota_begin failed: %s", esp_err_to_name(err));
            continue;
        }

        char dl_msg[32];
        bool dl_ok = true;
        while (1) {
            err = esp_https_ota_perform(ota_handle);
            if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
                int done  = esp_https_ota_get_image_len_read(ota_handle);
                int total = esp_https_ota_get_image_size(ota_handle);
                if (total > 0) {
                    int pct = (int)((float)done / total * 100.0f);
                    snprintf(dl_msg, sizeof(dl_msg), "Downloading... %d%%", pct);
                    notify(FOTA_DOWNLOADING, pct, dl_msg);
                }
                vTaskDelay(pdMS_TO_TICKS(20));
            } else if (err == ESP_OK) {
                break;
            } else {
                ESP_LOGE(TAG, "ota_perform failed: %s", esp_err_to_name(err));
                dl_ok = false;
                break;
            }
        }

        if (!dl_ok || !esp_https_ota_is_complete_data_received(ota_handle)) {
            esp_https_ota_abort(ota_handle);
            continue;
        }

        ota_final_err = esp_https_ota_finish(ota_handle);
        break;
    }

    cJSON_Delete(root);

    if (ota_final_err != ESP_OK) {
        if (ota_final_err == ESP_ERR_OTA_VALIDATE_FAILED)
            notify(FOTA_ERROR, 0, "Firmware invalid");
        else
            notify(FOTA_ERROR, 0, "Download failed (5 retries)");
        vTaskDelete(NULL);
        return;
    }

    /* ── 5. Done ────────────────────────────────────────────────── */
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
    xTaskCreate(fota_task, "fota", 16384, NULL, 5, NULL);
}

fota_state_t fota_get_state(void)       { return s_state; }
const char  *fota_current_version(void) { return esp_app_get_description()->version; }
