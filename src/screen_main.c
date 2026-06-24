#include "ui_common.h"

lv_obj_t *screen_main_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);

    /* wifi + time + date */
    lv_obj_t *wifi_btn = mk_cont(scr, 60, 42);
    lv_obj_align(wifi_btn, LV_ALIGN_TOP_MID, 0, 14);
    lv_obj_add_flag(wifi_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(wifi_btn, go_wifi_cb, LV_EVENT_CLICKED, NULL);
    app.w_wifi = mk_lbl(wifi_btn, LV_SYMBOL_WIFI, &lv_font_montserrat_14, C_GRAY,
                        LV_ALIGN_CENTER, 0, 0);
    app.w_time = mk_lbl(scr, "10:30", &lv_font_montserrat_48, C_WHITE,
                        LV_ALIGN_TOP_MID, 0, 48);
    app.w_date = mk_lbl(scr, "May 21 Wednesday",
                        &lv_font_montserrat_14, C_GRAY,
                        LV_ALIGN_TOP_MID, 0, 108);

    add_hdiv(scr, 134, 520);

    /* nav row: Settings | History | Alerts */
    const struct { const char *sym; const char *lbl; int xoff; lv_event_cb_t cb; } nav[] = {
        {LV_SYMBOL_SETTINGS, "Settings", -165, go_settings_cb},
        {LV_SYMBOL_LIST,     "History",     0, go_history_cb},
        {LV_SYMBOL_BELL,     "Alerts",    165, go_alerts_cb},
    };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *c = mk_cont(scr, 80, 50);
        lv_obj_align(c, LV_ALIGN_TOP_MID, nav[i].xoff, 142);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, nav[i].cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *ic = lv_label_create(c);
        lv_label_set_text(ic, nav[i].sym);
        lv_obj_set_style_text_color(ic, C_GRAY, 0);
        lv_obj_set_style_text_font(ic, &lv_font_montserrat_16, 0);
        lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_t *lb = lv_label_create(c);
        lv_label_set_text(lb, nav[i].lbl);
        lv_obj_set_style_text_color(lb, C_GRAY, 0);
        lv_obj_set_style_text_font(lb, &lv_font_montserrat_12, 0);
        lv_obj_align(lb, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    add_hdiv(scr, 200, 520);

    /* ── Solar Input (left) ────────────────────────────────────── */
    {
        lv_obj_t *c = mk_cont(scr, 120, 185);
        lv_obj_align(c, LV_ALIGN_LEFT_MID, 60, -30);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, go_solar_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *box = lv_obj_create(c);
        lv_obj_set_size(box, 82, 82);
        lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(box, lv_color_hex(0x0D1F12), 0);
        lv_obj_set_style_border_color(box, lv_color_hex(0x1A3820), 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_radius(box, 14, 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *ic = lv_label_create(box);
        lv_label_set_text(ic, LV_SYMBOL_CHARGE);
        lv_obj_set_style_text_color(ic, C_AMBER, 0);
        lv_obj_set_style_text_font(ic, &lv_font_montserrat_36, 0);
        lv_obj_center(ic);

        mk_lbl(c, "Solar Input", &lv_font_montserrat_14, C_WHITE,
               LV_ALIGN_BOTTOM_MID, 0, -42);

        app.w_solar_val = lv_label_create(c);
        lv_lbl_setf(app.w_solar_val, "%.1f kw", gd.solar_kw);
        lv_obj_set_style_text_color(app.w_solar_val, C_BLUE, 0);
        lv_obj_set_style_text_font(app.w_solar_val, &lv_font_montserrat_20, 0);
        lv_obj_align(app.w_solar_val, LV_ALIGN_BOTTOM_MID, 0, -14);

        lv_obj_t *uln = lv_obj_create(c);
        lv_obj_set_size(uln, 90, 2);
        lv_obj_align(uln, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(uln, C_BLUE, 0);
        lv_obj_set_style_bg_opa(uln, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(uln, 0, 0);
        lv_obj_set_style_pad_all(uln, 0, 0);
        lv_obj_set_style_radius(uln, 0, 0);
        lv_obj_clear_flag(uln, LV_OBJ_FLAG_CLICKABLE);
    }

    /* ── Battery Arc (centre) ──────────────────────────────────── */
    {
        lv_obj_t *ba = mk_cont(scr, 224, 224);
        lv_obj_align(ba, LV_ALIGN_CENTER, 0, 12);
        lv_obj_add_flag(ba, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ba, go_batt_cb, LV_EVENT_CLICKED, NULL);

        app.w_batt_arc = lv_arc_create(ba);
        lv_obj_set_size(app.w_batt_arc, 214, 214);
        lv_obj_center(app.w_batt_arc);
        lv_arc_set_rotation(app.w_batt_arc, 135);
        lv_arc_set_bg_angles(app.w_batt_arc, 0, 270);
        lv_arc_set_range(app.w_batt_arc, 0, 100);
        lv_arc_set_value(app.w_batt_arc, gd.batt_pct);
        lv_obj_set_style_arc_color(app.w_batt_arc, C_GREEN, LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(app.w_batt_arc, 13, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(app.w_batt_arc, lv_color_hex(0x1A2A1A), LV_PART_MAIN);
        lv_obj_set_style_arc_width(app.w_batt_arc, 13, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(app.w_batt_arc, LV_OPA_TRANSP, 0);
        lv_obj_set_style_opa(app.w_batt_arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_clear_flag(app.w_batt_arc, LV_OBJ_FLAG_CLICKABLE);

        mk_lbl(ba, LV_SYMBOL_BATTERY_FULL, &lv_font_montserrat_24,
               C_GREEN, LV_ALIGN_CENTER, 0, -52);

        app.w_batt_pct = lv_label_create(ba);
        lv_label_set_text_fmt(app.w_batt_pct, "%d%%", gd.batt_pct);
        lv_obj_set_style_text_color(app.w_batt_pct, C_WHITE, 0);
        lv_obj_set_style_text_font(app.w_batt_pct, &lv_font_montserrat_36, 0);
        lv_obj_align(app.w_batt_pct, LV_ALIGN_CENTER, 0, -12);

        mk_lbl(ba, "BATTERY", &lv_font_montserrat_12, C_GRAY,
               LV_ALIGN_CENTER, 0, 30);

        app.w_batt_backup = lv_label_create(ba);
        lv_label_set_text_fmt(app.w_batt_backup, "%dh %dm",
                              gd.backup_h, gd.backup_m);
        lv_obj_set_style_text_color(app.w_batt_backup, C_GREEN, 0);
        lv_obj_set_style_text_font(app.w_batt_backup, &lv_font_montserrat_16, 0);
        lv_obj_align(app.w_batt_backup, LV_ALIGN_CENTER, 0, 52);

        mk_lbl(ba, "backup time", &lv_font_montserrat_12, C_GRAY,
               LV_ALIGN_CENTER, 0, 74);
    }

    /* ── Home Load (right) ─────────────────────────────────────── */
    {
        lv_obj_t *c = mk_cont(scr, 120, 185);
        lv_obj_align(c, LV_ALIGN_RIGHT_MID, -60, -30);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, go_load_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *box = lv_obj_create(c);
        lv_obj_set_size(box, 82, 82);
        lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(box, lv_color_hex(0x0A1A2E), 0);
        lv_obj_set_style_border_color(box, lv_color_hex(0x1A2A4A), 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_radius(box, 14, 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *ic = lv_label_create(box);
        lv_label_set_text(ic, LV_SYMBOL_HOME);
        lv_obj_set_style_text_color(ic, C_BLUE, 0);
        lv_obj_set_style_text_font(ic, &lv_font_montserrat_36, 0);
        lv_obj_center(ic);

        mk_lbl(c, "Home Load", &lv_font_montserrat_14, C_WHITE,
               LV_ALIGN_BOTTOM_MID, 0, -42);

        app.w_load_val = lv_label_create(c);
        lv_lbl_setf(app.w_load_val, "%.1f kw", gd.load_kw);
        lv_obj_set_style_text_color(app.w_load_val, C_BLUE, 0);
        lv_obj_set_style_text_font(app.w_load_val, &lv_font_montserrat_20, 0);
        lv_obj_align(app.w_load_val, LV_ALIGN_BOTTOM_MID, 0, -14);

        lv_obj_t *uln = lv_obj_create(c);
        lv_obj_set_size(uln, 90, 2);
        lv_obj_align(uln, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(uln, C_BLUE, 0);
        lv_obj_set_style_bg_opa(uln, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(uln, 0, 0);
        lv_obj_set_style_pad_all(uln, 0, 0);
        lv_obj_set_style_radius(uln, 0, 0);
        lv_obj_clear_flag(uln, LV_OBJ_FLAG_CLICKABLE);
    }

    add_hdiv(scr, 460, 480);

    /* ── Bottom info row ───────────────────────────────────────── */
    /* Weather */
    {
        lv_obj_t *c = mk_cont(scr, 140, 95);
        lv_obj_align(c, LV_ALIGN_BOTTOM_LEFT, 145, -98);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, go_wx_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *row = mk_row(c);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 4);
        lv_obj_t *ic = weather_icon_create(row, 26, WX_ICON_TEMPERATURE);
        app.w_main_wx_tmp = lv_label_create(row);
        lv_label_set_text(app.w_main_wx_tmp, "--\xC2\xB0""C");
        lv_obj_set_style_text_color(app.w_main_wx_tmp, C_WHITE, 0);
        lv_obj_set_style_text_font(app.w_main_wx_tmp, &lv_font_montserrat_20, 0);

        mk_lbl(c, "Bengaluru", &lv_font_montserrat_12, C_GRAY,
               LV_ALIGN_BOTTOM_MID, 0, 0);
    }
    /* Air Quality */
    {
        lv_obj_t *c = mk_cont(scr, 140, 95);
        lv_obj_align(c, LV_ALIGN_BOTTOM_MID, 0, -98);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, go_wx_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *row = mk_row(c);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 4);
        lv_obj_t *ic = weather_icon_create(row, 26, WX_ICON_AQI);
        app.w_main_wx_aqi = lv_label_create(row);
        lv_label_set_text(app.w_main_wx_aqi, "--");
        lv_obj_set_style_text_color(app.w_main_wx_aqi, C_WHITE, 0);
        lv_obj_set_style_text_font(app.w_main_wx_aqi, &lv_font_montserrat_20, 0);

        app.w_main_wx_aqi_cat = mk_lbl(c, "--", &lv_font_montserrat_12, C_GREEN,
                                       LV_ALIGN_BOTTOM_MID, 0, -16);
        mk_lbl(c, "Air Quality", &lv_font_montserrat_12, C_GRAY,
               LV_ALIGN_BOTTOM_MID, 0, 0);
    }
    /* Humidity */
    {
        lv_obj_t *c = mk_cont(scr, 140, 95);
        lv_obj_align(c, LV_ALIGN_BOTTOM_RIGHT, -145, -98);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, go_wx_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *row = mk_row(c);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 4);
        lv_obj_t *ic = weather_icon_create(row, 26, WX_ICON_HUMIDITY);
        app.w_main_wx_hum = lv_label_create(row);
        lv_label_set_text(app.w_main_wx_hum, "--%");
        lv_obj_set_style_text_color(app.w_main_wx_hum, C_WHITE, 0);
        lv_obj_set_style_text_font(app.w_main_wx_hum, &lv_font_montserrat_20, 0);

        mk_lbl(c, "Humidity", &lv_font_montserrat_12, C_GRAY,
               LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    add_logo(scr, -30);

    /* ── Overload warning ring (hidden) ───────────────────────── */
    app.w_warn_ring = lv_arc_create(scr);
    lv_obj_set_size(app.w_warn_ring, 228, 228);
    lv_obj_align(app.w_warn_ring, LV_ALIGN_CENTER, 0, 12);
    lv_arc_set_rotation(app.w_warn_ring, 0);
    lv_arc_set_bg_angles(app.w_warn_ring, 0, 360);
    lv_arc_set_value(app.w_warn_ring, 100);
    lv_obj_set_style_arc_color(app.w_warn_ring, C_RED, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(app.w_warn_ring, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(app.w_warn_ring, lv_color_hex(0x200404), LV_PART_MAIN);
    lv_obj_set_style_arc_width(app.w_warn_ring, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(app.w_warn_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_size(app.w_warn_ring, 0, 0, LV_PART_KNOB);
    lv_obj_clear_flag(app.w_warn_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(app.w_warn_ring, LV_OBJ_FLAG_HIDDEN);

    /* ── Overload modal dialog (hidden) ───────────────────────── */
    app.w_warn_dlg = lv_obj_create(scr);
    lv_obj_set_size(app.w_warn_dlg, 390, 210);
    lv_obj_align(app.w_warn_dlg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(app.w_warn_dlg, lv_color_hex(0x200808), 0);
    lv_obj_set_style_border_color(app.w_warn_dlg, C_RED, 0);
    lv_obj_set_style_border_width(app.w_warn_dlg, 2, 0);
    lv_obj_set_style_radius(app.w_warn_dlg, 14, 0);
    lv_obj_set_style_pad_all(app.w_warn_dlg, 16, 0);
    lv_obj_clear_flag(app.w_warn_dlg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(app.w_warn_dlg, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *wi = lv_label_create(app.w_warn_dlg);
    lv_label_set_text(wi, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_color(wi, C_RED, 0);
    lv_obj_set_style_text_font(wi, &lv_font_montserrat_24, 0);
    lv_obj_align(wi, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *wt = lv_label_create(app.w_warn_dlg);
    lv_label_set_text(wt, "OVERLOAD WARNING!");
    lv_obj_set_style_text_color(wt, C_RED, 0);
    lv_obj_set_style_text_font(wt, &lv_font_montserrat_20, 0);
    lv_obj_align(wt, LV_ALIGN_TOP_MID, 0, 34);

    lv_obj_t *wm = lv_label_create(app.w_warn_dlg);
    lv_label_set_text(wm,
                      "Connected load (3.6 kW) is exceeding\n"
                      "the safe limit (3.0 kW).\n"
                      "Please reduce the load to avoid\nsystem shutdown.");
    lv_obj_set_style_text_color(wm, C_GRAY, 0);
    lv_obj_set_style_text_font(wm, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(wm, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(wm, 340);
    lv_obj_align(wm, LV_ALIGN_TOP_MID, 0, 64);

    lv_obj_t *ob = lv_btn_create(app.w_warn_dlg);
    lv_obj_set_size(ob, 190, 40);
    lv_obj_align(ob, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(ob, C_RED, 0);
    lv_obj_set_style_radius(ob, 8, 0);
    lv_obj_add_event_cb(ob, warn_ok_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ol = lv_label_create(ob);
    lv_label_set_text(ol, "OK, GOT IT");
    lv_obj_set_style_text_color(ol, C_WHITE, 0);
    lv_obj_set_style_text_font(ol, &lv_font_montserrat_14, 0);
    lv_obj_center(ol);

    return scr;
}
