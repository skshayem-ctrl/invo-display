#include "ui_common.h"
#include "modbus_inverter.h"

static lv_obj_t *s_out_status_lbl = NULL;
static lv_obj_t *s_out_btn_lbl = NULL;
static lv_obj_t *s_out_btn = NULL;

static void output_toggle_cb(lv_event_t *e)
{
    modbus_inverter_request_output(gd.inv_on ? 0 : 1);
}

lv_obj_t *screen_battery_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    app.w_wifi_bd = add_detail_header(scr, "Battery");

    /* ── Left: SYS STATUS card (185×200, CENTER,-195,-20) ──────────── */
    lv_obj_t *scard = lv_obj_create(scr);
    lv_obj_set_size(scard, 185, 200);
    lv_obj_align(scard, LV_ALIGN_CENTER, -195, -20);
    lv_obj_set_style_bg_color(scard, lv_color_hex(0x0a0f1a), 0);
    lv_obj_set_style_border_color(scard, lv_color_hex(0x00aacc), 0);
    lv_obj_set_style_border_width(scard, 1, 0);
    lv_obj_set_style_radius(scard, 12, 0);
    lv_obj_set_style_pad_all(scard, 10, 0);
    lv_obj_clear_flag(scard, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *stitle = lv_label_create(scard);
    lv_label_set_text(stitle, "SYS STATUS");
    lv_obj_set_style_text_color(stitle, lv_color_hex(0x00aacc), 0);
    lv_obj_set_style_text_font(stitle, &lv_font_montserrat_12, 0);
    lv_obj_align(stitle, LV_ALIGN_TOP_MID, 0, 0);

    /* 4 status rows */
    static const char *const row_names[] = {"INVERTER", "AC CHARGE", "BYPASS", "FAULT"};
    lv_obj_t *status_vals[4];
    for (int i = 0; i < 4; i++)
    {
        int oy = 22 + i * 40;

        lv_obj_t *rl = lv_label_create(scard);
        lv_label_set_text(rl, row_names[i]);
        lv_obj_set_style_text_color(rl, C_GRAY, 0);
        lv_obj_set_style_text_font(rl, &lv_font_montserrat_12, 0);
        lv_obj_align(rl, LV_ALIGN_TOP_LEFT, 4, oy);

        lv_obj_t *rv = lv_label_create(scard);
        lv_label_set_text(rv, "--");
        lv_obj_set_style_text_color(rv, C_GRAY, 0);
        lv_obj_set_style_text_font(rv, &lv_font_montserrat_14, 0);
        lv_obj_align(rv, LV_ALIGN_TOP_RIGHT, -4, oy);
        status_vals[i] = rv;
    }
    app.w_bd_inv_on = status_vals[0];
    app.w_bd_ac_chg = status_vals[1];
    app.w_bd_bypass = status_vals[2];
    app.w_bd_fault = status_vals[3];

    /* ── Right: 2×3 stat grid (175×80, CENTER offsets) ─────────────── */
    app.w_bd_pct = make_stat_card(scr, 175, 80, -8, -170, "SOC", "--", "Charge level", C_GREEN, C_GRAY);
    app.w_bd_batt_v = make_stat_card(scr, 175, 80, 177, -170, "Battery V", "--", "DC bus voltage", C_GREEN, C_GRAY);
    app.w_bd_batt_a = make_stat_card(scr, 175, 80, -8, -75, "Battery A", "--", "Batt current", C_GREEN, C_GRAY);
    app.w_bd_chg = make_stat_card(scr, 175, 80, 177, -75, "Charge", "--", "Charge power", C_ORANGE, C_GRAY);
    app.w_bd_tmp = make_stat_card(scr, 175, 80, -8, 20, "Inv Temp", "--", "Inverter temp", C_ORANGE, C_GRAY);
    app.w_bd_bkp = make_stat_card(scr, 175, 80, 177, 20, "Backup", "--", "Est. runtime", C_BLUE, C_GRAY);

    /* ── Output toggle button ───────────────────────────────────────── */
    s_out_status_lbl = lv_label_create(scr);
    lv_label_set_text(s_out_status_lbl, "Output is OFF");
    lv_obj_set_style_text_color(s_out_status_lbl, lv_color_hex(0xff3300), 0);
    lv_obj_set_style_text_font(s_out_status_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(s_out_status_lbl, LV_ALIGN_CENTER, 85, 85);

    lv_obj_t *tog_btn = lv_btn_create(scr);
    s_out_btn = tog_btn; // ← add this line
    lv_obj_set_size(tog_btn, 120, 45);
    lv_obj_align(tog_btn, LV_ALIGN_CENTER, 85, 130);
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
