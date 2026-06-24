#include "ui_common.h"

lv_obj_t *screen_solar_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    app.w_wifi_sd = add_detail_header(scr, "Solar Input");

    /* ── Left panel: icon / title / live kW ─────────────────────────── */
    lv_obj_t *icon = lv_label_create(scr);
    lv_label_set_text(icon, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(icon, C_ORANGE, 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_36, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, -215, -138);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Solar Input");
    lv_obj_set_style_text_color(title, C_GRAY, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, -215, -82);

    app.w_sd_kw = lv_label_create(scr);
    lv_label_set_text(app.w_sd_kw, "--");
    lv_obj_set_style_text_color(app.w_sd_kw, C_BLUE, 0);
    lv_obj_set_style_text_font(app.w_sd_kw, &lv_font_montserrat_24, 0);
    lv_obj_align(app.w_sd_kw, LV_ALIGN_CENTER, -215, -46);

    /* ── Vertical divider ────────────────────────────────────────────── */
    lv_obj_t *divider = lv_obj_create(scr);
    lv_obj_set_size(divider, 1, 230);
    lv_obj_align(divider, LV_ALIGN_CENTER, -122, -85);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* ── Right: 2×2 stat grid (195×100, CENTER offsets) ─────────────── */
    app.w_sd_grid_hz = make_stat_card(scr, 195, 100,  -18, -185, "Grid Hz",    "--", "AC input freq",  C_PURPLE, C_GRAY);
    app.w_sd_grid_v  = make_stat_card(scr, 195, 100,  192, -185, "Grid V",     "--", "Mains voltage",  C_PURPLE, C_GRAY);
    app.w_sd_volt    = make_stat_card(scr, 195, 100,  -18,  -70, "PV Voltage", "--", "Panel voltage",  C_ORANGE, C_GRAY);
    app.w_sd_cur     = make_stat_card(scr, 195, 100,  192,  -70, "PV Current", "--", "Panel current",  C_ORANGE, C_GRAY);

    /* ── Chart label ─────────────────────────────────────────────────── */
    lv_obj_t *ct = lv_label_create(scr);
    lv_label_set_text(ct, "POWER GENERATION  (TODAY)");
    lv_obj_set_style_text_color(ct, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(ct, &lv_font_montserrat_12, 0);
    lv_obj_align(ct, LV_ALIGN_CENTER, 0, 58);

    /* ── Live chart (555×125, CENTER,0,138) ──────────────────────────── */
    lv_obj_t *chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 555, 125);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 138);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x0d1a0d), 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chart, lv_color_hex(0x1e2e1e), 0);
    lv_obj_set_style_border_width(chart, 1, 0);
    lv_obj_set_style_radius(chart, 10, 0);
    lv_chart_set_div_line_count(chart, 3, 4);
    lv_obj_set_style_line_color(chart, lv_color_hex(0x1a2a1a), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 30);
    lv_chart_set_point_count(chart, 13);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_width(chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(chart, 0, LV_PART_INDICATOR);

    lv_chart_series_t *ser = lv_chart_add_series(chart, C_BLUE, LV_CHART_AXIS_PRIMARY_Y);
    for (int i = 0; i < 13; i++) lv_chart_set_next_value(chart, ser, 0);
    app.w_sd_chart = chart;
    app.w_sd_ser   = ser;

    add_logo(scr, -22);
    return scr;
}
