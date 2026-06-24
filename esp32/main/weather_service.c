#include <string.h>
#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"

#include "ui_common.h"
#include "lvgl_port.h"
#include "wifi_manager.h"
#include "weather_service.h"
#include "weather_icon.h"

#define WEATHER_URL \
    "https://api.open-meteo.com/v1/forecast" \
    "?latitude=12.97&longitude=77.59" \
    "&current=temperature_2m,apparent_temperature," \
    "relative_humidity_2m,weather_code,wind_speed_10m" \
    "&daily=weather_code,temperature_2m_max,temperature_2m_min" \
    "&timezone=Asia%2FKolkata"

#define AQI_URL \
    "https://air-quality-api.open-meteo.com/v1/air-quality" \
    "?latitude=12.97&longitude=77.59" \
    "&current=us_aqi,pm2_5,pm10"

#define FETCH_INTERVAL_MS (10 * 60 * 1000)
#define HTTP_BUF_SIZE     4096

static const char *TAG = "weather";

/* ── WMO / AQI helpers ─────────────────────────────────────────── */

static const char *wmo_description(int code)
{
    if (code == 0)    return "Clear Sky";
    if (code <= 3)    return "Partly Cloudy";
    if (code <= 48)   return "Foggy";
    if (code <= 55)   return "Drizzle";
    if (code <= 65)   return "Rain";
    if (code <= 77)   return "Snow";
    if (code <= 82)   return "Showers";
    return "Thunderstorm";
}

/* Short version for the 84px forecast tiles */
static const char *wmo_desc_short(int code)
{
    if (code == 0)    return "Clear";
    if (code <= 3)    return "P.Cloudy";
    if (code <= 48)   return "Foggy";
    if (code <= 55)   return "Drizzle";
    if (code <= 65)   return "Rain";
    if (code <= 77)   return "Snow";
    if (code <= 82)   return "Showers";
    return "Thunder";
}



static const char *aqi_category(int aqi)
{
    if (aqi <= 50)  return "Good";
    if (aqi <= 100) return "Moderate";
    if (aqi <= 150) return "Unhealthy (Sensitive)";
    if (aqi <= 200) return "Unhealthy";
    if (aqi <= 300) return "Very Unhealthy";
    return "Hazardous";
}

static const char *aqi_description(int aqi)
{
    if (aqi <= 50)  return "Air is fresh and healthy";
    if (aqi <= 100) return "Acceptable air quality";
    if (aqi <= 150) return "Sensitive groups at risk";
    if (aqi <= 200) return "Limit outdoor activity";
    if (aqi <= 300) return "Avoid outdoor activity";
    return "Stay indoors";
}

static lv_color_t aqi_color(int aqi)
{
    if (aqi <= 50)  return C_GREEN;
    if (aqi <= 100) return C_AMBER;
    return C_RED;
}

/* ── HTTP helper ───────────────────────────────────────────────── */

static bool http_get(const char *url, char *buf, int buf_size)
{
    esp_http_client_config_t cfg = {
        .url               = url,
        .timeout_ms        = 20000,   /* 20s: covers TLS handshake over SDIO */
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    for (int attempt = 0; attempt < 3; attempt++) {
        if (attempt > 0) {
            ESP_LOGW(TAG, "HTTP retry %d for %s", attempt, url);
            vTaskDelay(pdMS_TO_TICKS(8000));
        }
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        bool ok = false;
        if (esp_http_client_open(client, 0) == ESP_OK) {
            esp_http_client_fetch_headers(client);
            int len = esp_http_client_read_response(client, buf, buf_size - 1);
            if (len > 0) {
                buf[len] = '\0';
                ok = true;
            }
        }
        esp_http_client_cleanup(client);
        if (ok) return true;
    }
    return false;
}

/* ── Fetch functions ───────────────────────────────────────────── */

static bool weather_fetch(void)
{
    static char buf[HTTP_BUF_SIZE];
    if (!http_get(WEATHER_URL, buf, HTTP_BUF_SIZE))
        return false;

    cJSON *root    = cJSON_Parse(buf);
    cJSON *current = root ? cJSON_GetObjectItem(root, "current") : NULL;
    bool ok = false;

    if (current) {
        cJSON *item;
        item = cJSON_GetObjectItem(current, "temperature_2m");
        if (item) gd.wx_c = (int)item->valuedouble;
        item = cJSON_GetObjectItem(current, "apparent_temperature");
        if (item) gd.wx_feels_c = (int)item->valuedouble;
        item = cJSON_GetObjectItem(current, "relative_humidity_2m");
        if (item) gd.humidity = item->valueint;
        item = cJSON_GetObjectItem(current, "weather_code");
        if (item) gd.wx_code = item->valueint;
        item = cJSON_GetObjectItem(current, "wind_speed_10m");
        if (item) gd.wx_wind_kmh = (float)item->valuedouble;
        ok = true;
        ESP_LOGI(TAG, "Current: %d°C feels %d°C hum %d%% wind %.1f code %d",
                 gd.wx_c, gd.wx_feels_c, gd.humidity, gd.wx_wind_kmh, gd.wx_code);
    }

    cJSON *daily = root ? cJSON_GetObjectItem(root, "daily") : NULL;
    if (daily) {
        cJSON *codes  = cJSON_GetObjectItem(daily, "weather_code");
        cJSON *hi_arr = cJSON_GetObjectItem(daily, "temperature_2m_max");
        cJSON *lo_arr = cJSON_GetObjectItem(daily, "temperature_2m_min");
        int n = codes ? cJSON_GetArraySize(codes) : 0;
        for (int i = 0; i < 7 && i < n; i++) {
            cJSON *c = cJSON_GetArrayItem(codes,  i);
            cJSON *h = hi_arr ? cJSON_GetArrayItem(hi_arr, i) : NULL;
            cJSON *l = lo_arr ? cJSON_GetArrayItem(lo_arr, i) : NULL;
            if (c) gd.fc_code[i] = c->valueint;
            if (h) gd.fc_hi[i]   = (int)h->valuedouble;
            if (l) gd.fc_lo[i]   = (int)l->valuedouble;
        }
        ESP_LOGI(TAG, "Forecast: %d days parsed", n < 7 ? n : 7);
    }

    cJSON_Delete(root);
    return ok;
}

static bool aqi_fetch(void)
{
    static char buf[1024];
    if (!http_get(AQI_URL, buf, 1024))
        return false;

    cJSON *root    = cJSON_Parse(buf);
    cJSON *current = root ? cJSON_GetObjectItem(root, "current") : NULL;
    bool ok = false;

    if (current) {
        cJSON *item;
        item = cJSON_GetObjectItem(current, "us_aqi");
        if (item) gd.aqi = item->valueint;
        item = cJSON_GetObjectItem(current, "pm2_5");
        if (item) gd.wx_pm25 = (int)item->valuedouble;
        item = cJSON_GetObjectItem(current, "pm10");
        if (item) gd.wx_pm10 = (int)item->valuedouble;
        ok = true;
        ESP_LOGI(TAG, "AQI: %d  PM2.5: %d  PM10: %d", gd.aqi, gd.wx_pm25, gd.wx_pm10);
    }

    cJSON_Delete(root);
    return ok;
}

/* ── UI update ─────────────────────────────────────────────────── */

static void update_ui(void)
{
    lvgl_acquire();

    /* current weather */
    if (app.w_wx_tmp)
        lv_label_set_text_fmt(app.w_wx_tmp, "%d\xC2\xB0""C", gd.wx_c);
    if (app.w_wx_cond)
        lv_label_set_text(app.w_wx_cond, wmo_description(gd.wx_code));
    if (app.w_wx_feels) {
        char tmp[48];
        snprintf(tmp, sizeof(tmp), LV_SYMBOL_GPS "  Feels like %d\xC2\xB0""C", gd.wx_feels_c);
        lv_label_set_text(app.w_wx_feels, tmp);
    }
    if (app.w_wx_wind) {
        char tmp[48];
        snprintf(tmp, sizeof(tmp), LV_SYMBOL_REFRESH "  Wind %.0f km/h", gd.wx_wind_kmh);
        lv_label_set_text(app.w_wx_wind, tmp);
    }
    if (app.w_wx_hum) {
        char tmp[48];
        snprintf(tmp, sizeof(tmp), LV_SYMBOL_DOWNLOAD "  Humidity %d%%", gd.humidity);
        lv_label_set_text(app.w_wx_hum, tmp);
    }

    /* AQI */
    if (app.w_wx_aqarc) {
        lv_arc_set_value(app.w_wx_aqarc, gd.aqi);
        lv_obj_set_style_arc_color(app.w_wx_aqarc, aqi_color(gd.aqi), LV_PART_INDICATOR);
    }
    if (app.w_wx_aqval)
        lv_label_set_text_fmt(app.w_wx_aqval, "%d", gd.aqi);
    if (app.w_wx_aq_cat) {
        lv_label_set_text(app.w_wx_aq_cat, aqi_category(gd.aqi));
        lv_obj_set_style_text_color(app.w_wx_aq_cat, aqi_color(gd.aqi), 0);
    }
    if (app.w_wx_aq_desc)
        lv_label_set_text(app.w_wx_aq_desc, aqi_description(gd.aqi));
    if (app.w_wx_aqpm) {
        char tmp[40];
        snprintf(tmp, sizeof(tmp), "PM2.5 %d  PM10 %d", gd.wx_pm25, gd.wx_pm10);
        lv_label_set_text(app.w_wx_aqpm, tmp);
    }

    /* main screen bottom row */
    if (app.w_main_wx_tmp)
        lv_label_set_text_fmt(app.w_main_wx_tmp, "%d\xC2\xB0""C", gd.wx_c);
    if (app.w_main_wx_hum)
        lv_label_set_text_fmt(app.w_main_wx_hum, "%d%%", gd.humidity);
    if (app.w_main_wx_aqi)
        lv_label_set_text_fmt(app.w_main_wx_aqi, "%d", gd.aqi);
    if (app.w_main_wx_aqi_cat) {
        lv_label_set_text(app.w_main_wx_aqi_cat, aqi_category(gd.aqi));
        lv_obj_set_style_text_color(app.w_main_wx_aqi_cat, aqi_color(gd.aqi), 0);
    }

    /* 7-day forecast */
    time_t now = time(NULL);
    for (int i = 0; i < 7; i++) {
        if (app.w_fc_day[i] && wifi_manager_time_synced()) {
            time_t day_t = now + (time_t)i * 86400;
            struct tm ti;
            localtime_r(&day_t, &ti);
            if (i == 0) {
                lv_label_set_text(app.w_fc_day[i], "Today");
            } else {
                char day_name[12];
                strftime(day_name, sizeof(day_name), "%a", &ti);
                lv_label_set_text(app.w_fc_day[i], day_name);
            }
        }
        if (app.w_fc_icon[i])
            weather_icon_update(app.w_fc_icon[i], gd.fc_code[i]);
        if (app.w_fc_hi[i])
            lv_label_set_text_fmt(app.w_fc_hi[i], "%d\xC2\xB0""C", gd.fc_hi[i]);
        if (app.w_fc_lo[i])
            lv_label_set_text_fmt(app.w_fc_lo[i], "%d\xC2\xB0""C", gd.fc_lo[i]);
        if (app.w_fc_desc[i])
            lv_label_set_text(app.w_fc_desc[i], wmo_desc_short(gd.fc_code[i]));
    }

    lvgl_release();
}

/* ── Task ──────────────────────────────────────────────────────── */

static void weather_task(void *arg)
{
    /* Wait for WiFi then NTP — NTP sync implies the initial DHCP/association
     * burst has fully settled, giving SDIO a clean state for HTTPS.
     * Cap at 30s in case NTP never syncs (no internet yet). */
    while (!wifi_manager_connected())
        vTaskDelay(pdMS_TO_TICKS(2000));

    for (int i = 0; i < 15 && !wifi_manager_time_synced(); i++)
        vTaskDelay(pdMS_TO_TICKS(2000));

    /* 5s extra: let NTP traffic fully drain off the SDIO bus */
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (1) {
        /* Wait for WiFi if transport had a failure between cycles */
        for (int w = 0; !wifi_manager_connected() && w < 30; w++)
            vTaskDelay(pdMS_TO_TICKS(2000));

        bool wx_ok  = weather_fetch();
        /* 15s gap: let TCP teardown and DNS cache flush before AQI opens
         * a new TLS session to a different hostname over SDIO */
        vTaskDelay(pdMS_TO_TICKS(15000));
        bool aqi_ok = aqi_fetch();
        if (wx_ok || aqi_ok)
            update_ui();
        /* Sleep in 30s chunks — exits early if WiFi drops then reconnects
         * so the next fetch cycle starts within 30s of reconnection.      */
        int sleep_ms = (wx_ok || aqi_ok) ? FETCH_INTERVAL_MS : 2 * 60 * 1000;
        bool was_connected = wifi_manager_connected();
        while (sleep_ms > 0) {
            int chunk = sleep_ms > 30000 ? 30000 : sleep_ms;
            vTaskDelay(pdMS_TO_TICKS(chunk));
            sleep_ms -= chunk;
            bool now_connected = wifi_manager_connected();
            /* If WiFi just came back after a blip, abort the remainder */
            if (!was_connected && now_connected && sleep_ms > 30000)
                break;
            was_connected = now_connected;
        }
    }
}

void weather_service_start(void)
{
    xTaskCreate(weather_task, "wx_task", 16384, NULL, 3, NULL);
}
