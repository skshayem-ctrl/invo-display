#include "invo_weather.h"
#include "invo_screens.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

/* Open-Meteo: free, no API key — Bengaluru coords */
#define WX_URL \
    "https://api.open-meteo.com/v1/forecast" \
    "?latitude=12.9716&longitude=77.5946" \
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature" \
    ",weather_code,wind_speed_10m" \
    "&daily=weather_code,temperature_2m_max,temperature_2m_min" \
    "&timezone=Asia%2FKolkata&forecast_days=7"

#define AQ_URL \
    "https://air-quality-api.open-meteo.com/v1/air-quality" \
    "?latitude=12.9716&longitude=77.5946" \
    "&current=pm10,pm2_5,european_aqi"

/* ── Data ─────────────────────────────────────────────── */
typedef struct {
    float temp, feels_like, wind_kmh;
    int   humidity, wmo_code;
    float hi[7], lo[7];
    int   fc_code[7];
    int   aqi;
    float pm25, pm10;
    int   valid;
} wx_t;

typedef enum { WX_IDLE, WX_RUNNING, WX_DONE } wx_state_t;
static volatile wx_state_t wx_state = WX_IDLE;
static wx_t wx;

/* ── Widget pointers ──────────────────────────────────── */
static lv_obj_t * w1_temp, * w1_desc, * w1_row[3];
static lv_obj_t * w2_hum_val, * w2_hum_sub, * w2_wnd_val, * w2_wnd_sub;
static lv_obj_t * w3_arc, * w3_num, * w3_lbl, * w3_pm;
static lv_obj_t * wf_day[7], * wf_hi[7], * wf_lo[7], * wf_icon[7];
static lv_obj_t * w_status;
static lv_obj_t * wx_hdr_time;
static lv_obj_t * wx_hdr_date;

/* ── Home screen summary labels (registered from invo_home.c) ── */
static lv_obj_t * h_temp_lbl;
static lv_obj_t * h_city_lbl;
static lv_obj_t * h_aqi_val;
static lv_obj_t * h_aqi_lbl;

void invo_weather_register_home(lv_obj_t * temp, lv_obj_t * city,
                                 lv_obj_t * aqi_val, lv_obj_t * aqi_lbl) {
    h_temp_lbl = temp;
    h_city_lbl = city;
    h_aqi_val  = aqi_val;
    h_aqi_lbl  = aqi_lbl;
}

/* ── Minimal JSON helpers ─────────────────────────────── */
static int jf(const char * j, const char * key, float * v) {
    char k[80]; snprintf(k, sizeof(k), "\"%s\":", key);
    const char * p = strstr(j, k);
    if (!p) return 0;
    p += strlen(k);
    while (*p == ' ') p++;
    char * e; *v = strtof(p, &e);
    return (e != p);
}

static int ji(const char * j, const char * key, int * v) {
    float f; if (!jf(j, key, &f)) return 0;
    *v = (int)roundf(f); return 1;
}

/* Look specifically for array form "key":[ to avoid hitting scalar fields */
static void jfa(const char * j, const char * key, float * arr, int n) {
    char k1[80], k2[80];
    snprintf(k1, sizeof(k1), "\"%s\":[", key);
    snprintf(k2, sizeof(k2), "\"%s\": [", key);
    const char * p = strstr(j, k1);
    if (!p) p = strstr(j, k2);
    if (!p) return;
    p = strchr(p, '[') + 1;
    for (int i = 0; i < n; i++) {
        while (*p == ' ' || *p == '\n' || *p == '\r') p++;
        if (*p == ']') break;
        char * e; arr[i] = strtof(p, &e);
        if (e == p) break;
        p = e;
        while (*p == ',' || *p == ' ') p++;
    }
}

static void jia(const char * j, const char * key, int * arr, int n) {
    float tmp[7] = {0}; jfa(j, key, tmp, n);
    for (int i = 0; i < n; i++) arr[i] = (int)roundf(tmp[i]);
}

/* ── Label helpers ────────────────────────────────────── */
static const char * wmo_desc(int c) {
    if (c == 0)  return "Clear Sky";
    if (c <= 2)  return "Mainly Clear";
    if (c == 3)  return "Overcast";
    if (c <= 48) return "Foggy";
    if (c <= 55) return "Drizzle";
    if (c <= 67) return "Rainy";
    if (c <= 77) return "Snowy";
    if (c <= 82) return "Showers";
    if (c <= 99) return "Thunderstorm";
    return "--";
}

static const char * wmo_icon(int c) {
    if (c == 0)  return WI_SUN;
    if (c <= 2)  return WI_CLOUD_SUN;
    if (c == 3)  return WI_CLOUD;
    if (c <= 48) return WI_SMOG;
    if (c <= 67) return WI_RAIN;
    if (c <= 77) return WI_SNOW;
    if (c <= 82) return WI_RAIN;
    if (c <= 99) return WI_BOLT;
    return WI_CLOUD;
}

static lv_color_t wmo_color(int c) {
    if (c == 0)  return lv_color_hex(0xFFCC00);  /* sunny yellow */
    if (c <= 2)  return lv_color_hex(0xFFAA44);  /* partly cloudy orange */
    if (c == 3)  return lv_color_hex(0x888888);  /* overcast gray */
    if (c <= 48) return lv_color_hex(0xAAAAAA);  /* fog light gray */
    if (c <= 55) return lv_color_hex(0x88AAFF);  /* drizzle light blue */
    if (c <= 77) return lv_color_hex(0x4488FF);  /* rain/snow blue */
    if (c <= 82) return lv_color_hex(0x4488FF);  /* showers blue */
    if (c <= 99) return lv_color_hex(0xFFDD00);  /* storm yellow */
    return lv_color_hex(0x888888);
}

static const char * aqi_cat(int a) {
    if (a < 20) return "Good";
    if (a < 40) return "Fair";
    if (a < 60) return "Moderate";
    if (a < 80) return "Poor";
    return "Very Poor";
}

static const char * hum_cat(int h) {
    if (h < 30) return "Dry";
    if (h < 60) return "Comfortable";
    if (h < 80) return "Humid";
    return "Very Humid";
}

static const char * wind_cat(float w) {
    if (w < 10) return "Calm";
    if (w < 30) return "Moderate";
    if (w < 60) return "Strong";
    return "Very Strong";
}

/* ── Fetch (runs in background thread) ───────────────── */
static char wx_buf[8192];
static char aq_buf[1024];

static void do_fetch(void) {
    wx.valid = 0;
    wx.aqi   = -1;

    FILE * fp = popen("curl -sf --max-time 12 '" WX_URL "' 2>/dev/null", "r");
    wx_buf[0] = '\0';
    if (fp) { fread(wx_buf, 1, sizeof(wx_buf) - 1, fp); pclose(fp); }

    if (wx_buf[0]) {
        /* Parse current and daily sections separately to avoid key ambiguity */
        const char * cur   = strstr(wx_buf, "\"current\":");
        const char * daily = strstr(wx_buf, "\"daily\":");
        if (cur) {
            jf(cur, "temperature_2m",       &wx.temp);
            jf(cur, "apparent_temperature", &wx.feels_like);
            jf(cur, "wind_speed_10m",       &wx.wind_kmh);
            ji(cur, "relative_humidity_2m", &wx.humidity);
            ji(cur, "weather_code",         &wx.wmo_code);
            wx.valid = 1;
        }
        if (daily) {
            jfa(daily, "temperature_2m_max", wx.hi, 7);
            jfa(daily, "temperature_2m_min", wx.lo, 7);
            jia(daily, "weather_code",       wx.fc_code, 7);
        }
    }

    FILE * fp2 = popen("curl -sf --max-time 12 '" AQ_URL "' 2>/dev/null", "r");
    aq_buf[0] = '\0';
    if (fp2) { fread(aq_buf, 1, sizeof(aq_buf) - 1, fp2); pclose(fp2); }

    if (aq_buf[0]) {
        const char * cur2 = strstr(aq_buf, "\"current\":");
        if (cur2) {
            ji(cur2, "european_aqi", &wx.aqi);
            jf(cur2, "pm2_5",        &wx.pm25);
            jf(cur2, "pm10",         &wx.pm10);
        }
    }
}

static void * wx_thread(void * arg) {
    (void)arg;
    do_fetch();
    wx_state = WX_DONE;
    return NULL;
}

/* ── Apply fetched data to widgets (LVGL main thread) ── */
static void apply_weather_data(void) {
    char buf[64];

    if (!wx.valid) {
        lv_label_set_text(w_status, LV_SYMBOL_WARNING " Fetch failed — try again");
        lv_label_set_text(w1_temp, "--");
        return;
    }

    /* Card 1: Weather Now */
    lv_snprintf(buf, sizeof(buf), "%.0f\xc2\xb0""C", wx.temp);
    lv_label_set_text(w1_temp, buf);

    /* Push temp to home screen summary card */
    if (h_temp_lbl) lv_label_set_text(h_temp_lbl, buf);
    if (h_city_lbl) lv_label_set_text(h_city_lbl, "Bengaluru");
    lv_label_set_text(w1_desc, wmo_desc(wx.wmo_code));
    lv_snprintf(buf, sizeof(buf), "Feels %.0f\xc2\xb0""C", wx.feels_like);
    lv_label_set_text(w1_row[0], buf);
    lv_snprintf(buf, sizeof(buf), "Wind %.0f km/h", wx.wind_kmh);
    lv_label_set_text(w1_row[1], buf);
    lv_snprintf(buf, sizeof(buf), "Hum. %d%%", wx.humidity);
    lv_label_set_text(w1_row[2], buf);

    /* Card 2: Humidity & Wind */
    lv_snprintf(buf, sizeof(buf), "%d%%", wx.humidity);
    lv_label_set_text(w2_hum_val, buf);
    lv_label_set_text(w2_hum_sub, hum_cat(wx.humidity));
    lv_snprintf(buf, sizeof(buf), "%.0f km/h", wx.wind_kmh);
    lv_label_set_text(w2_wnd_val, buf);
    lv_label_set_text(w2_wnd_sub, wind_cat(wx.wind_kmh));

    /* Card 3: Air Quality */
    if (wx.aqi >= 0) {
        lv_arc_set_value(w3_arc, wx.aqi > 100 ? 100 : wx.aqi);
        lv_snprintf(buf, sizeof(buf), "%d", wx.aqi);
        lv_label_set_text(w3_num, buf);
        lv_label_set_text(w3_lbl, aqi_cat(wx.aqi));
        /* Push AQI to home screen summary card */
        if (h_aqi_val) { lv_snprintf(buf, sizeof(buf), "%d AQI", wx.aqi); lv_label_set_text(h_aqi_val, buf); }
        if (h_aqi_lbl) lv_label_set_text(h_aqi_lbl, aqi_cat(wx.aqi));
        lv_snprintf(buf, sizeof(buf), "PM2.5:%.0f  PM10:%.0f", wx.pm25, wx.pm10);
        lv_label_set_text(w3_pm, buf);
        lv_color_t ac = wx.aqi < 40 ? C_GREEN :
                        wx.aqi < 60 ? C_ORANGE : lv_color_hex(0xff4444);
        lv_obj_set_style_arc_color(w3_arc, ac, LV_PART_INDICATOR);
        lv_obj_set_style_text_color(w3_num, ac, 0);
        lv_obj_set_style_text_color(w3_lbl, ac, 0);
    }

    /* Forecast */
    time_t now = time(NULL);
    struct tm * t = localtime(&now);
    const char * sday[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    for (int i = 0; i < 7; i++) {
        lv_label_set_text(wf_day[i], i == 0 ? "Today" : sday[(t->tm_wday + i) % 7]);
        lv_label_set_text(wf_icon[i], wmo_icon(wx.fc_code[i]));
        lv_obj_set_style_text_color(wf_icon[i], wmo_color(wx.fc_code[i]), 0);
        lv_snprintf(buf, sizeof(buf), "%.0f\xc2\xb0", wx.hi[i]);
        lv_label_set_text(wf_hi[i], buf);
        lv_snprintf(buf, sizeof(buf), "%.0f\xc2\xb0", wx.lo[i]);
        lv_label_set_text(wf_lo[i], buf);
    }

    /* Status: last updated time */
    lv_snprintf(buf, sizeof(buf), "Updated %02d:%02d", t->tm_hour, t->tm_min);
    lv_label_set_text(w_status, buf);
}

/* ── Poll timer: check thread state every 500 ms ─────── */
static void wx_poll_cb(lv_timer_t * t) {
    if (wx_state != WX_DONE) return;
    lv_timer_delete(t);
    wx_state = WX_IDLE;
    apply_weather_data();
}

/* ── Back button callback ─────────────────────────────── */
static void weather_back_cb(lv_event_t * e) { LV_UNUSED(e); nav_to_home(); }

/* ── Screen builder ───────────────────────────────────── */
static lv_obj_t * make_card(lv_obj_t * scr, int ox, int oy, int w, int h) {
    lv_obj_t * c = lv_obj_create(scr);
    lv_obj_set_size(c, w, h);
    lv_obj_align(c, LV_ALIGN_CENTER, ox, oy);
    lv_obj_set_style_bg_color(c, lv_color_hex(0x161616), 0);
    lv_obj_set_style_border_color(c, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, 14, 0);
    lv_obj_set_style_pad_all(c, 10, 0);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

static void build_weather_screen_ui(lv_obj_t * scr) {
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Header: WiFi icon + time + date */
    {
        time_t now = time(NULL); struct tm * t = localtime(&now);
        char tb[16], db[24];
        strftime(tb, sizeof(tb), "%I:%M %p", t);
        strftime(db, sizeof(db), "%b %d %A", t);

        lv_obj_t * wi = lv_label_create(scr);
        lv_label_set_text(wi, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(wi, C_GREEN, 0);
        lv_obj_set_style_text_font(wi, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_opa(wi, LV_OPA_70, 0);
        lv_obj_align(wi, LV_ALIGN_TOP_MID, 0, 52);

        wx_hdr_time = lv_label_create(scr);
        lv_label_set_text(wx_hdr_time, tb);
        lv_obj_set_style_text_color(wx_hdr_time, C_WHITE, 0);
        lv_obj_set_style_text_font(wx_hdr_time, &lv_font_montserrat_28, 0);
        lv_obj_align(wx_hdr_time, LV_ALIGN_TOP_MID, 0, 76);

        wx_hdr_date = lv_label_create(scr);
        lv_label_set_text(wx_hdr_date, db);
        lv_obj_set_style_text_color(wx_hdr_date, C_GRAY, 0);
        lv_obj_set_style_text_font(wx_hdr_date, &lv_font_montserrat_14, 0);
        lv_obj_align(wx_hdr_date, LV_ALIGN_TOP_MID, 0, 113);
    }

    int cw = 175, ch = 285, coy = -80;

    /* ── Card 1: Weather Now ── */
    lv_obj_t * c1 = make_card(scr, -187, coy, cw, ch);

    lv_obj_t * c1h = lv_label_create(c1);
    lv_label_set_text(c1h, "WEATHER NOW");
    lv_obj_set_style_text_color(c1h, C_GRAY, 0);
    lv_obj_set_style_text_font(c1h, &lv_font_montserrat_12, 0);
    lv_obj_align(c1h, LV_ALIGN_TOP_MID, 0, 0);

    w1_temp = lv_label_create(c1);
    lv_label_set_text(w1_temp, "--");
    lv_obj_set_style_text_color(w1_temp, C_WHITE, 0);
    lv_obj_set_style_text_font(w1_temp, &lv_font_montserrat_32, 0);
    lv_obj_align(w1_temp, LV_ALIGN_TOP_MID, 0, 20);

    w1_desc = lv_label_create(c1);
    lv_label_set_text(w1_desc, "...");
    lv_obj_set_style_text_color(w1_desc, C_GRAY, 0);
    lv_obj_set_style_text_font(w1_desc, &lv_font_montserrat_14, 0);
    lv_obj_align(w1_desc, LV_ALIGN_TOP_MID, 0, 70);

    const char * init_rows[] = { "Feels --\xc2\xb0""C", "Wind -- km/h", "Hum. --%"};
    for (int i = 0; i < 3; i++) {
        w1_row[i] = lv_label_create(c1);
        lv_label_set_text(w1_row[i], init_rows[i]);
        lv_obj_set_style_text_color(w1_row[i], lv_color_hex(0x555555), 0);
        lv_obj_set_style_text_font(w1_row[i], &lv_font_montserrat_12, 0);
        lv_obj_align(w1_row[i], LV_ALIGN_TOP_LEFT, 0, 98 + i * 22);
    }

    lv_obj_t * c1c = lv_label_create(c1);
    lv_label_set_text(c1c, LV_SYMBOL_GPS "  Bengaluru");
    lv_obj_set_style_text_color(c1c, C_BLUE, 0);
    lv_obj_set_style_text_font(c1c, &lv_font_montserrat_12, 0);
    lv_obj_align(c1c, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ── Card 2: Humidity & Wind ── */
    lv_obj_t * c2 = make_card(scr, 0, coy, cw, ch);

    lv_obj_t * c2h = lv_label_create(c2);
    lv_label_set_text(c2h, "HUMIDITY & WIND");
    lv_obj_set_style_text_color(c2h, C_GRAY, 0);
    lv_obj_set_style_text_font(c2h, &lv_font_montserrat_12, 0);
    lv_obj_align(c2h, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * c2hl = lv_label_create(c2);
    lv_label_set_text(c2hl, "Humidity");
    lv_obj_set_style_text_color(c2hl, C_GRAY, 0);
    lv_obj_set_style_text_font(c2hl, &lv_font_montserrat_12, 0);
    lv_obj_align(c2hl, LV_ALIGN_TOP_MID, 0, 20);

    w2_hum_val = lv_label_create(c2);
    lv_label_set_text(w2_hum_val, "--%");
    lv_obj_set_style_text_color(w2_hum_val, C_WHITE, 0);
    lv_obj_set_style_text_font(w2_hum_val, &lv_font_montserrat_30, 0);
    lv_obj_align(w2_hum_val, LV_ALIGN_TOP_MID, 0, 38);

    w2_hum_sub = lv_label_create(c2);
    lv_label_set_text(w2_hum_sub, "--");
    lv_obj_set_style_text_color(w2_hum_sub, C_GREEN, 0);
    lv_obj_set_style_text_font(w2_hum_sub, &lv_font_montserrat_12, 0);
    lv_obj_align(w2_hum_sub, LV_ALIGN_TOP_MID, 0, 88);

    lv_obj_t * c2wl = lv_label_create(c2);
    lv_label_set_text(c2wl, "Wind");
    lv_obj_set_style_text_color(c2wl, C_GRAY, 0);
    lv_obj_set_style_text_font(c2wl, &lv_font_montserrat_12, 0);
    lv_obj_align(c2wl, LV_ALIGN_TOP_MID, 0, 118);

    w2_wnd_val = lv_label_create(c2);
    lv_label_set_text(w2_wnd_val, "-- km/h");
    lv_obj_set_style_text_color(w2_wnd_val, C_WHITE, 0);
    lv_obj_set_style_text_font(w2_wnd_val, &lv_font_montserrat_22, 0);
    lv_obj_align(w2_wnd_val, LV_ALIGN_TOP_MID, 0, 136);

    w2_wnd_sub = lv_label_create(c2);
    lv_label_set_text(w2_wnd_sub, "--");
    lv_obj_set_style_text_color(w2_wnd_sub, C_GREEN, 0);
    lv_obj_set_style_text_font(w2_wnd_sub, &lv_font_montserrat_12, 0);
    lv_obj_align(w2_wnd_sub, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ── Card 3: Air Quality ── */
    lv_obj_t * c3 = make_card(scr, 187, coy, cw, ch);

    lv_obj_t * c3h = lv_label_create(c3);
    lv_label_set_text(c3h, LV_SYMBOL_OK "  AIR QUALITY");
    lv_obj_set_style_text_color(c3h, C_GREEN, 0);
    lv_obj_set_style_text_font(c3h, &lv_font_montserrat_12, 0);
    lv_obj_align(c3h, LV_ALIGN_TOP_MID, 0, 0);

    w3_arc = lv_arc_create(c3);
    lv_obj_set_size(w3_arc, 95, 95);
    lv_arc_set_rotation(w3_arc, 270);
    lv_arc_set_bg_angles(w3_arc, 0, 360);
    lv_arc_set_value(w3_arc, 0);
    lv_arc_set_range(w3_arc, 0, 100);
    lv_obj_set_style_arc_color(w3_arc, C_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(w3_arc, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(w3_arc, lv_color_hex(0x1a2a1a), LV_PART_MAIN);
    lv_obj_set_style_arc_width(w3_arc, 7, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(w3_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(w3_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(w3_arc, LV_ALIGN_TOP_MID, 0, 20);

    w3_num = lv_label_create(c3);
    lv_label_set_text(w3_num, "--");
    lv_obj_set_style_text_color(w3_num, C_WHITE, 0);
    lv_obj_set_style_text_font(w3_num, &lv_font_montserrat_22, 0);
    lv_obj_align(w3_num, LV_ALIGN_TOP_MID, 0, 60);

    w3_lbl = lv_label_create(c3);
    lv_label_set_text(w3_lbl, "--");
    lv_obj_set_style_text_color(w3_lbl, C_GREEN, 0);
    lv_obj_set_style_text_font(w3_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(w3_lbl, LV_ALIGN_TOP_MID, 0, 130);

    lv_obj_t * c3d = lv_label_create(c3);
    lv_label_set_text(c3d, "European AQI\nindex (0-100)");
    lv_obj_set_style_text_color(c3d, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(c3d, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(c3d, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(c3d, LV_ALIGN_TOP_MID, 0, 152);

    w3_pm = lv_label_create(c3);
    lv_label_set_text(w3_pm, "PM2.5:--  PM10:--");
    lv_obj_set_style_text_color(w3_pm, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(w3_pm, &lv_font_montserrat_12, 0);
    lv_obj_align(w3_pm, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ── 7-day forecast strip ── */
    lv_obj_t * fch = lv_label_create(scr);
    lv_label_set_text(fch, "7-DAY FORECAST");
    lv_obj_set_style_text_color(fch, lv_color_hex(0x444444), 0);
    lv_obj_set_style_text_font(fch, &lv_font_montserrat_12, 0);
    lv_obj_align(fch, LV_ALIGN_CENTER, 0, 105);

    int fc_ox[] = { -258, -172, -86, 0, 86, 172, 258 };
    for (int i = 0; i < 7; i++) {
        lv_obj_t * fc = lv_obj_create(scr);
        lv_obj_set_size(fc, 80, 92);
        lv_obj_align(fc, LV_ALIGN_CENTER, fc_ox[i], 160);
        lv_obj_set_style_bg_color(fc, lv_color_hex(0x111111), 0);
        lv_obj_set_style_border_opa(fc, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(fc, 10, 0);
        lv_obj_set_style_pad_all(fc, 4, 0);
        lv_obj_remove_flag(fc, LV_OBJ_FLAG_SCROLLABLE);

        wf_day[i] = lv_label_create(fc);
        lv_label_set_text(wf_day[i], "--");
        lv_obj_set_style_text_color(wf_day[i], i == 0 ? C_WHITE : C_GRAY, 0);
        lv_obj_set_style_text_font(wf_day[i], &lv_font_montserrat_12, 0);
        lv_obj_align(wf_day[i], LV_ALIGN_TOP_MID, 0, 0);

        wf_icon[i] = lv_label_create(fc);
        lv_label_set_text(wf_icon[i], WI_CLOUD);
        lv_obj_set_style_text_color(wf_icon[i], lv_color_hex(0x555555), 0);
        lv_obj_set_style_text_font(wf_icon[i], &lv_font_weather_20, 0);
        lv_obj_align(wf_icon[i], LV_ALIGN_TOP_MID, 0, 17);

        wf_hi[i] = lv_label_create(fc);
        lv_label_set_text(wf_hi[i], "--\xc2\xb0");
        lv_obj_set_style_text_color(wf_hi[i], i == 0 ? C_ORANGE : C_WHITE, 0);
        lv_obj_set_style_text_font(wf_hi[i], &lv_font_montserrat_14, 0);
        lv_obj_align(wf_hi[i], LV_ALIGN_TOP_MID, 0, 42);

        wf_lo[i] = lv_label_create(fc);
        lv_label_set_text(wf_lo[i], "--\xc2\xb0");
        lv_obj_set_style_text_color(wf_lo[i], C_GRAY, 0);
        lv_obj_set_style_text_font(wf_lo[i], &lv_font_montserrat_12, 0);
        lv_obj_align(wf_lo[i], LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    /* ── Back button ── */
    lv_obj_t * btn = lv_button_create(scr);
    lv_obj_set_size(btn, 72, 72);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, -200, -90);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_radius(btn, 36, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_add_event_cb(btn, weather_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * bl = lv_label_create(btn);
    lv_label_set_text(bl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(bl, C_WHITE, 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_24, 0);
    lv_obj_center(bl);

    /* ── Logo ── */
    lv_obj_t * logo_cont = lv_obj_create(scr);
    lv_obj_set_size(logo_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(logo_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(logo_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(logo_cont, 0, 0);
    lv_obj_set_style_pad_column(logo_cont, 0, 0);
    lv_obj_remove_flag(logo_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(logo_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(logo_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(logo_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(logo_cont, LV_ALIGN_BOTTOM_MID, 0, -22);

    lv_obj_t * ll1 = lv_label_create(logo_cont);
    lv_label_set_text(ll1, "INV");
    lv_obj_set_style_text_color(ll1, C_WHITE, 0);
    lv_obj_set_style_text_font(ll1, &lv_font_montserrat_26, 0);

    lv_obj_t * ll2 = lv_label_create(logo_cont);
    lv_label_set_text(ll2, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(ll2, C_GREEN, 0);
    lv_obj_set_style_text_font(ll2, &lv_font_montserrat_26, 0);

    /* ── Status line (last-updated / error) ── */
    w_status = lv_label_create(scr);
    lv_label_set_text(w_status, "tap to refresh");
    lv_obj_set_style_text_color(w_status, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(w_status, &lv_font_montserrat_12, 0);
    lv_obj_align(w_status, LV_ALIGN_CENTER, 0, 216);
}

/* ── Public API ───────────────────────────────────────── */
void invo_weather_init(lv_obj_t * scr) {
    build_weather_screen_ui(scr);
}

void invo_weather_refresh(void) {
    if (wx_state == WX_RUNNING) return;
    lv_label_set_text(w1_temp, "--");
    lv_label_set_text(w_status, LV_SYMBOL_REFRESH " Fetching...");
    wx_state = WX_RUNNING;

    pthread_t th;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&th, &attr, wx_thread, NULL);
    pthread_attr_destroy(&attr);

    lv_timer_create(wx_poll_cb, 500, NULL);
}

void invo_weather_sync_clock(void) {
    if (!wx_hdr_time || !wx_hdr_date) return;
    time_t now = time(NULL);
    struct tm * tm_info = localtime(&now);
    char tb[16], db[24];
    strftime(tb, sizeof(tb), "%I:%M %p", tm_info);
    strftime(db, sizeof(db), "%b %d %A", tm_info);
    lv_label_set_text(wx_hdr_time, tb);
    lv_label_set_text(wx_hdr_date, db);
}
