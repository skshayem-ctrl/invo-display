#pragma once
#include "lvgl.h"

/* colour palette */
#define C_BG    lv_color_hex(0x080C18)
#define C_CARD  lv_color_hex(0x0E1422)
#define C_LINE  lv_color_hex(0x1E2A3A)
#define C_BLUE  lv_color_hex(0x1E90FF)
#define C_GREEN lv_color_hex(0x39D353)
#define C_AMBER lv_color_hex(0xFFA726)
#define C_RED   lv_color_hex(0xFF3B3B)
#define C_WHITE lv_color_hex(0xFFFFFF)
#define C_GRAY  lv_color_hex(0x8892A0)
#define C_DGRAY lv_color_hex(0x2A3348)

/* simulated telemetry — swap fields with real sensor reads */
typedef struct {
    float solar_kw, load_kw;
    int   batt_pct, backup_h, backup_m;
    float chg_kw, batt_cap, batt_usable;
    int   batt_health, batt_cycles;
    float batt_life, batt_temp;
    int   chg_h, chg_m;
    int   wx_c, wx_feels_c, humidity, aqi;
    float wx_wind_kmh;
    int   wx_code;
    int   wx_pm25, wx_pm10;
    int   fc_code[7], fc_hi[7], fc_lo[7];
    int   voltage;
    float current, today_solar_kwh, today_load_kwh, month_kwh;
} gd_t;
extern gd_t gd;

/* app-wide widget handles */
typedef struct {
    lv_obj_t *scr_main, *scr_batt, *scr_solar, *scr_load, *scr_wx, *scr_sleep, *scr_wifi;
    lv_obj_t *scr_settings, *scr_history, *scr_alerts;
    lv_obj_t *w_wifi;                                       /* main screen */
    lv_obj_t *w_wifi_bd, *w_wifi_sd, *w_wifi_ld, *w_wifi_wxd; /* detail screens */
    lv_obj_t *w_time, *w_date;
    lv_obj_t *w_sleep_time, *w_sleep_date;
    lv_obj_t *w_batt_arc, *w_batt_pct, *w_batt_backup;
    lv_obj_t *w_solar_val, *w_load_val;
    lv_obj_t *w_warn_ring, *w_warn_dlg;
    /* battery detail */
    lv_obj_t *w_bd_pct, *w_bd_chg, *w_bd_bkp, *w_bd_full, *w_bd_tmp;
    /* solar detail */
    lv_obj_t *w_sd_kw, *w_sd_kwh, *w_sd_volt, *w_sd_cur;
    lv_obj_t *w_sd_chart;
    lv_chart_series_t *w_sd_ser;
    /* home load detail */
    lv_obj_t *w_ld_kw, *w_ld_kwh;
    lv_obj_t *w_ld_chart;
    lv_chart_series_t *w_ld_ser;
    /* weather detail */
    lv_obj_t *w_wx_tmp, *w_wx_hum, *w_wx_aqarc, *w_wx_aqval;
    lv_obj_t *w_wx_cond, *w_wx_feels, *w_wx_wind;
    lv_obj_t *w_wx_aq_cat, *w_wx_aq_desc, *w_wx_aqpm;
    /* 7-day forecast tiles */
    lv_obj_t *w_fc_day[7], *w_fc_icon[7], *w_fc_hi[7], *w_fc_lo[7], *w_fc_desc[7];
    /* current weather icon (canvas) on weather detail screen */
    lv_obj_t *w_wx_icon;
    /* main screen bottom row */
    lv_obj_t *w_main_wx_tmp, *w_main_wx_hum, *w_main_wx_aqi, *w_main_wx_aqi_cat;
    int clk_h, clk_m, clk_s;
    int startup_phase;
} app_t;
extern app_t app;

/* shared widget helpers */
void      style_screen(lv_obj_t *scr);
lv_obj_t *mk_lbl(lv_obj_t *par, const char *txt, const lv_font_t *fnt,
                 lv_color_t col, lv_align_t align, int xo, int yo);
void      add_hdiv(lv_obj_t *par, int yoff, int w);
lv_obj_t *mk_card(lv_obj_t *par, int x, int y, int w, int h,
                  const char *icon, lv_color_t icon_col,
                  const char *title, const char *value, const char *sub);
lv_obj_t *mk_row(lv_obj_t *par);
void      lv_lbl_setf(lv_obj_t *l, const char *fmt, double v);
lv_obj_t *mk_cont(lv_obj_t *par, int w, int h);
lv_obj_t *add_logo(lv_obj_t *par, int yoff);
lv_obj_t *add_detail_header(lv_obj_t *par, const char *title);

/* navigation callbacks */
void go_load_cb(lv_event_t *e);
void go_settings_cb(lv_event_t *e);
void go_history_cb(lv_event_t *e);
void go_alerts_cb(lv_event_t *e);
void swipe_back_cb(lv_event_t *e);
void go_main_cb(lv_event_t *e);
void go_batt_cb(lv_event_t *e);
void go_solar_cb(lv_event_t *e);
void go_wx_cb(lv_event_t *e);
void go_sleep_cb(lv_event_t *e);
void go_wifi_cb(lv_event_t *e);
void wake_cb(lv_event_t *e);
void warn_ok_cb(lv_event_t *e);

/* LVGL timer callbacks */
void clock_tick_cb(lv_timer_t *t);
void data_tick_cb(lv_timer_t *t);

/* screen builders */
lv_obj_t *screen_main_create(void);
lv_obj_t *screen_battery_create(void);
lv_obj_t *screen_solar_create(void);
lv_obj_t *screen_weather_create(void);
lv_obj_t *screen_sleep_create(void);
lv_obj_t *screen_load_create(void);
lv_obj_t *screen_wifi_create(void);
lv_obj_t *screen_settings_create(void);
lv_obj_t *screen_history_create(void);
lv_obj_t *screen_alerts_create(void);

/* weather icon (canvas-drawn) */
#include "weather_icon.h"
