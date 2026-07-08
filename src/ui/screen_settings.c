#include "ui_common.h"

lv_obj_t *screen_settings_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    mk_lbl(scr, LV_SYMBOL_SETTINGS, &lv_font_montserrat_20, C_GRAY, LV_ALIGN_TOP_MID, 0, 28);
    mk_lbl(scr, "Settings", &lv_font_montserrat_16, C_GRAY, LV_ALIGN_TOP_MID, 0, 56);

    /* ── General Settings row ───────────────────────────────────────── */
    lv_obj_t *gen = lv_obj_create(scr);
    lv_obj_set_size(gen, 440, 60);
    lv_obj_align(gen, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_bg_color(gen, C_CARD, 0);
    lv_obj_set_style_border_color(gen, C_LINE, 0);
    lv_obj_set_style_border_width(gen, 1, 0);
    lv_obj_set_style_radius(gen, 12, 0);
    lv_obj_set_style_pad_hor(gen, 16, 0);
    lv_obj_set_style_pad_ver(gen, 0, 0);
    lv_obj_clear_flag(gen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(gen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gen, go_settings_general_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *gen_ic = lv_label_create(gen);
    lv_label_set_text(gen_ic, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gen_ic, C_BLUE, 0);
    lv_obj_set_style_text_font(gen_ic, &lv_font_montserrat_20, 0);
    lv_obj_align(gen_ic, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *gen_lbl = lv_label_create(gen);
    lv_label_set_text(gen_lbl, "General Settings");
    lv_obj_set_style_text_color(gen_lbl, C_WHITE, 0);
    lv_obj_set_style_text_font(gen_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(gen_lbl, LV_ALIGN_LEFT_MID, 36, 0);
    lv_obj_t *gen_arr = lv_label_create(gen);
    lv_label_set_text(gen_arr, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(gen_arr, C_GRAY, 0);
    lv_obj_set_style_text_font(gen_arr, &lv_font_montserrat_16, 0);
    lv_obj_align(gen_arr, LV_ALIGN_RIGHT_MID, 0, 0);

    /* ── Battery Settings row ───────────────────────────────────────── */
    lv_obj_t *bat = lv_obj_create(scr);
    lv_obj_set_size(bat, 440, 60);
    lv_obj_align(bat, LV_ALIGN_TOP_MID, 0, 172);
    lv_obj_set_style_bg_color(bat, C_CARD, 0);
    lv_obj_set_style_border_color(bat, C_LINE, 0);
    lv_obj_set_style_border_width(bat, 1, 0);
    lv_obj_set_style_radius(bat, 12, 0);
    lv_obj_set_style_pad_hor(bat, 16, 0);
    lv_obj_set_style_pad_ver(bat, 0, 0);
    lv_obj_clear_flag(bat, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(bat, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bat, go_batt_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bat_ic = lv_label_create(bat);
    lv_label_set_text(bat_ic, LV_SYMBOL_BATTERY_3);
    lv_obj_set_style_text_color(bat_ic, C_GREEN, 0);
    lv_obj_set_style_text_font(bat_ic, &lv_font_montserrat_20, 0);
    lv_obj_align(bat_ic, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *bat_lbl = lv_label_create(bat);
    lv_label_set_text(bat_lbl, "Battery Settings");
    lv_obj_set_style_text_color(bat_lbl, C_WHITE, 0);
    lv_obj_set_style_text_font(bat_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(bat_lbl, LV_ALIGN_LEFT_MID, 36, 0);
    lv_obj_t *bat_arr = lv_label_create(bat);
    lv_label_set_text(bat_arr, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(bat_arr, C_GRAY, 0);
    lv_obj_set_style_text_font(bat_arr, &lv_font_montserrat_16, 0);
    lv_obj_align(bat_arr, LV_ALIGN_RIGHT_MID, 0, 0);

    /* ── Back button ────────────────────────────────────────────────── */
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
