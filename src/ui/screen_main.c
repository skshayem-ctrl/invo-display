#include "ui_common.h"

lv_obj_t *screen_main_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_home_cb, LV_EVENT_GESTURE, NULL);

    /* ── WiFi (top center, tappable) ─────────────────────────────── */
    lv_obj_t *wifi_btn = mk_cont(scr, 50, 40);
    lv_obj_align(wifi_btn, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_add_flag(wifi_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(wifi_btn, go_wifi_cb, LV_EVENT_CLICKED, NULL);
    app.w_wifi = mk_lbl(wifi_btn, LV_SYMBOL_WIFI, &lv_font_montserrat_16, C_GRAY,
                        LV_ALIGN_CENTER, 0, 0);

    /* ── Time ────────────────────────────────────────────────────── */
    app.w_time = mk_lbl(scr, "10:30", &lv_font_montserrat_48, C_WHITE,
                        LV_ALIGN_TOP_MID, 0, 62);

    /* ── Day ─────────────────────────────────────────────────────── */
    app.w_date = mk_lbl(scr, "Monday", &lv_font_montserrat_20, C_GRAY,
                        LV_ALIGN_TOP_MID, 0, 116);

    /* ── Settings icon (left, tap) ───────────────────────────────── */
    {
        lv_obj_t *sb = mk_cont(scr, 60, 60);
        lv_obj_align(sb, LV_ALIGN_CENTER, -180, -180);
        lv_obj_add_flag(sb, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(sb, go_settings_cb, LV_EVENT_CLICKED, NULL);
        mk_lbl(sb, LV_SYMBOL_SETTINGS, &lv_font_montserrat_20, C_GRAY,
               LV_ALIGN_CENTER, 0, 0);
    }

    /* ── Alerts bell (right, tap) ────────────────────────────────── */
    {
        lv_obj_t *ab = mk_cont(scr, 60, 60);
        lv_obj_align(ab, LV_ALIGN_CENTER, +180, -180);
        lv_obj_add_flag(ab, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ab, go_alerts_cb, LV_EVENT_CLICKED, NULL);
        mk_lbl(ab, LV_SYMBOL_BELL, &lv_font_montserrat_20, C_GRAY,
               LV_ALIGN_CENTER, 0, 0);
    }

    /* ── Solar tile (upper-left) ─────────────────────────────────── */
    {
        lv_obj_t *sc = mk_cont(scr, 120, 76);
        lv_obj_align(sc, LV_ALIGN_CENTER, -250, -70);
        lv_obj_add_flag(sc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(sc, go_solar_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *sr = mk_row(sc);
        lv_obj_align(sr, LV_ALIGN_TOP_MID, 0, 8);
        energy_icon_create(sr, 32, EICON_SOLAR);
        app.w_solar_val = lv_label_create(sr);
        lv_label_set_text(app.w_solar_val, "--");
        lv_obj_set_style_text_color(app.w_solar_val, C_SUN, 0);
        lv_obj_set_style_text_font(app.w_solar_val, &lv_font_montserrat_24, 0);

        mk_lbl(sc, "Solar", &lv_font_montserrat_14, C_GRAY,
               LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    /* ── AC Input tile (lower-left) ──────────────────────────────── */
    {
        lv_obj_t *gc = mk_cont(scr, 120, 76);
        lv_obj_align(gc, LV_ALIGN_CENTER, -250, +70);
        lv_obj_add_flag(gc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(gc, go_grid_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *gr = mk_row(gc);
        lv_obj_align(gr, LV_ALIGN_TOP_MID, 0, 8);
        energy_icon_create(gr, 32, EICON_GRID);
        app.w_grid_val = lv_label_create(gr);
        lv_label_set_text(app.w_grid_val, "--");
        lv_obj_set_style_text_color(app.w_grid_val, C_BLUE, 0);
        lv_obj_set_style_text_font(app.w_grid_val, &lv_font_montserrat_24, 0);

        app.w_grid_status = mk_lbl(gc, "Input", &lv_font_montserrat_14, C_GRAY,
                                   LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    /* ── Battery arc (center) ────────────────────────────────────── */
    {
        lv_obj_t *ba = mk_cont(scr, 244, 244);
        lv_obj_align(ba, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(ba, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ba, go_batt_cb, LV_EVENT_CLICKED, NULL);

        app.w_batt_arc = lv_arc_create(ba);
        lv_obj_set_size(app.w_batt_arc, 234, 234);
        lv_obj_center(app.w_batt_arc);
        lv_arc_set_rotation(app.w_batt_arc, 135);
        lv_arc_set_bg_angles(app.w_batt_arc, 0, 270);
        lv_arc_set_range(app.w_batt_arc, 0, 100);
        lv_arc_set_value(app.w_batt_arc, 0);
        lv_obj_set_style_arc_color(app.w_batt_arc, C_GREEN, LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(app.w_batt_arc, 16, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(app.w_batt_arc, lv_color_hex(0x122214), LV_PART_MAIN);
        lv_obj_set_style_arc_width(app.w_batt_arc, 16, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(app.w_batt_arc, LV_OPA_TRANSP, 0);
        lv_obj_set_style_opa(app.w_batt_arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_clear_flag(app.w_batt_arc, LV_OBJ_FLAG_CLICKABLE);

        app.w_batt_pct = lv_label_create(ba);
        lv_label_set_text(app.w_batt_pct, "--");
        lv_obj_set_style_text_color(app.w_batt_pct, C_WHITE, 0);
        lv_obj_set_style_text_font(app.w_batt_pct, &lv_font_montserrat_48, 0);
        lv_obj_align(app.w_batt_pct, LV_ALIGN_CENTER, 0, -22);

        app.w_batt_mode = lv_label_create(ba);
        lv_label_set_text(app.w_batt_mode, "Idle");
        lv_obj_set_style_text_color(app.w_batt_mode, C_GRAY, 0);
        lv_obj_set_style_text_font(app.w_batt_mode, &lv_font_montserrat_14, 0);
        lv_obj_align(app.w_batt_mode, LV_ALIGN_CENTER, 0, +24);

        app.w_batt_backup = lv_label_create(ba);
        lv_label_set_text(app.w_batt_backup, "--");
        lv_obj_set_style_text_color(app.w_batt_backup, C_GREEN, 0);
        lv_obj_set_style_text_font(app.w_batt_backup, &lv_font_montserrat_14, 0);
        lv_obj_align(app.w_batt_backup, LV_ALIGN_CENTER, 0, +46);
    }

    /* ── Load tile (upper-right) ─────────────────────────────────── */
    {
        lv_obj_t *lc = mk_cont(scr, 120, 76);
        lv_obj_align(lc, LV_ALIGN_CENTER, +250, -70);
        lv_obj_add_flag(lc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(lc, go_load_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *lr = mk_row(lc);
        lv_obj_align(lr, LV_ALIGN_TOP_MID, 0, 8);
        energy_icon_create(lr, 32, EICON_LOAD);
        app.w_load_val = lv_label_create(lr);
        lv_label_set_text(app.w_load_val, "--");
        lv_obj_set_style_text_color(app.w_load_val, C_BLUE, 0);
        lv_obj_set_style_text_font(app.w_load_val, &lv_font_montserrat_24, 0);

        mk_lbl(lc, "Load", &lv_font_montserrat_14, C_GRAY,
               LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    /* ── DC Discharge tile (lower-right, hidden when idle) ───────── */
    {
        app.w_dc_tile = mk_cont(scr, 120, 76);
        lv_obj_align(app.w_dc_tile, LV_ALIGN_CENTER, +250, +70);
        lv_obj_add_flag(app.w_dc_tile, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(app.w_dc_tile, go_batt_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *dr = mk_row(app.w_dc_tile);
        lv_obj_align(dr, LV_ALIGN_TOP_MID, 0, 8);
        energy_icon_create(dr, 32, EICON_DISCHARGE);
        app.w_dc_val = lv_label_create(dr);
        lv_label_set_text(app.w_dc_val, "--");
        lv_obj_set_style_text_color(app.w_dc_val, C_AMBER, 0);
        lv_obj_set_style_text_font(app.w_dc_val, &lv_font_montserrat_24, 0);

        mk_lbl(app.w_dc_tile, "DC Out", &lv_font_montserrat_14, C_GRAY,
               LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    /* ── Outdoor tile (bottom-left) → weather detail ─────────────── */
    {
        lv_obj_t *oc = mk_cont(scr, 132, 76);
        lv_obj_align(oc, LV_ALIGN_CENTER, -180, +180);
        lv_obj_add_flag(oc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(oc, go_wx_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *row = mk_row(oc);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 7);
        app.w_main_wx_icon = weather_icon_create(row, 18, 0);
        app.w_main_wx_tmp = lv_label_create(row);
        lv_label_set_text(app.w_main_wx_tmp, "--\xC2\xB0""C");
        lv_obj_set_style_text_color(app.w_main_wx_tmp, C_WHITE, 0);
        lv_obj_set_style_text_font(app.w_main_wx_tmp, &lv_font_montserrat_16, 0);

        lv_obj_t *aqrow = mk_row(oc);
        lv_obj_align(aqrow, LV_ALIGN_BOTTOM_MID, 0, -3);
        app.w_main_wx_aqi = lv_label_create(aqrow);
        lv_label_set_text(app.w_main_wx_aqi, "--");
        lv_obj_set_style_text_color(app.w_main_wx_aqi, C_GRAY, 0);
        lv_obj_set_style_text_font(app.w_main_wx_aqi, &lv_font_montserrat_12, 0);
        app.w_main_wx_aqi_cat = lv_label_create(aqrow);
        lv_label_set_text(app.w_main_wx_aqi_cat, " --");
        lv_obj_set_style_text_color(app.w_main_wx_aqi_cat, C_GRAY, 0);
        lv_obj_set_style_text_font(app.w_main_wx_aqi_cat, &lv_font_montserrat_12, 0);
    }

    /* ── Room tile (bottom-right) → room detail ──────────────────── */
    {
        lv_obj_t *rc = mk_cont(scr, 132, 76);
        lv_obj_align(rc, LV_ALIGN_CENTER, +180, +180);
        lv_obj_add_flag(rc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(rc, go_room_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *row = mk_row(rc);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 7);
        weather_icon_create(row, 18, 999); /* WX_ICON_TEMPERATURE */
        lv_obj_t *room_tmp = lv_label_create(row);
        lv_label_set_text(room_tmp, "24\xC2\xB0""C");
        lv_obj_set_style_text_color(room_tmp, C_WHITE, 0);
        lv_obj_set_style_text_font(room_tmp, &lv_font_montserrat_16, 0);
        app.w_main_wx_hum = NULL;

        mk_lbl(rc, "AQI 42 | Good", &lv_font_montserrat_12, C_GREEN,
               LV_ALIGN_BOTTOM_MID, 0, -3);
    }

    /* ── INVO logo ───────────────────────────────────────────────── */
    add_logo(scr, -22);

    /* ── Overload warning ring (hidden) ─────────────────────────── */
    app.w_warn_ring = lv_arc_create(scr);
    lv_obj_set_size(app.w_warn_ring, 248, 248);
    lv_obj_align(app.w_warn_ring, LV_ALIGN_CENTER, 0, 0);
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

    /* ── Overload modal (hidden) ─────────────────────────────────── */
    app.w_warn_dlg = lv_obj_create(scr);
    lv_obj_set_size(app.w_warn_dlg, 390, 210);
    lv_obj_align(app.w_warn_dlg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(app.w_warn_dlg, lv_color_hex(0x1A0808), 0);
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
