#include "ui_common.h"

lv_obj_t *screen_weather_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    app.w_wifi_wxd = add_detail_header(scr, "Outdoor");

    /* ── Weather icon (canvas) ───────────────────────────────────── */
    app.w_wx_icon = weather_icon_create(scr, 60, 0); /* 0 = clear sky (WMO) */
    lv_obj_align(app.w_wx_icon, LV_ALIGN_CENTER, 0, -180);

    /* ── Big temperature ─────────────────────────────────────────── */
    app.w_wx_tmp = lv_label_create(scr);
    lv_label_set_text(app.w_wx_tmp, "--\xC2\xB0""C");
    lv_obj_set_style_text_color(app.w_wx_tmp, C_WHITE, 0);
    lv_obj_set_style_text_font(app.w_wx_tmp, &lv_font_montserrat_48, 0);
    lv_obj_align(app.w_wx_tmp, LV_ALIGN_CENTER, 0, -112);

    /* ── Condition + feels-like row ──────────────────────────────── */
    app.w_wx_cond = mk_lbl(scr, "--", &lv_font_montserrat_20, C_LTGRAY,
                           LV_ALIGN_CENTER, 0, -58);
    app.w_wx_feels = mk_lbl(scr, "Feels like --\xC2\xB0""C",
                            &lv_font_montserrat_14, C_GRAY,
                            LV_ALIGN_CENTER, 0, -30);

    /* ── AQI badge row ───────────────────────────────────────────── */
    {
        lv_obj_t *aqrow = mk_row(scr);
        lv_obj_align(aqrow, LV_ALIGN_CENTER, 0, -4);

        lv_obj_t *aqnum = lv_label_create(aqrow);
        lv_label_set_text(aqnum, "AQI --");
        lv_obj_set_style_text_color(aqnum, C_GREEN, 0);
        lv_obj_set_style_text_font(aqnum, &lv_font_montserrat_14, 0);
        app.w_wx_aq_cat = lv_label_create(aqrow);
        lv_label_set_text(app.w_wx_aq_cat, "· --");
        lv_obj_set_style_text_color(app.w_wx_aq_cat, C_GRAY, 0);
        lv_obj_set_style_text_font(app.w_wx_aq_cat, &lv_font_montserrat_14, 0);
        /* store the AQI number in aqval handle, reuse aqrow's first label */
        app.w_wx_aqval = aqnum;
    }

    /* ── 3 stat rows ─────────────────────────────────────────────── */
    app.w_wx_wind = mk_stat_row(scr,  +52, "Wind",       "--");
    app.w_wx_hum  = mk_stat_row(scr, +104, "Humidity",   "--");

    /* Null out unused handles from old design */
    app.w_wx_aqarc   = NULL;
    app.w_wx_aq_desc = NULL;
    app.w_wx_aqpm    = NULL;

    /* Forecast tiles removed — keep handles NULL (weather_service guards) */
    for (int i = 0; i < 7; i++) {
        app.w_fc_day[i]  = NULL;
        app.w_fc_icon[i] = NULL;
        app.w_fc_hi[i]   = NULL;
        app.w_fc_lo[i]   = NULL;
        app.w_fc_desc[i] = NULL;
    }

    add_logo(scr, -22);
    return scr;
}
