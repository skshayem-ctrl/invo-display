#include "ui_common.h"
#include "modbus_inverter.h"
#include "esp_log.h"
#include "esp_timer.h"

#define TAG "btn"

static lv_obj_t *s_out_status_lbl = NULL;
static lv_obj_t *s_out_btn_lbl = NULL;
static lv_obj_t *s_out_btn = NULL;

static void output_toggle_cb(lv_event_t *e)
{
    static int64_t s_last_press_ms = 0;
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - s_last_press_ms < 1000) return;  /* ignore within 1 second */
    s_last_press_ms = now_ms;

    int cmd = gd.out_switch ? 0 : 1;
    ESP_LOGI(TAG, "TOUCH  output %s requested  t=%lldms", cmd ? "ON" : "OFF", now_ms);
    modbus_inverter_request_output(cmd);
}

lv_obj_t *screen_battery_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    app.w_wifi_bd = add_detail_header(scr, "Battery");

    /* ── Status flags bar (compact horizontal strip) ────────────────── */
    lv_obj_t *sbar = lv_obj_create(scr);
    lv_obj_set_size(sbar, 480, 60);
    lv_obj_align(sbar, LV_ALIGN_CENTER, 0, -210);
    lv_obj_set_style_bg_color(sbar, lv_color_hex(0x0a0f1a), 0);
    lv_obj_set_style_border_color(sbar, lv_color_hex(0x00aacc), 0);
    lv_obj_set_style_border_width(sbar, 1, 0);
    lv_obj_set_style_radius(sbar, 12, 0);
    lv_obj_set_style_pad_hor(sbar, 16, 0);
    lv_obj_set_style_pad_ver(sbar, 8, 0);
    lv_obj_clear_flag(sbar, LV_OBJ_FLAG_SCROLLABLE);

    static const char *const snames[] = {"INVERTER", "AC CHG", "BYPASS", "FAULT"};
    lv_obj_t *status_vals[4];
    for (int i = 0; i < 4; i++) {
        lv_obj_t *nl = lv_label_create(sbar);
        lv_label_set_text(nl, snames[i]);
        lv_obj_set_style_text_color(nl, C_GRAY, 0);
        lv_obj_set_style_text_font(nl, &lv_font_montserrat_12, 0);
        lv_obj_align(nl, LV_ALIGN_TOP_LEFT, i * 112, 0);

        lv_obj_t *vl = lv_label_create(sbar);
        lv_label_set_text(vl, "--");
        lv_obj_set_style_text_color(vl, C_GRAY, 0);
        lv_obj_set_style_text_font(vl, &lv_font_montserrat_14, 0);
        lv_obj_align(vl, LV_ALIGN_TOP_LEFT, i * 112, 20);
        status_vals[i] = vl;
    }
    app.w_bd_inv_on = status_vals[0];
    app.w_bd_ac_chg = status_vals[1];
    app.w_bd_bypass = status_vals[2];
    app.w_bd_fault  = status_vals[3];

    /* ── Stat cards — centered 2×3 grid ─────────────────────────────── */
    app.w_bd_pct    = make_stat_card(scr, 210, 88, -112, -115, "SOC",       "--", "Charge level",     C_GREEN,  C_GRAY);
    app.w_bd_batt_v = make_stat_card(scr, 210, 88,  112, -115, "Battery V", "--", "DC bus voltage",   C_GREEN,  C_GRAY);
    app.w_bd_batt_a = make_stat_card(scr, 210, 88, -112,  -10, "Battery A", "--", "Batt current",     C_GREEN,  C_GRAY);
    app.w_bd_chg    = make_stat_card(scr, 210, 88,  112,  -10, "Charge",    "--", "Charge power",     C_ORANGE, C_GRAY);
    app.w_bd_tmp    = make_stat_card(scr, 210, 88, -112,   95, "Inv Temp",  "--", "Inverter temp",    C_ORANGE, C_GRAY);
    app.w_bd_grid_a = make_stat_card(scr, 210, 88,  112,   95, "Grid A",    "--", "AC input current", C_BLUE,   C_GRAY);

    /* ── Output status + toggle (centered) ─────────────────────────── */
    s_out_status_lbl = lv_label_create(scr);
    lv_label_set_text(s_out_status_lbl, "Output is OFF");
    lv_obj_set_style_text_color(s_out_status_lbl, lv_color_hex(0xff3300), 0);
    lv_obj_set_style_text_font(s_out_status_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(s_out_status_lbl, LV_ALIGN_CENTER, 0, 192);

    lv_obj_t *tog_btn = lv_btn_create(scr);
    s_out_btn = tog_btn;
    lv_obj_set_size(tog_btn, 160, 45);
    lv_obj_align(tog_btn, LV_ALIGN_CENTER, 0, 238);
    lv_obj_set_style_bg_color(tog_btn, lv_color_hex(0x0d2a0d), 0);
    lv_obj_set_style_radius(tog_btn, 8, 0);
    lv_obj_set_style_border_color(tog_btn, C_GREEN, 0);
    lv_obj_set_style_border_width(tog_btn, 2, 0);
    lv_obj_add_event_cb(tog_btn, output_toggle_cb, LV_EVENT_CLICKED, NULL);
    s_out_btn_lbl = lv_label_create(tog_btn);
    lv_label_set_text(s_out_btn_lbl, "Turn ON");
    lv_obj_set_style_text_color(s_out_btn_lbl, C_GREEN, 0);
    lv_obj_set_style_text_font(s_out_btn_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(s_out_btn_lbl);

    add_logo(scr, -22);
    return scr;
}

void screen_battery_set_output_state(int actual_on, int switch_on)
{
    if (s_out_status_lbl)
    {
        lv_label_set_text(s_out_status_lbl, actual_on ? "Output is ON" : "Output is OFF");
        lv_obj_set_style_text_color(s_out_status_lbl,
                                    actual_on ? C_GREEN : lv_color_hex(0xff3300), 0);
    }
    if (s_out_btn)
    {
        lv_obj_set_style_bg_color(s_out_btn,
                                  switch_on ? lv_color_hex(0x2a0d0d) : lv_color_hex(0x0d2a0d), 0);
        lv_obj_set_style_border_color(s_out_btn,
                                      switch_on ? lv_color_hex(0xff3300) : C_GREEN, 0);
    }
    if (s_out_btn_lbl)
    {
        lv_label_set_text(s_out_btn_lbl, switch_on ? "Turn OFF" : "Turn ON");
        lv_obj_set_style_text_color(s_out_btn_lbl,
                                    switch_on ? lv_color_hex(0xff3300) : C_GREEN, 0);
    }
}
