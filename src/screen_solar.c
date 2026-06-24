#include "ui_common.h"

lv_obj_t *screen_solar_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    app.w_wifi_sd = add_detail_header(scr, "Solar Input");

    /* ── Icon + live kW label ──────────────────────────────────── */
    lv_obj_t *ic_box = lv_obj_create(scr);
    lv_obj_set_size(ic_box, 76, 76);
    lv_obj_align(ic_box, LV_ALIGN_TOP_LEFT, 130, 95);
    lv_obj_set_style_bg_color(ic_box, lv_color_hex(0x0D1F12), 0);
    lv_obj_set_style_border_color(ic_box, lv_color_hex(0x1A3820), 0);
    lv_obj_set_style_border_width(ic_box, 1, 0);
    lv_obj_set_style_radius(ic_box, 12, 0);
    lv_obj_clear_flag(ic_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *ic = lv_label_create(ic_box);
    lv_label_set_text(ic, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(ic, C_AMBER, 0);
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_36, 0);
    lv_obj_center(ic);

    mk_lbl(scr, "Solar Input", &lv_font_montserrat_14, C_WHITE,
           LV_ALIGN_TOP_LEFT, 130, 181);
    app.w_sd_kw = mk_lbl(scr, "2.4 kw", &lv_font_montserrat_24, C_BLUE,
                         LV_ALIGN_TOP_LEFT, 130, 203);

    /* ── Stats grid (2×2) ──────────────────────────────────────── */
    const struct {
        const char *ico; lv_color_t col;
        const char *title, *val;
    } stats[] = {
        {LV_SYMBOL_DOWNLOAD, C_BLUE,  "Today's Energy", "12.6 kWh"},
        {LV_SYMBOL_LIST,     C_BLUE,  "This Month",     "156.4 kWh"},
        {LV_SYMBOL_CHARGE,   C_AMBER, "Voltage",        "380 V"},
        {LV_SYMBOL_POWER,    C_GREEN, "Current",        "6.3 A"},
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *sc = mk_card(scr,
                265 + (i % 2) * 170,
                100 + (i / 2) * 108,
                160, 98,
                stats[i].ico, stats[i].col,
                stats[i].title, stats[i].val, "");
        lv_obj_t *sv = lv_obj_get_child(sc, 2);
        if (i == 0) app.w_sd_kwh  = sv;
        if (i == 2) app.w_sd_volt = sv;
        if (i == 3) app.w_sd_cur  = sv;
    }

    /* ── Live power chart ──────────────────────────────────────── */
    add_hdiv(scr, 318, 580);
    mk_lbl(scr, "POWER GENERATION (LIVE)", &lv_font_montserrat_12,
           C_GRAY, LV_ALIGN_TOP_MID, 0, 324);

    lv_obj_t *chart = lv_chart_create(scr);
    lv_obj_set_pos(chart, 58, 340);
    lv_obj_set_size(chart, 604, 145);
    lv_obj_set_style_bg_color(chart, C_CARD, 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chart, C_LINE, 0);
    lv_obj_set_style_border_width(chart, 1, 0);
    lv_obj_set_style_radius(chart, 8, 0);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 4000);
    lv_chart_set_point_count(chart, 60);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);

    lv_chart_series_t *ser = lv_chart_add_series(
        chart, C_BLUE, LV_CHART_AXIS_PRIMARY_Y);
    app.w_sd_chart = chart;
    app.w_sd_ser   = ser;

    /* seed with current solar value so chart looks meaningful immediately */
    for (int i = 0; i < 60; i++)
        lv_chart_set_next_value(chart, ser, (lv_value_precise_t)(gd.solar_kw * 1000.0f));

    mk_lbl(scr, "2 MIN AGO", &lv_font_montserrat_12, C_GRAY,
           LV_ALIGN_TOP_LEFT, 58, 490);
    mk_lbl(scr, "NOW", &lv_font_montserrat_12, C_GRAY,
           LV_ALIGN_TOP_RIGHT, -58, 490);

    add_logo(scr, -22);
    return scr;
}
