#include "ui_common.h"

lv_obj_t *screen_solar_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    app.w_wifi_sd = add_detail_header(scr, "Solar Input");

    /* ── Left panel: icon + title + live kW (centre-relative) ──────── */
    lv_obj_t *ic_box = lv_obj_create(scr);
    lv_obj_set_size(ic_box, 80, 80);
    lv_obj_align(ic_box, LV_ALIGN_CENTER, -215, -140);
    lv_obj_set_style_bg_color(ic_box, lv_color_hex(0x0D1F12), 0);
    lv_obj_set_style_border_color(ic_box, lv_color_hex(0x1A3820), 0);
    lv_obj_set_style_border_width(ic_box, 1, 0);
    lv_obj_set_style_radius(ic_box, 14, 0);
    lv_obj_clear_flag(ic_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *ic = lv_label_create(ic_box);
    lv_label_set_text(ic, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(ic, C_AMBER, 0);
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_36, 0);
    lv_obj_center(ic);

    mk_lbl(scr, "Solar Input", &lv_font_montserrat_16, C_GRAY,
           LV_ALIGN_CENTER, -215, -72);
    app.w_sd_kw = mk_lbl(scr, "2.4 kw", &lv_font_montserrat_24, C_AMBER,
                         LV_ALIGN_CENTER, -215, -42);

    /* ── Vertical divider ─────────────────────────────────────────── */
    lv_obj_t *vdiv = lv_obj_create(scr);
    lv_obj_set_size(vdiv, 1, 240);
    lv_obj_align(vdiv, LV_ALIGN_CENTER, -118, -82);
    lv_obj_set_style_bg_color(vdiv, C_LINE, 0);
    lv_obj_set_style_bg_opa(vdiv, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vdiv, 0, 0);
    lv_obj_set_style_pad_all(vdiv, 0, 0);
    lv_obj_clear_flag(vdiv, LV_OBJ_FLAG_CLICKABLE);

    /* ── Right: 2×2 stat cards (195×100, centre-relative) ──────────── */
    const struct {
        const char *ico;
        lv_color_t  col;
        const char *title, *init_val;
        lv_obj_t  **handle;
    } stats[] = {
        {LV_SYMBOL_DOWNLOAD, C_BLUE,  "Today's Energy", "12.6 kWh", &app.w_sd_kwh},
        {LV_SYMBOL_LIST,     C_BLUE,  "This Month",     "156.4 kWh", NULL},
        {LV_SYMBOL_CHARGE,   C_AMBER, "Voltage",        "380 V",    &app.w_sd_volt},
        {LV_SYMBOL_POWER,    C_GREEN, "Current",        "6.3 A",    &app.w_sd_cur},
    };
    const int sxoffs[2] = {-18, 192};
    const int syoffs[2] = {-185, -70};

    for (int i = 0; i < 4; i++) {
        int col = i % 2, row = i / 2;

        lv_obj_t *c = lv_obj_create(scr);
        lv_obj_set_size(c, 195, 100);
        lv_obj_align(c, LV_ALIGN_CENTER, sxoffs[col], syoffs[row]);
        lv_obj_set_style_bg_color(c, C_CARD, 0);
        lv_obj_set_style_border_color(c, C_LINE, 0);
        lv_obj_set_style_border_width(c, 1, 0);
        lv_obj_set_style_radius(c, 10, 0);
        lv_obj_set_style_pad_all(c, 8, 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *ico = lv_label_create(c);
        lv_label_set_text(ico, stats[i].ico);
        lv_obj_set_style_text_color(ico, stats[i].col, 0);
        lv_obj_set_style_text_font(ico, &lv_font_montserrat_14, 0);
        lv_obj_align(ico, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *ttl = lv_label_create(c);
        lv_label_set_text(ttl, stats[i].title);
        lv_obj_set_style_text_color(ttl, C_GRAY, 0);
        lv_obj_set_style_text_font(ttl, &lv_font_montserrat_12, 0);
        lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, 20, 2);

        lv_obj_t *val = lv_label_create(c);
        lv_label_set_text(val, stats[i].init_val);
        lv_obj_set_style_text_color(val, C_WHITE, 0);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
        lv_obj_align(val, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        if (stats[i].handle) *stats[i].handle = val;
    }

    /* ── Live power chart (centre-relative) ───────────────────────── */
    mk_lbl(scr, "POWER GENERATION (LIVE)", &lv_font_montserrat_12,
           C_GRAY, LV_ALIGN_CENTER, 0, 100);

    lv_obj_t *chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 555, 125);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 140);
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

    for (int i = 0; i < 60; i++)
        lv_chart_set_next_value(chart, ser, (lv_value_precise_t)(gd.solar_kw * 1000.0f));

    mk_lbl(scr, "2 MIN AGO", &lv_font_montserrat_12, C_GRAY,
           LV_ALIGN_CENTER, -248, 210);
    mk_lbl(scr, "NOW", &lv_font_montserrat_12, C_GRAY,
           LV_ALIGN_CENTER, 248, 210);

    return scr;
}
