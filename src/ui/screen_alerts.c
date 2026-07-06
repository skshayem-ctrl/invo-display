#include "ui_common.h"

/*
 * Circular 720px display — same 440px / y=88 geometry as screen_history.
 * All four corners of the list container are inside the circle radius 360px.
 */

static void alerts_row(lv_obj_t *list, const char *icon, lv_color_t border_col,
                        const char *title, const char *desc, const char *time_str)
{
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row, 66, 0);
    lv_obj_set_style_bg_color(row, C_CARD, 0);
    lv_obj_set_style_border_color(row, border_col, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_pad_all(row, 12, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *ic = lv_label_create(row);
    lv_label_set_text(ic, icon);
    lv_obj_set_style_text_color(ic, border_col, 0);
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_20, 0);
    lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *ts = lv_label_create(row);
    lv_label_set_text(ts, time_str);
    lv_obj_set_style_text_color(ts, C_GRAY, 0);
    lv_obj_set_style_text_font(ts, &lv_font_montserrat_12, 0);
    lv_obj_align(ts, LV_ALIGN_TOP_RIGHT, 0, 2);

    lv_obj_t *tl = lv_label_create(row);
    lv_label_set_text(tl, title);
    lv_obj_set_style_text_color(tl, C_WHITE, 0);
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_14, 0);
    lv_obj_align(tl, LV_ALIGN_TOP_LEFT, 30, 2);

    lv_obj_t *dl = lv_label_create(row);
    lv_label_set_text(dl, desc);
    lv_obj_set_style_text_color(dl, C_GRAY, 0);
    lv_obj_set_style_text_font(dl, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(dl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(dl, 310);
    lv_obj_align(dl, LV_ALIGN_TOP_LEFT, 30, 22);
}

lv_obj_t *screen_alerts_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    mk_lbl(scr, LV_SYMBOL_BELL, &lv_font_montserrat_20, C_AMBER,
           LV_ALIGN_TOP_MID, 0, 28);
    mk_lbl(scr, "Alerts", &lv_font_montserrat_16, C_GRAY,
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

    alerts_row(list, LV_SYMBOL_WARNING, C_RED,   "Overload",       "Load 3.6 kW exceeded 3.0 kW limit",  "07:30");
    alerts_row(list, LV_SYMBOL_WARNING, C_AMBER, "Low Battery",    "Battery dropped below 20%",           "06:45");
    alerts_row(list, LV_SYMBOL_CHARGE,  C_GREEN, "Solar Restored", "Generation resumed after cloud cover", "09:02");
    alerts_row(list, LV_SYMBOL_WIFI,    C_AMBER, "Grid Interrupt", "Grid lost, running on battery",       "05:48");

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
