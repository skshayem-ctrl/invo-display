#include "ui_common.h"
#include "uart_input.h"
#include "wifi_manager.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "esp_random.h"

/* ── global state ──────────────────────────────────────────────── */

gd_t gd = {0};

app_t app;

/* ── screen style ──────────────────────────────────────────────── */

void style_screen(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

/* ── widget helpers ────────────────────────────────────────────── */

lv_obj_t *mk_lbl(lv_obj_t *par, const char *txt,
                 const lv_font_t *fnt, lv_color_t col,
                 lv_align_t align, int xo, int yo)
{
    lv_obj_t *l = lv_label_create(par);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, fnt, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_align(l, align, xo, yo);
    return l;
}

void add_hdiv(lv_obj_t *par, int yoff, int w)
{
    lv_obj_t *d = lv_obj_create(par);
    lv_obj_set_size(d, w, 1);
    lv_obj_align(d, LV_ALIGN_TOP_MID, 0, yoff);
    lv_obj_set_style_bg_color(d, C_LINE, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(d, 0, 0);
    lv_obj_set_style_pad_all(d, 0, 0);
    lv_obj_set_style_radius(d, 0, 0);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t *mk_card(lv_obj_t *par, int x, int y, int w, int h,
                  const char *icon, lv_color_t icon_col,
                  const char *title, const char *value, const char *sub)
{
    lv_obj_t *c = lv_obj_create(par);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, C_CARD, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(c, C_LINE, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, 10, 0);
    lv_obj_set_style_pad_all(c, 8, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ico = lv_label_create(c);
    lv_label_set_text(ico, icon);
    lv_obj_set_style_text_color(ico, icon_col, 0);
    lv_obj_set_style_text_font(ico, &lv_font_montserrat_14, 0);
    lv_obj_align(ico, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *ttl = lv_label_create(c);
    lv_label_set_text(ttl, title);
    lv_obj_set_style_text_color(ttl, C_GRAY, 0);
    lv_obj_set_style_text_font(ttl, &lv_font_montserrat_12, 0);
    lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, 20, 2);

    bool has_sub = sub && sub[0];
    lv_obj_t *val = lv_label_create(c);
    lv_label_set_text(val, value);
    lv_obj_set_style_text_color(val, C_WHITE, 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_16, 0);
    lv_obj_align(val, LV_ALIGN_BOTTOM_LEFT, 0, has_sub ? -16 : 0);

    if (has_sub)
    {
        lv_obj_t *s = lv_label_create(c);
        lv_label_set_text(s, sub);
        lv_obj_set_style_text_color(s, C_GRAY, 0);
        lv_obj_set_style_text_font(s, &lv_font_montserrat_12, 0);
        lv_obj_align(s, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }
    return c;
}

lv_obj_t *make_stat_card(lv_obj_t *scr, int w, int h, int ox, int oy,
                         const char *label, const char *value, const char *sub,
                         lv_color_t val_col, lv_color_t sub_col)
{
    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_set_size(card, w, h);
    lv_obj_align(card, LV_ALIGN_CENTER, ox, oy);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x161616), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val = lv_label_create(card);
    lv_label_set_text(val, value);
    lv_obj_set_style_text_color(val, val_col, 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
    lv_obj_align(val, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *sub_lbl = lv_label_create(card);
    lv_label_set_text(sub_lbl, sub);
    lv_obj_set_style_text_color(sub_lbl, sub_col, 0);
    lv_obj_set_style_text_font(sub_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(sub_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return val;
}

lv_obj_t *mk_row(lv_obj_t *par)
{
    lv_obj_t *r = lv_obj_create(par);
    lv_obj_set_size(r, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_set_style_pad_column(r, 5, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return r;
}

/* lv_snprintf has no %f — use stdlib snprintf for all float labels */
void lv_lbl_setf(lv_obj_t *l, const char *fmt, double v)
{
    if (!l)
        return;
    char buf[24];
    snprintf(buf, sizeof(buf), fmt, v);
    lv_label_set_text(l, buf);
}

lv_obj_t *mk_cont(lv_obj_t *par, int w, int h)
{
    lv_obj_t *c = lv_obj_create(par);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

/* ── INVO logo ─────────────────────────────────────────────────── */

lv_obj_t *add_logo(lv_obj_t *par, int yoff)
{
    lv_obj_t *c = mk_cont(par, 250, 42);
    lv_obj_align(c, LV_ALIGN_BOTTOM_MID, 0, yoff);

    lv_obj_t *row = mk_row(c);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *inv = lv_label_create(row);
    lv_label_set_text(inv, "INV");
    lv_obj_set_style_text_color(inv, C_WHITE, 0);
    lv_obj_set_style_text_font(inv, &lv_font_montserrat_20, 0);

    lv_obj_t *bolt = lv_label_create(row);
    lv_label_set_text(bolt, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(bolt, C_GREEN, 0);
    lv_obj_set_style_text_font(bolt, &lv_font_montserrat_24, 0);

    lv_obj_t *tag = lv_label_create(c);
    lv_label_set_text(tag, "Uninterrupted. Intelligent. Reliable.");
    lv_obj_set_style_text_color(tag, lv_color_hex(0x4A5A6A), 0);
    lv_obj_set_style_text_font(tag, &lv_font_montserrat_12, 0);
    lv_obj_align(tag, LV_ALIGN_BOTTOM_MID, 0, 0);

    return c;
}

/* ── detail screen header (WiFi centred at top, title below) ───── */

lv_obj_t *add_detail_header(lv_obj_t *par, const char *title)
{
    lv_obj_t *wifi = mk_lbl(par, LV_SYMBOL_WIFI, &lv_font_montserrat_14, C_GRAY,
                            LV_ALIGN_TOP_MID, 0, 28);
    mk_lbl(par, title, &lv_font_montserrat_16, C_GRAY,
           LV_ALIGN_TOP_MID, 0, 46);

    lv_obj_t *back = lv_btn_create(par);
    lv_obj_set_size(back, 72, 72);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, -200, -90);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_radius(back, 36, 0);
    lv_obj_set_style_border_color(back, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(back, 1, 0);
    lv_obj_add_event_cb(back, go_main_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *arr = lv_label_create(back);
    lv_label_set_text(arr, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(arr, C_WHITE, 0);
    lv_obj_set_style_text_font(arr, &lv_font_montserrat_24, 0);
    lv_obj_center(arr);
    return wifi;
}

/* ── navigation callbacks ──────────────────────────────────────── */

void go_main_cb(lv_event_t *e)
{
    lv_screen_load_anim(app.scr_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
}
void go_batt_cb(lv_event_t *e)
{
    if (app.scr_batt)
        lv_screen_load_anim(app.scr_batt, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}
void go_solar_cb(lv_event_t *e)
{
    if (app.scr_solar)
        lv_screen_load_anim(app.scr_solar, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}
void go_wx_cb(lv_event_t *e)
{
    if (app.scr_wx)
        lv_screen_load_anim(app.scr_wx, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}
void go_sleep_cb(lv_event_t *e)
{
    if (app.scr_sleep)
        lv_screen_load_anim(app.scr_sleep, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
}
void go_wifi_cb(lv_event_t *e)
{
    if (!app.scr_wifi)
        app.scr_wifi = screen_wifi_create();
    lv_screen_load_anim(app.scr_wifi, LV_SCR_LOAD_ANIM_MOVE_TOP, 250, 0, false);
}
void go_load_cb(lv_event_t *e)
{
    if (app.scr_load)
        lv_screen_load_anim(app.scr_load, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}
void go_settings_cb(lv_event_t *e)
{
    if (app.scr_settings)
        lv_screen_load_anim(app.scr_settings, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
}
void go_settings_general_cb(lv_event_t *e)
{
    if (app.scr_settings_general)
        lv_screen_load_anim(app.scr_settings_general, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
}
void go_batt_settings_cb(lv_event_t *e)
{
    if (app.scr_batt_settings)
        lv_screen_load_anim(app.scr_batt_settings, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
}
void go_history_cb(lv_event_t *e)
{
    if (app.scr_history)
        lv_screen_load_anim(app.scr_history, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
}
void go_alerts_cb(lv_event_t *e)
{
    if (app.scr_alerts)
        lv_screen_load_anim(app.scr_alerts, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
}
void swipe_back_cb(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_active();
    if (!indev)
        return;
    if (lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT)
        lv_screen_load_anim(app.scr_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 250, 0, false);
}
void wake_cb(lv_event_t *e)
{
    lv_screen_load_anim(app.scr_main, LV_SCR_LOAD_ANIM_FADE_IN, 400, 0, false);
}
void warn_ok_cb(lv_event_t *e)
{
    lv_obj_add_flag(app.w_warn_dlg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(app.w_warn_ring, LV_OBJ_FLAG_HIDDEN);
}

/* ── clock tick (1 s) ──────────────────────────────────────────── */

void clock_tick_cb(lv_timer_t *t)
{
    if (wifi_manager_time_synced())
    {
        struct tm ti;
        time_t now = time(NULL);
        localtime_r(&now, &ti);
        app.clk_h = ti.tm_hour;
        app.clk_m = ti.tm_min;
        app.clk_s = ti.tm_sec;

        if (app.w_date)
        {
            char date_buf[32];
            strftime(date_buf, sizeof(date_buf), "%b %d %A", &ti);
            lv_label_set_text(app.w_date, date_buf);
        }
    }
    else
    {
        app.clk_s++;
        if (app.clk_s >= 60)
        {
            app.clk_s = 0;
            app.clk_m++;
        }
        if (app.clk_m >= 60)
        {
            app.clk_m = 0;
            app.clk_h++;
        }
        if (app.clk_h >= 24)
        {
            app.clk_h = 0;
        }
    }

    if (app.w_time)
        lv_label_set_text_fmt(app.w_time, "%02d:%02d", app.clk_h, app.clk_m);

    /* Auto-sleep after 10 s of no touch — only from main screen */
    if (app.scr_sleep && lv_screen_active() == app.scr_main)
    {
        uint32_t idle_ms = lv_display_get_inactive_time(NULL);
        if (idle_ms >= 30000)
            lv_screen_load_anim(app.scr_sleep, LV_SCR_LOAD_ANIM_FADE_IN, 400, 0, false);
    }

    if (app.w_sleep_time)
        lv_label_set_text_fmt(app.w_sleep_time, "%02d:%02d", app.clk_h, app.clk_m);

    if (app.w_sleep_date && wifi_manager_time_synced())
    {
        struct tm ti;
        time_t now = time(NULL);
        localtime_r(&now, &ti);
        char date_buf[32];
        strftime(date_buf, sizeof(date_buf), "%A %d %B", &ti);
        lv_label_set_text(app.w_sleep_date, date_buf);
    }
}

/* ── data simulation tick (2 s) ────────────────────────────────── */

void data_tick_cb(lv_timer_t *t)
{
    lv_color_t wifi_col = wifi_manager_connected() ? C_GREEN : C_GRAY;
    if (app.w_wifi)
        lv_obj_set_style_text_color(app.w_wifi, wifi_col, 0);
    if (app.w_wifi_bd)
        lv_obj_set_style_text_color(app.w_wifi_bd, wifi_col, 0);
    if (app.w_wifi_sd)
        lv_obj_set_style_text_color(app.w_wifi_sd, wifi_col, 0);
    if (app.w_wifi_ld)
        lv_obj_set_style_text_color(app.w_wifi_ld, wifi_col, 0);
    if (app.w_wifi_wxd)
        lv_obj_set_style_text_color(app.w_wifi_wxd, wifi_col, 0);

    if (!uart_batt_valid())
    {
        gd.solar_kw = 0.0f;
        gd.load_kw = 0.0f;
        gd.batt_v = 0.0f;
        gd.batt_a = 0.0f;
        gd.chg_kw = 0.0f;
        gd.batt_temp = 0.0f;
        gd.pv_v = 0.0f;
        gd.pv_a = 0.0f;
        gd.grid_v = 0.0f;
        gd.grid_hz = 0.0f;
        gd.grid_chg_w = 0;
        gd.out_v = 0.0f;
        gd.out_hz = 0.0f;
        gd.out_a = 0.0f;
    }
    gd.today_solar_kwh += gd.solar_kw * (2.0f / 3600.0f);
    gd.today_load_kwh += gd.load_kw * (2.0f / 3600.0f);
    bool batt_ok = uart_batt_valid();

    bool pct_ok = batt_ok && gd.batt_pct > 0;
    lv_color_t arc_col = pct_ok ? (gd.batt_pct >= 50 ? C_GREEN : gd.batt_pct >= 20 ? C_AMBER
                                                                                   : C_RED)
                                : C_GRAY;

    /* ── Main screen ─────────────────────────────────────────────── */
    if (gd.solar_kw > 0)
        lv_lbl_setf(app.w_solar_val, "%.1f kw", gd.solar_kw);
    else if (app.w_solar_val)
        lv_label_set_text(app.w_solar_val, "--");

    if (gd.load_kw > 0)
        lv_lbl_setf(app.w_load_val, "%.1f kw", gd.load_kw);
    else if (app.w_load_val)
        lv_label_set_text(app.w_load_val, "--");

    if (app.w_batt_pct)
        pct_ok ? lv_label_set_text_fmt(app.w_batt_pct, "%d%%", gd.batt_pct)
               : lv_label_set_text(app.w_batt_pct, "--");

    if (app.w_batt_backup)
        pct_ok ? lv_label_set_text_fmt(app.w_batt_backup, "%dh %dm",
                                       gd.backup_h, gd.backup_m)
               : lv_label_set_text(app.w_batt_backup, "--");

    if (app.w_batt_arc)
    {
        lv_arc_set_value(app.w_batt_arc, pct_ok ? gd.batt_pct : 0);
        lv_obj_set_style_arc_color(app.w_batt_arc, arc_col, LV_PART_INDICATOR);
    }

    /* ── Battery detail ──────────────────────────────────────────── */
    if (app.w_bd_pct)
        pct_ok ? lv_label_set_text_fmt(app.w_bd_pct, "%d%%", gd.batt_pct)
               : lv_label_set_text(app.w_bd_pct, "--");
    if (app.w_bd_batt_v)
        gd.batt_v > 0 ? lv_lbl_setf(app.w_bd_batt_v, "%.1f V", gd.batt_v)
                      : lv_label_set_text(app.w_bd_batt_v, "--");
    if (app.w_bd_batt_a)
        lv_lbl_setf(app.w_bd_batt_a, "%.1f A", gd.batt_a);
    if (app.w_bd_chg)
        lv_lbl_setf(app.w_bd_chg, "%.1f kW", gd.chg_kw);
    if (app.w_bd_tmp)
        gd.batt_temp > 0 ? lv_lbl_setf(app.w_bd_tmp, "%.1f\xC2\xB0"
                                                     "C",
                                       gd.batt_temp)
                         : lv_label_set_text(app.w_bd_tmp, "--");
    if (gd.chg_set_w > 0)
        screen_battery_settings_set_chg_last(gd.chg_set_w);
    if (gd.chgv_set_v > 0)
        screen_battery_settings_set_chgv_last(gd.chgv_set_v);
    screen_battery_set_output_state(gd.inv_on, gd.out_switch);
    if (app.w_bd_bkp)
        pct_ok ? lv_label_set_text_fmt(app.w_bd_bkp, "%dh %dm",
                                       gd.backup_h, gd.backup_m)
               : lv_label_set_text(app.w_bd_bkp, "--");
    if (app.w_bd_grid_a)
        lv_lbl_setf(app.w_bd_grid_a, "%.1f A", gd.grid_a);
    if (app.w_bd_grid_chg_w)
        gd.grid_chg_w > 0 ? lv_label_set_text_fmt(app.w_bd_grid_chg_w, "%d W", gd.grid_chg_w)
                          : lv_label_set_text(app.w_bd_grid_chg_w, "--");
    /* status flags */
    if (app.w_bd_inv_on)
    {
        lv_label_set_text(app.w_bd_inv_on, gd.inv_on ? "ON" : "OFF");
        lv_obj_set_style_text_color(app.w_bd_inv_on,
                                    gd.inv_on ? C_GREEN : C_GRAY, 0);
    }
    if (app.w_bd_bypass)
    {
        lv_label_set_text(app.w_bd_bypass, gd.bypassing ? "YES" : "NO");
        lv_obj_set_style_text_color(app.w_bd_bypass,
                                    gd.bypassing ? C_AMBER : C_GRAY, 0);
    }
    if (app.w_bd_fault)
    {
        lv_label_set_text(app.w_bd_fault, gd.fault ? "YES" : "NO");
        lv_obj_set_style_text_color(app.w_bd_fault,
                                    gd.fault ? C_RED : C_GRAY, 0);
    }
    if (app.w_bd_ac_chg)
    {
        lv_label_set_text(app.w_bd_ac_chg, gd.ac_chg ? "YES" : "NO");
        lv_obj_set_style_text_color(app.w_bd_ac_chg,
                                    gd.ac_chg ? C_BLUE : C_GRAY, 0);
    }

    /* ── Solar detail ────────────────────────────────────────────── */
    if (app.w_sd_kw)
        gd.solar_kw > 0 ? lv_lbl_setf(app.w_sd_kw, "%.1f kw", gd.solar_kw)
                        : lv_label_set_text(app.w_sd_kw, "--");
    if (app.w_sd_grid_hz)
        gd.grid_hz > 0 ? lv_lbl_setf(app.w_sd_grid_hz, "%.1f Hz", gd.grid_hz)
                       : lv_label_set_text(app.w_sd_grid_hz, "--");
    if (app.w_sd_grid_v)
        gd.grid_v > 0 ? lv_lbl_setf(app.w_sd_grid_v, "%.0f V", gd.grid_v)
                      : lv_label_set_text(app.w_sd_grid_v, "--");
    if (app.w_sd_volt)
        gd.pv_v > 0 ? lv_lbl_setf(app.w_sd_volt, "%.0f V", gd.pv_v)
                    : lv_label_set_text(app.w_sd_volt, "--");
    if (app.w_sd_cur)
        gd.pv_a > 0 ? lv_lbl_setf(app.w_sd_cur, "%.1f A", gd.pv_a)
                    : lv_label_set_text(app.w_sd_cur, "--");
    if (app.w_sd_chart && app.w_sd_ser)
        lv_chart_set_next_value(app.w_sd_chart, app.w_sd_ser,
                                (lv_value_precise_t)(gd.solar_kw * 10.0f));

    /* ── Home load detail ────────────────────────────────────────── */
    if (app.w_ld_kw)
        gd.load_kw > 0 ? lv_lbl_setf(app.w_ld_kw, "%.1f kw", gd.load_kw)
                       : lv_label_set_text(app.w_ld_kw, "--");
    if (app.w_ld_out_v)
        gd.out_v > 0 ? lv_lbl_setf(app.w_ld_out_v, "%.0f V", gd.out_v)
                     : lv_label_set_text(app.w_ld_out_v, "--");
    if (app.w_ld_out_hz)
        gd.out_hz > 0 ? lv_lbl_setf(app.w_ld_out_hz, "%.1f Hz", gd.out_hz)
                      : lv_label_set_text(app.w_ld_out_hz, "--");
    if (app.w_ld_out_w)
        gd.load_kw > 0 ? lv_lbl_setf(app.w_ld_out_w, "%.0f W", gd.load_kw * 1000.0f)
                       : lv_label_set_text(app.w_ld_out_w, "--");
    if (app.w_ld_out_a)
        gd.out_a > 0 ? lv_lbl_setf(app.w_ld_out_a, "%.1f A", gd.out_a)
                     : lv_label_set_text(app.w_ld_out_a, "--");
    if (app.w_ld_chart && app.w_ld_ser)
        lv_chart_set_next_value(app.w_ld_chart, app.w_ld_ser,
                                (lv_value_precise_t)(gd.load_kw * 10.0f));

    /* weather widgets managed by weather_service */
}
