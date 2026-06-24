#include "ui_common.h"
#include <stdio.h>

static void output_on_cb(lv_event_t *e)
{
#ifndef ESP_PLATFORM
    FILE *f = fopen("/home/intelli/invo_cmd", "w");
    if (f) { fprintf(f, "output_on\n"); fclose(f); }
#endif
}

static void output_off_cb(lv_event_t *e)
{
#ifndef ESP_PLATFORM
    FILE *f = fopen("/home/intelli/invo_cmd", "w");
    if (f) { fprintf(f, "output_off\n"); fclose(f); }
#endif
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
    for (int i = 0; i < 4; i++) {
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
    app.w_bd_fault  = status_vals[3];

    /* ── Right: 2×3 stat grid (175×80, CENTER offsets) ─────────────── */
    app.w_bd_pct    = make_stat_card(scr, 175, 80,  -8, -170, "SOC",       "--", "Charge level",  C_GREEN,  C_GRAY);
    app.w_bd_batt_v = make_stat_card(scr, 175, 80, 177, -170, "Battery V", "--", "DC bus voltage", C_GREEN,  C_GRAY);
    app.w_bd_batt_a = make_stat_card(scr, 175, 80,  -8,  -75, "Battery A", "--", "Batt current",  C_GREEN,  C_GRAY);
    app.w_bd_chg    = make_stat_card(scr, 175, 80, 177,  -75, "Charge",    "--", "Charge power",  C_ORANGE, C_GRAY);
    app.w_bd_tmp    = make_stat_card(scr, 175, 80,  -8,   20, "Inv Temp",  "--", "Inverter temp", C_ORANGE, C_GRAY);
    app.w_bd_bkp    = make_stat_card(scr, 175, 80, 177,   20, "Backup",    "--", "Est. runtime",  C_BLUE,   C_GRAY);

    /* ── Output ON button (CENTER,10,-90) ───────────────────────────── */
    lv_obj_t *on_btn = lv_btn_create(scr);
    lv_obj_set_size(on_btn, 85, 55);
    lv_obj_align(on_btn, LV_ALIGN_CENTER, 10, -90);
    lv_obj_set_style_bg_color(on_btn, lv_color_hex(0x0d2a0d), 0);
    lv_obj_set_style_radius(on_btn, 8, 0);
    lv_obj_set_style_border_color(on_btn, C_GREEN, 0);
    lv_obj_set_style_border_width(on_btn, 2, 0);
    lv_obj_add_event_cb(on_btn, output_on_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *on_lbl = lv_label_create(on_btn);
    lv_label_set_text(on_lbl, "OUT\nON");
    lv_obj_set_style_text_color(on_lbl, C_GREEN, 0);
    lv_obj_set_style_text_font(on_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(on_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(on_lbl);

    /* ── Output OFF button (CENTER,110,-90) ─────────────────────────── */
    lv_obj_t *off_btn = lv_btn_create(scr);
    lv_obj_set_size(off_btn, 85, 55);
    lv_obj_align(off_btn, LV_ALIGN_CENTER, 110, -90);
    lv_obj_set_style_bg_color(off_btn, lv_color_hex(0x2a0d0d), 0);
    lv_obj_set_style_radius(off_btn, 8, 0);
    lv_obj_set_style_border_color(off_btn, lv_color_hex(0xff3300), 0);
    lv_obj_set_style_border_width(off_btn, 2, 0);
    lv_obj_add_event_cb(off_btn, output_off_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *off_lbl = lv_label_create(off_btn);
    lv_label_set_text(off_lbl, "OUT\nOFF");
    lv_obj_set_style_text_color(off_lbl, lv_color_hex(0xff3300), 0);
    lv_obj_set_style_text_font(off_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(off_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(off_lbl);

    add_logo(scr, -22);
    return scr;
}
