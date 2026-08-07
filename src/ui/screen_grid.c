#include "ui_common.h"

lv_obj_t *screen_grid_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_grid_cb, LV_EVENT_GESTURE, NULL);

    add_detail_header(scr, "AC Input");

    /* ── headroom progress bar ───────────────────────────────────── */
    /* Shows current AC input as a fraction of 3 kW nominal capacity */
    {
        lv_obj_t *bg = lv_obj_create(scr);
        lv_obj_set_size(bg, 400, 16);
        lv_obj_align(bg, LV_ALIGN_CENTER, 0, -190);
        lv_obj_set_style_bg_color(bg, C_DGRAY, 0);
        lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bg, 0, 0);
        lv_obj_set_style_radius(bg, 8, 0);
        lv_obj_set_style_pad_all(bg, 0, 0);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        app.w_gd_bar = lv_bar_create(bg);
        lv_obj_set_size(app.w_gd_bar, 400, 16);
        lv_obj_center(app.w_gd_bar);
        lv_bar_set_range(app.w_gd_bar, 0, 100);
        lv_bar_set_value(app.w_gd_bar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(app.w_gd_bar, C_DGRAY, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(app.w_gd_bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(app.w_gd_bar, 8, LV_PART_MAIN);
        lv_obj_set_style_bg_color(app.w_gd_bar, C_BLUE, LV_PART_INDICATOR);
        lv_obj_set_style_radius(app.w_gd_bar, 8, LV_PART_INDICATOR);
        lv_obj_clear_flag(app.w_gd_bar, LV_OBJ_FLAG_CLICKABLE);
    }

    /* ── bar label: "-- / 3.0 kW" — dynamic handle ─────────────── */
    app.w_gd_bar_lbl = mk_lbl(scr, "-- / 3.0 kW  (grid capacity)",
                               &lv_font_montserrat_12, C_GRAY,
                               LV_ALIGN_CENTER, 0, -166);

    /* ── 5 stat rows ─────────────────────────────────────────────── */
    app.w_gd_input  = mk_stat_row(scr, -104, "Input now",    "--");
    app.w_gd_v      = mk_stat_row(scr,  -52, "Grid voltage", "--");
    app.w_gd_hz     = mk_stat_row(scr,    0, "Grid freq",    "--");
    app.w_gd_state  = mk_stat_row(scr,  +52, "Battery state","--");
    app.w_gd_chg_w  = mk_stat_row(scr, +104, "Charge power", "--");

    add_logo(scr, -22);
    return scr;
}
