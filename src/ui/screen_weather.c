#include "ui_common.h"

lv_obj_t *screen_weather_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    app.w_wifi_wxd = add_detail_header(scr, "Environment");

    /* ── Weather Now ───────────────────────────────────────────── */
    lv_obj_t *wx = lv_obj_create(scr);
    lv_obj_set_pos(wx, 115, 98);
    lv_obj_set_size(wx, 240, 200);
    lv_obj_set_style_bg_color(wx, C_CARD, 0);
    lv_obj_set_style_border_color(wx, C_LINE, 0);
    lv_obj_set_style_border_width(wx, 1, 0);
    lv_obj_set_style_radius(wx, 12, 0);
    lv_obj_set_style_pad_all(wx, 12, 0);
    lv_obj_clear_flag(wx, LV_OBJ_FLAG_SCROLLABLE);

    mk_lbl(wx, "WEATHER NOW", &lv_font_montserrat_12, C_GRAY,
           LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *wtemp = lv_label_create(wx);
    lv_label_set_text(wtemp, "--\xC2\xB0""C");
    lv_obj_set_style_text_color(wtemp, C_WHITE, 0);
    lv_obj_set_style_text_font(wtemp, &lv_font_montserrat_36, 0);
    lv_obj_align(wtemp, LV_ALIGN_TOP_MID, 0, 18);
    app.w_wx_tmp = wtemp;

    app.w_wx_cond = mk_lbl(wx, "--", &lv_font_montserrat_12, C_GRAY,
                           LV_ALIGN_TOP_MID, 0, 72);
    app.w_wx_feels = mk_lbl(wx, LV_SYMBOL_GPS "  Feels like --\xC2\xB0""C",
                            &lv_font_montserrat_12, C_GRAY, LV_ALIGN_TOP_LEFT, 0, 96);
    app.w_wx_wind = mk_lbl(wx, LV_SYMBOL_REFRESH "  Wind -- km/h",
                           &lv_font_montserrat_12, C_GRAY, LV_ALIGN_TOP_LEFT, 0, 118);

    lv_obj_t *hum = lv_label_create(wx);
    lv_label_set_text(hum, LV_SYMBOL_DOWNLOAD "  Humidity --");
    lv_obj_set_style_text_color(hum, C_GRAY, 0);
    lv_obj_set_style_text_font(hum, &lv_font_montserrat_12, 0);
    lv_obj_align(hum, LV_ALIGN_TOP_LEFT, 0, 140);
    app.w_wx_hum = hum;

    /* ── Air Quality ───────────────────────────────────────────── */
    lv_obj_t *aq = lv_obj_create(scr);
    lv_obj_set_pos(aq, 365, 98);
    lv_obj_set_size(aq, 240, 200);
    lv_obj_set_style_bg_color(aq, C_CARD, 0);
    lv_obj_set_style_border_color(aq, C_LINE, 0);
    lv_obj_set_style_border_width(aq, 1, 0);
    lv_obj_set_style_radius(aq, 12, 0);
    lv_obj_set_style_pad_all(aq, 12, 0);
    lv_obj_clear_flag(aq, LV_OBJ_FLAG_SCROLLABLE);

    mk_lbl(aq, "AIR QUALITY", &lv_font_montserrat_12, C_GRAY,
           LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *aqarc = lv_arc_create(aq);
    lv_obj_set_size(aqarc, 80, 80);
    lv_obj_align(aqarc, LV_ALIGN_TOP_MID, 0, 16);
    lv_arc_set_rotation(aqarc, 135);
    lv_arc_set_bg_angles(aqarc, 0, 270);
    lv_arc_set_range(aqarc, 0, 300);
    lv_arc_set_value(aqarc, 0);
    lv_obj_set_style_arc_color(aqarc, C_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(aqarc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(aqarc, C_DGRAY, LV_PART_MAIN);
    lv_obj_set_style_arc_width(aqarc, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(aqarc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_size(aqarc, 0, 0, LV_PART_KNOB);
    lv_obj_clear_flag(aqarc, LV_OBJ_FLAG_CLICKABLE);
    app.w_wx_aqarc = aqarc;

    lv_obj_t *aqval = lv_label_create(aqarc);
    lv_label_set_text(aqval, "--");
    lv_obj_set_style_text_color(aqval, C_WHITE, 0);
    lv_obj_set_style_text_font(aqval, &lv_font_montserrat_20, 0);
    lv_obj_center(aqval);
    app.w_wx_aqval = aqval;

    app.w_wx_aq_cat  = mk_lbl(aq, "--", &lv_font_montserrat_14, C_GRAY,
                              LV_ALIGN_TOP_MID, 0, 102);
    app.w_wx_aq_desc = mk_lbl(aq, "--",
                              &lv_font_montserrat_12, C_GRAY, LV_ALIGN_TOP_MID, 0, 122);
    app.w_wx_aqpm    = mk_lbl(aq, "PM2.5 --  PM10 --",
                              &lv_font_montserrat_12, C_GRAY, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ── 7-day forecast ────────────────────────────────────────── */
    add_hdiv(scr, 308, 608);
    mk_lbl(scr, "7-DAY WEATHER FORECAST", &lv_font_montserrat_12,
           C_GRAY, LV_ALIGN_TOP_MID, 0, 315);

    static const char *day_init[] =
        {"Today","Day 1","Day 2","Day 3","Day 4","Day 5","Day 6"};

    for (int i = 0; i < 7; i++) {
        lv_obj_t *dc = lv_obj_create(scr);
        lv_obj_set_size(dc, 84, 150);
        lv_obj_set_pos(dc, 51 + i * 89, 333);
        lv_obj_set_style_bg_color(dc, C_CARD, 0);
        lv_obj_set_style_bg_opa(dc, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(dc, C_LINE, 0);
        lv_obj_set_style_border_width(dc, 1, 0);
        lv_obj_set_style_radius(dc, 8, 0);
        lv_obj_set_style_pad_all(dc, 0, 0);
        lv_obj_clear_flag(dc, LV_OBJ_FLAG_SCROLLABLE);

        app.w_fc_day[i] = lv_label_create(dc);
        lv_label_set_text(app.w_fc_day[i], day_init[i]);
        lv_obj_set_style_text_color(app.w_fc_day[i], C_GRAY, 0);
        lv_obj_set_style_text_font(app.w_fc_day[i], &lv_font_montserrat_12, 0);
        lv_obj_align(app.w_fc_day[i], LV_ALIGN_TOP_MID, 0, 6);

        /* canvas weather icon — 38×38 fits the 84px tile */
        app.w_fc_icon[i] = weather_icon_create(dc, 38, 0);
        lv_obj_align(app.w_fc_icon[i], LV_ALIGN_TOP_MID, 0, 22);

        app.w_fc_hi[i] = lv_label_create(dc);
        lv_label_set_text(app.w_fc_hi[i], "--\xC2\xB0""C");
        lv_obj_set_style_text_color(app.w_fc_hi[i], C_WHITE, 0);
        lv_obj_set_style_text_font(app.w_fc_hi[i], &lv_font_montserrat_14, 0);
        lv_obj_align(app.w_fc_hi[i], LV_ALIGN_TOP_MID, 0, 64);

        app.w_fc_lo[i] = lv_label_create(dc);
        lv_label_set_text(app.w_fc_lo[i], "--\xC2\xB0""C");
        lv_obj_set_style_text_color(app.w_fc_lo[i], C_BLUE, 0);
        lv_obj_set_style_text_font(app.w_fc_lo[i], &lv_font_montserrat_14, 0);
        lv_obj_align(app.w_fc_lo[i], LV_ALIGN_TOP_MID, 0, 86);

        app.w_fc_desc[i] = lv_label_create(dc);
        lv_label_set_text(app.w_fc_desc[i], "--");
        lv_obj_set_style_text_color(app.w_fc_desc[i], C_GRAY, 0);
        lv_obj_set_style_text_font(app.w_fc_desc[i], &lv_font_montserrat_12, 0);
        lv_label_set_long_mode(app.w_fc_desc[i], LV_LABEL_LONG_WRAP);
        lv_obj_set_width(app.w_fc_desc[i], 84);
        lv_obj_set_style_text_align(app.w_fc_desc[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(app.w_fc_desc[i], LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    add_logo(scr, -22);
    return scr;
}
