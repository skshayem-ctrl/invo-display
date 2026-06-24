#include "ui_common.h"

lv_obj_t *screen_battery_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    app.w_wifi_bd = add_detail_header(scr, "Battery");

    /* ── Left: SOC card ────────────────────────────────────────── */
    lv_obj_t *soc = lv_obj_create(scr);
    lv_obj_set_pos(soc, 115, 100);
    lv_obj_set_size(soc, 165, 270);
    lv_obj_set_style_bg_color(soc, C_CARD, 0);
    lv_obj_set_style_border_color(soc, C_GREEN, 0);
    lv_obj_set_style_border_width(soc, 1, 0);
    lv_obj_set_style_radius(soc, 12, 0);
    lv_obj_set_style_pad_all(soc, 12, 0);
    lv_obj_clear_flag(soc, LV_OBJ_FLAG_SCROLLABLE);

    mk_lbl(soc, LV_SYMBOL_BATTERY_FULL, &lv_font_montserrat_36,
           C_GREEN, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *pct = lv_label_create(soc);
    lv_label_set_text_fmt(pct, "%d%%", gd.batt_pct);
    lv_obj_set_style_text_color(pct, C_WHITE, 0);
    lv_obj_set_style_text_font(pct, &lv_font_montserrat_36, 0);
    lv_obj_align(pct, LV_ALIGN_TOP_MID, 0, 52);
    app.w_bd_pct = pct;

    mk_lbl(soc, "STATE OF CHARGE", &lv_font_montserrat_12, C_GRAY,
           LV_ALIGN_TOP_MID, 0, 100);

    lv_obj_t *hc = lv_obj_create(soc);
    lv_obj_set_size(hc, 160, 34);
    lv_obj_align(hc, LV_ALIGN_TOP_MID, 0, 120);
    lv_obj_set_style_bg_color(hc, lv_color_hex(0x0A2010), 0);
    lv_obj_set_style_border_color(hc, C_GREEN, 0);
    lv_obj_set_style_border_width(hc, 1, 0);
    lv_obj_set_style_radius(hc, 8, 0);
    lv_obj_clear_flag(hc, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hr = mk_row(hc);
    lv_obj_center(hr);
    lv_obj_t *hic = lv_label_create(hr);
    lv_label_set_text(hic, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(hic, C_GREEN, 0);
    lv_obj_set_style_text_font(hic, &lv_font_montserrat_14, 0);
    lv_obj_t *htxt = lv_label_create(hr);
    lv_label_set_text(htxt, "GOOD HEALTH");
    lv_obj_set_style_text_color(htxt, C_GREEN, 0);
    lv_obj_set_style_text_font(htxt, &lv_font_montserrat_14, 0);

    mk_lbl(soc, "Battery is operating\nnormally.",
           &lv_font_montserrat_12, C_GRAY,
           LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ── Right: 2×3 metric grid ────────────────────────────────── */
    const struct {
        const char *ico; lv_color_t col;
        const char *title, *val, *sub;
    } cells[] = {
        {LV_SYMBOL_CHARGE,   C_BLUE,  "Battery Capacity", "10.2 kWh",  "Total Capacity"},
        {LV_SYMBOL_POWER,    C_BLUE,  "Usable Capacity",  "8.0 kWh",   "78% of total"},
        {LV_SYMBOL_OK,       C_GREEN, "Battery Health",   "92%",        "Excellent"},
        {LV_SYMBOL_REFRESH,  C_AMBER, "Cycle Count",      "312",        "Cycles"},
        {LV_SYMBOL_DOWNLOAD, C_BLUE,  "Remaining Life",   "8.2 Years",  "Est. remaining"},
        {LV_SYMBOL_DOWNLOAD, lv_color_hex(0xFF6B6B),
                                      "Battery Temp",     "26\xC2\xB0""C", "Optimal"},
    };
    for (int i = 0; i < 6; i++) {
        lv_obj_t *gc = mk_card(scr,
                290 + (i % 2) * 160,
                100 + (i / 2) * 92,
                152, 84,
                cells[i].ico, cells[i].col,
                cells[i].title, cells[i].val, cells[i].sub);
        if (i == 5) app.w_bd_tmp = lv_obj_get_child(gc, 2);
    }

    /* ── Bottom row: 3 stats ───────────────────────────────────── */
    const struct {
        const char *ico; lv_color_t col;
        const char *title, *val, *sub;
    } bot[] = {
        {LV_SYMBOL_CHARGE,   C_GREEN,              "Charging Power",    "2.1 kW",  "Charging"},
        {LV_SYMBOL_REFRESH,  C_BLUE,               "Backup Time",       "3h 45m",  "At current load"},
        {LV_SYMBOL_POWER,    lv_color_hex(0xBB86FC),"Next Full Charge", "1h 20m",  "Est. time remaining"},
    };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *bc = mk_card(scr,
                58 + i * 202, 382,
                192, 100,
                bot[i].ico, bot[i].col,
                bot[i].title, bot[i].val, bot[i].sub);
        lv_obj_t *bv = lv_obj_get_child(bc, 2);
        if (i == 0) app.w_bd_chg  = bv;
        if (i == 1) app.w_bd_bkp  = bv;
        if (i == 2) app.w_bd_full = bv;
    }

    add_logo(scr, -20);
    return scr;
}
