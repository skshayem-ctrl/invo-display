#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_hosted_event.h"
#include "wifi_manager.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAX_RETRY     INT32_MAX   /* keep retrying forever */
#define NVS_NS             "wifi_creds"

static const char *TAG = "wifi";
static EventGroupHandle_t s_eg;
static int  s_retry       = 0;
static volatile bool s_connected  = false;
static volatile bool s_time_synced = false;

/* scan state */
static volatile bool     s_scan_done  = false;
static uint16_t          s_ap_count   = 0;
static wifi_ap_record_t  s_ap_recs[20];

/* dynamic connect state */
static volatile wm_state_t s_state          = WM_IDLE;
static volatile bool       s_force_reconnect = false;
static volatile bool       s_has_credentials = false;
static char                s_new_ssid[33]   = {0};
static char                s_new_pass[65]   = {0};

/* ── NTP ────────────────────────────────────────────────────────── */

static void sntp_sync_cb(struct timeval *tv)
{
    setenv("TZ", "IST-5:30", 1);
    tzset();
    s_time_synced = true;
    ESP_LOGI(TAG, "NTP synced");
}

static void start_sntp(void)
{
    static volatile bool started = false;
    if (started) return;
    started = true;
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(sntp_sync_cb);
    esp_sntp_init();
}

/* ── Hosted transport event handler ────────────────────────────── */

static void hosted_event_handler(void *arg, esp_event_base_t base,
                                 int32_t id, void *data)
{
    if (id == ESP_HOSTED_EVENT_TRANSPORT_FAILURE) {
        /* Just log — don't touch s_connected. An SDIO write blip does NOT
         * mean the C6 lost its AP association. The C6 self-recovers and
         * WIFI_EVENT_STA_DISCONNECTED will fire if WiFi actually drops. */
        ESP_LOGW(TAG, "SDIO transport blip (C6 will recover)");
    }
}

/* ── Event handler ──────────────────────────────────────────────── */

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (s_has_credentials)
            esp_wifi_connect();

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        s_ap_count = 20;
        esp_wifi_scan_get_ap_records(&s_ap_count, s_ap_recs);
        s_scan_done = true;
        ESP_LOGI(TAG, "Scan done: %d APs", s_ap_count);

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        if (s_force_reconnect) {
            s_force_reconnect = false;
            wifi_config_t cfg = {};
            memcpy(cfg.sta.ssid,     s_new_ssid, 32);
            memcpy(cfg.sta.password, s_new_pass, 64);
            esp_wifi_set_config(WIFI_IF_STA, &cfg);
            esp_wifi_connect();
            s_retry = 0;
        } else if (s_has_credentials) {
            s_retry++;
            /* back off: 2s for first few retries, 10s after that */
            uint32_t delay_ms = (s_retry <= 3) ? 2000 : 10000;
            ESP_LOGI(TAG, "Retry %d (backoff %lu ms)", s_retry, (unsigned long)delay_ms);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            esp_wifi_connect();
        } else {
            s_state = WM_FAILED;
            xEventGroupSetBits(s_eg, WIFI_FAIL_BIT);
        }

    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        s_retry     = 0;
        s_connected = true;
        s_state     = WM_CONNECTED;
        xEventGroupSetBits(s_eg, WIFI_CONNECTED_BIT);
        /* start NTP only once */
        if (!s_time_synced)
            start_sntp();
    }
}

/* ── Init ───────────────────────────────────────────────────────── */

void wifi_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_eg = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t icfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t wifi_ret = esp_wifi_init(&icfg);
    if (wifi_ret != ESP_OK) {
        /* C6 SDIO slave not ready yet (common after TTS restart).
         * Give it 3 more seconds then try a clean reboot. */
        ESP_LOGW(TAG, "esp_wifi_init failed (%s) — restarting",
                 esp_err_to_name(wifi_ret));
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
                        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
                        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
                        ESP_HOSTED_EVENT, ESP_HOSTED_EVENT_TRANSPORT_FAILURE,
                        hosted_event_handler, NULL, NULL));

    /* Load saved credentials from NVS, fall back to Kconfig */
    char ssid[33] = CONFIG_ESP_WIFI_SSID;
    char pass[65] = CONFIG_ESP_WIFI_PASSWORD;
    nvs_handle_t nvs;
    if (nvs_open(NVS_NS, NVS_READONLY, &nvs) == ESP_OK) {
        size_t n = sizeof(ssid);
        if (nvs_get_str(nvs, "ssid", ssid, &n) == ESP_OK) {
            n = sizeof(pass);
            nvs_get_str(nvs, "pass", pass, &n);
            ESP_LOGI(TAG, "NVS SSID: %s", ssid);
        }
        nvs_close(nvs);
    }

    s_has_credentials = (ssid[0] != '\0');

    wifi_config_t wifi_cfg = {};
    memcpy(wifi_cfg.sta.ssid,     ssid, 32);
    memcpy(wifi_cfg.sta.password, pass, 64);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    if (s_has_credentials) {
        s_state = WM_CONNECTING;
        EventBits_t bits = xEventGroupWaitBits(s_eg,
                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                               pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
        if (!(bits & WIFI_CONNECTED_BIT)) {
            s_state = WM_FAILED;
            ESP_LOGW(TAG, "Initial connect failed");
        }
    } else {
        s_state = WM_IDLE;
        ESP_LOGI(TAG, "No saved credentials — waiting for user to connect");
    }
}

/* ── Public API ─────────────────────────────────────────────────── */

bool wifi_manager_connected(void)   { return s_connected; }
bool wifi_manager_time_synced(void) { return s_time_synced; }
wm_state_t wifi_manager_conn_state(void) { return s_state; }

void wifi_manager_scan_start(void)
{
    s_scan_done = false;
    s_ap_count  = 0;
    wifi_scan_config_t cfg = { .show_hidden = false };
    esp_wifi_scan_start(&cfg, false);
}

bool wifi_manager_scan_done(void) { return s_scan_done; }
int  wifi_manager_ap_count(void)  { return (int)s_ap_count; }

bool wifi_manager_ap_info(int idx, char ssid[33], int8_t *rssi, bool *secured)
{
    if (idx < 0 || idx >= (int)s_ap_count) return false;
    memcpy(ssid, s_ap_recs[idx].ssid, 33);
    *rssi    = s_ap_recs[idx].rssi;
    *secured = s_ap_recs[idx].authmode != WIFI_AUTH_OPEN;
    return true;
}

void wifi_manager_connect_to(const char *ssid, const char *pass)
{
    s_has_credentials = true;

    /* Save to NVS */
    nvs_handle_t nvs;
    if (nvs_open(NVS_NS, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "pass", pass);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    strncpy(s_new_ssid, ssid, sizeof(s_new_ssid) - 1);
    strncpy(s_new_pass, pass, sizeof(s_new_pass) - 1);
    s_force_reconnect = true;
    s_state           = WM_CONNECTING;
    s_retry           = 0;

    if (s_connected)
        esp_wifi_disconnect();          /* handler will reconnect */
    else {
        s_force_reconnect = false;
        wifi_config_t cfg = {};
        memcpy(cfg.sta.ssid,     s_new_ssid, 32);
        memcpy(cfg.sta.password, s_new_pass, 64);
        esp_wifi_set_config(WIFI_IF_STA, &cfg);
        esp_wifi_connect();
    }
}
