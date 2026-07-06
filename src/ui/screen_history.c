#include "ui_common.h"

/*
 * Circular 720px display — list at y=88, width=440px centred.
 * Corner (140, 88): dist=sqrt(220²+272²)=sqrt(122384)≈349.8 < 360 ✓
 * Corner (140, 588): dist=sqrt(220²+228²)=sqrt(100384)≈316.8 < 360 ✓
 */

static void history_row(lv_obj_t *list, const char *icon, lv_color_t icon_col,
                         const char *title, const char *detail, const char *time_str)
{
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row, 58, 0);
    lv_obj_set_style_bg_color(row, C_CARD, 0);
    lv_obj_set_style_border_color(row, C_LINE, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_pad_all(row, 12, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *ic = lv_label_create(row);
    lv_label_set_text(ic, icon);
    lv_obj_set_style_text_color(ic, icon_col, 0);
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_16, 0);
    lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 0, 2);

    lv_obj_t *ts = lv_label_create(row);
    lv_label_set_text(ts, time_str);
    lv_obj_set_style_text_color(ts, C_GRAY, 0);
    lv_obj_set_style_text_font(ts, &lv_font_montserrat_12, 0);
    lv_obj_align(ts, LV_ALIGN_TOP_RIGHT, 0, 2);

    lv_obj_t *tl = lv_label_create(row);
    lv_label_set_text(tl, title);
    lv_obj_set_style_text_color(tl, C_WHITE, 0);
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_14, 0);
    lv_obj_align(tl, LV_ALIGN_TOP_LEFT, 26, 0);

    lv_obj_t *dl = lv_label_create(row);
    lv_label_set_text(dl, detail);
    lv_obj_set_style_text_color(dl, C_GRAY, 0);
    lv_obj_set_style_text_font(dl, &lv_font_montserrat_12, 0);
    lv_obj_align(dl, LV_ALIGN_TOP_LEFT, 26, 20);
}

lv_obj_t *screen_history_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    mk_lbl(scr, LV_SYMBOL_LIST, &lv_font_montserrat_20, C_GRAY,
           LV_ALIGN_TOP_MID, 0, 28);
    mk_lbl(scr, "History", &lv_font_montserrat_16, C_GRAY,
           LV_ALIGN_TOP_MID, 0, 56);

    /* list — 440px wide keeps all corners inside the 360px-radius circle */
    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, 440, 500);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 88);
    lv_obj_set_style_bg_color(list, C_BG, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_layout(list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 8, 0);

    history_row(list, LV_SYMBOL_CHARGE,       C_GREEN, "Solar ON",       "2.4 kW generation started",   "09:12");
    history_row(list, LV_SYMBOL_BATTERY_FULL, C_GREEN, "Battery 100%",   "Fully charged",               "08:47");
    history_row(list, LV_SYMBOL_WARNING,      C_AMBER, "High Load",      "Load exceeded 2.8 kW",        "07:30");
    history_row(list, LV_SYMBOL_HOME,         C_BLUE,  "Grid Import",    "0.6 kW from grid",            "06:15");
    history_row(list, LV_SYMBOL_POWER,        C_GRAY,  "System Start",   "Boot completed",              "06:00");

    /* back button */
    lv_obj_t *back = lv_btn_create(scr);
    lv_obj_set_size(back, 110, 42);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -72);
    lv_obj_set_style_bg_color(back, C_DGRAY, 0);
    lv_obj_set_style_radius(back, 21, 0);
    lv_obj_add_event_cb(back, go_main_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *brow = mk_row(back);
    lv_obj_center(brow);
    lv_obj_t *ba = lv_label_create(brow);
    lv_label_set_text(ba, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(ba, C_WHITE, 0);
    lv_obj_set_style_text_font(ba, &lv_font_montserrat_16, 0);
    lv_obj_t *bbl = lv_label_create(brow);
    lv_label_set_text(bbl, "Home");
    lv_obj_set_style_text_color(bbl, C_WHITE, 0);
    lv_obj_set_style_text_font(bbl, &lv_font_montserrat_16, 0);

    return scr;
}
