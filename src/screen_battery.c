#include "ui_common.h"
#include <stdio.h>
#ifdef ESP_PLATFORM
#include "modbus_inverter.h"
#endif

static int s_chg_target = 0;
static lv_obj_t *s_chg_target_lbl = NULL;

static int s_chgv_target = 0;       /* tenths of a volt, 0 = not set yet */
static lv_obj_t *s_chgv_target_lbl = NULL;

static void chg_minus_cb(lv_event_t *e)
{
    if (s_chg_target >= 50) s_chg_target -= 50;
    if (s_chg_target_lbl) lv_label_set_text_fmt(s_chg_target_lbl, "%d W", s_chg_target);
}

static void chg_plus_cb(lv_event_t *e)
{
    if (s_chg_target <= 950) s_chg_target += 50;
    if (s_chg_target_lbl) lv_label_set_text_fmt(s_chg_target_lbl, "%d W", s_chg_target);
}

static void chg_set_cb(lv_event_t *e)
{
#ifdef ESP_PLATFORM
    modbus_inverter_request_chg_w(s_chg_target);
#endif
}

static void chgv_minus_cb(lv_event_t *e)
{
    if (s_chgv_target >= 462) s_chgv_target -= 2;   /* min 46.2V */
    if (s_chgv_target_lbl) lv_label_set_text_fmt(s_chgv_target_lbl, "%d.%d V", s_chgv_target / 10, s_chgv_target % 10);
}

static void chgv_plus_cb(lv_event_t *e)
{
    if (s_chgv_target <= 582) s_chgv_target += 2;   /* max 58.4V */
    if (s_chgv_target_lbl) lv_label_set_text_fmt(s_chgv_target_lbl, "%d.%d V", s_chgv_target / 10, s_chgv_target % 10);
}

static void chgv_set_cb(lv_event_t *e)
{
    if (s_chgv_target == 0) return;
#ifdef ESP_PLATFORM
    modbus_inverter_request_chg_v(s_chgv_target);
#endif
}

static void output_on_cb(lv_event_t *e)
{
#ifdef ESP_PLATFORM
    modbus_inverter_request_output(1);
#else
    FILE *f = fopen("/home/intelli/invo_cmd", "w");
    if (f) { fprintf(f, "output_on\n"); fclose(f); }
#endif
}

static void output_off_cb(lv_event_t *e)
{
#ifdef ESP_PLATFORM
    modbus_inverter_request_output(0);
#else
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

    /* ── Output ON button — below stat grid, centered on right columns ─ */
    lv_obj_t *on_btn = lv_btn_create(scr);
    lv_obj_set_size(on_btn, 85, 55);
    lv_obj_align(on_btn, LV_ALIGN_CENTER, 35, 125);
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

    /* ── Output OFF button ───────────────────────────────────────────── */
    lv_obj_t *off_btn = lv_btn_create(scr);
    lv_obj_set_size(off_btn, 85, 55);
    lv_obj_align(off_btn, LV_ALIGN_CENTER, 135, 125);
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

    /* ── Mains Charge card (below SYS STATUS, left side) ──────────────── */
    lv_obj_t *ccard = lv_obj_create(scr);
    lv_obj_set_size(ccard, 185, 90);
    lv_obj_align(ccard, LV_ALIGN_CENTER, -185, 128);
    lv_obj_set_style_bg_color(ccard, lv_color_hex(0x0a0f1a), 0);
    lv_obj_set_style_border_color(ccard, C_AMBER, 0);
    lv_obj_set_style_border_width(ccard, 1, 0);
    lv_obj_set_style_radius(ccard, 12, 0);
    lv_obj_set_style_pad_all(ccard, 8, 0);
    lv_obj_clear_flag(ccard, LV_OBJ_FLAG_SCROLLABLE);

    /* Row 1: label + live reading */
    lv_obj_t *ctl = lv_label_create(ccard);
    lv_label_set_text(ctl, "MAINS CHARGE");
    lv_obj_set_style_text_color(ctl, C_AMBER, 0);
    lv_obj_set_style_text_font(ctl, &lv_font_montserrat_12, 0);
    lv_obj_align(ctl, LV_ALIGN_TOP_LEFT, 0, 0);

    app.w_bd_grid_chg_w = lv_label_create(ccard);
    lv_label_set_text(app.w_bd_grid_chg_w, "--");
    lv_obj_set_style_text_color(app.w_bd_grid_chg_w, C_WHITE, 0);
    lv_obj_set_style_text_font(app.w_bd_grid_chg_w, &lv_font_montserrat_14, 0);
    lv_obj_align(app.w_bd_grid_chg_w, LV_ALIGN_TOP_RIGHT, 0, 0);

    /* Row 2: − target + SET */
    lv_obj_t *btn_m = lv_btn_create(ccard);
    lv_obj_set_size(btn_m, 32, 32);
    lv_obj_align(btn_m, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(btn_m, C_DGRAY, 0);
    lv_obj_set_style_radius(btn_m, 6, 0);
    lv_obj_add_event_cb(btn_m, chg_minus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lm = lv_label_create(btn_m);
    lv_label_set_text(lm, "-");
    lv_obj_set_style_text_font(lm, &lv_font_montserrat_16, 0);
    lv_obj_center(lm);

    s_chg_target_lbl = lv_label_create(ccard);
    lv_label_set_text_fmt(s_chg_target_lbl, "%d W", s_chg_target);
    lv_obj_set_style_text_color(s_chg_target_lbl, C_AMBER, 0);
    lv_obj_set_style_text_font(s_chg_target_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(s_chg_target_lbl, LV_ALIGN_BOTTOM_MID, -16, -6);

    lv_obj_t *btn_p = lv_btn_create(ccard);
    lv_obj_set_size(btn_p, 32, 32);
    lv_obj_align(btn_p, LV_ALIGN_BOTTOM_MID, 24, 0);
    lv_obj_set_style_bg_color(btn_p, C_DGRAY, 0);
    lv_obj_set_style_radius(btn_p, 6, 0);
    lv_obj_add_event_cb(btn_p, chg_plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lp = lv_label_create(btn_p);
    lv_label_set_text(lp, "+");
    lv_obj_set_style_text_font(lp, &lv_font_montserrat_16, 0);
    lv_obj_center(lp);

    lv_obj_t *btn_set = lv_btn_create(ccard);
    lv_obj_set_size(btn_set, 42, 32);
    lv_obj_align(btn_set, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_set, lv_color_hex(0x1a1200), 0);
    lv_obj_set_style_border_color(btn_set, C_AMBER, 0);
    lv_obj_set_style_border_width(btn_set, 1, 0);
    lv_obj_set_style_radius(btn_set, 6, 0);
    lv_obj_add_event_cb(btn_set, chg_set_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ls = lv_label_create(btn_set);
    lv_label_set_text(ls, "SET");
    lv_obj_set_style_text_color(ls, C_AMBER, 0);
    lv_obj_set_style_text_font(ls, &lv_font_montserrat_12, 0);
    lv_obj_center(ls);

    /* ── Charge Voltage card (below ON/OFF buttons, right side) ──────────── */
    lv_obj_t *vcard = lv_obj_create(scr);
    lv_obj_set_size(vcard, 185, 90);
    lv_obj_align(vcard, LV_ALIGN_CENTER, 85, 215);
    lv_obj_set_style_bg_color(vcard, lv_color_hex(0x0a0f1a), 0);
    lv_obj_set_style_border_color(vcard, C_GREEN, 0);
    lv_obj_set_style_border_width(vcard, 1, 0);
    lv_obj_set_style_radius(vcard, 12, 0);
    lv_obj_set_style_pad_all(vcard, 8, 0);
    lv_obj_clear_flag(vcard, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *vtitle = lv_label_create(vcard);
    lv_label_set_text(vtitle, "CHG VOLTAGE");
    lv_obj_set_style_text_color(vtitle, C_GREEN, 0);
    lv_obj_set_style_text_font(vtitle, &lv_font_montserrat_12, 0);
    lv_obj_align(vtitle, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *btn_vm = lv_btn_create(vcard);
    lv_obj_set_size(btn_vm, 32, 32);
    lv_obj_align(btn_vm, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(btn_vm, C_DGRAY, 0);
    lv_obj_set_style_radius(btn_vm, 6, 0);
    lv_obj_add_event_cb(btn_vm, chgv_minus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lvm = lv_label_create(btn_vm);
    lv_label_set_text(lvm, "-");
    lv_obj_set_style_text_font(lvm, &lv_font_montserrat_16, 0);
    lv_obj_center(lvm);

    s_chgv_target_lbl = lv_label_create(vcard);
    lv_label_set_text(s_chgv_target_lbl, "--");
    lv_obj_set_style_text_color(s_chgv_target_lbl, C_GREEN, 0);
    lv_obj_set_style_text_font(s_chgv_target_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(s_chgv_target_lbl, LV_ALIGN_BOTTOM_MID, -16, -6);

    lv_obj_t *btn_vp = lv_btn_create(vcard);
    lv_obj_set_size(btn_vp, 32, 32);
    lv_obj_align(btn_vp, LV_ALIGN_BOTTOM_MID, 24, 0);
    lv_obj_set_style_bg_color(btn_vp, C_DGRAY, 0);
    lv_obj_set_style_radius(btn_vp, 6, 0);
    lv_obj_add_event_cb(btn_vp, chgv_plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lvp = lv_label_create(btn_vp);
    lv_label_set_text(lvp, "+");
    lv_obj_set_style_text_font(lvp, &lv_font_montserrat_16, 0);
    lv_obj_center(lvp);

    lv_obj_t *btn_vset = lv_btn_create(vcard);
    lv_obj_set_size(btn_vset, 42, 32);
    lv_obj_align(btn_vset, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_vset, lv_color_hex(0x001a00), 0);
    lv_obj_set_style_border_color(btn_vset, C_GREEN, 0);
    lv_obj_set_style_border_width(btn_vset, 1, 0);
    lv_obj_set_style_radius(btn_vset, 6, 0);
    lv_obj_add_event_cb(btn_vset, chgv_set_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lvs = lv_label_create(btn_vset);
    lv_label_set_text(lvs, "SET");
    lv_obj_set_style_text_color(lvs, C_GREEN, 0);
    lv_obj_set_style_text_font(lvs, &lv_font_montserrat_12, 0);
    lv_obj_center(lvs);

    add_logo(scr, -22);
    return scr;
}
