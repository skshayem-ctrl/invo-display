#include "ui_common.h"

lv_obj_t *screen_battery_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_batt_cb, LV_EVENT_GESTURE, NULL);

    app.w_wifi_bd = add_detail_header(scr, "Battery");

    /* ── Battery SOC arc ──────────────────────────────────────────── */
    {
        lv_obj_t *arc = lv_arc_create(scr);
        lv_obj_set_size(arc, 150, 150);
        lv_arc_set_rotation(arc, 135);
        lv_arc_set_bg_angles(arc, 0, 270);
        lv_arc_set_range(arc, 0, 100);
        lv_arc_set_value(arc, 0);
        lv_obj_set_style_arc_color(arc, C_DGRAY, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, C_GRAY, LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(arc, LV_ALIGN_CENTER, 0, -192);
        app.w_bd_arc = arc;

        /* SOC % centered inside the ring */
        app.w_bd_arc_val = lv_label_create(scr);
        lv_label_set_text(app.w_bd_arc_val, "--");
        lv_obj_set_style_text_color(app.w_bd_arc_val, C_WHITE, 0);
        lv_obj_set_style_text_font(app.w_bd_arc_val, &lv_font_montserrat_20, 0);
        lv_obj_align(app.w_bd_arc_val, LV_ALIGN_CENTER, 0, -192);
    }

    /* 5 stat rows shifted down to give space for the arc above */
    app.w_bd_chg  = mk_stat_row(scr,  -52, "Battery state",   "--");
    app.w_bd_pct  = mk_stat_row(scr,    0, "State of charge", "--");
    app.w_bd_tmp  = mk_stat_row(scr,  +52, "Battery temp",    "--");
    app.w_bd_full = mk_stat_row(scr, +104, "Total capacity",  "--");
    app.w_bd_bkp  = mk_stat_row(scr, +156, "Backup time",     "--");

    /* null everything removed from this screen */
    app.w_bd_batt_v     = NULL;
    app.w_bd_batt_a     = NULL;
    app.w_bd_grid_a     = NULL;
    app.w_bd_grid_chg_w = NULL;
    app.w_bd_inv_on     = NULL;
    app.w_bd_ac_chg     = NULL;
    app.w_bd_bypass     = NULL;
    app.w_bd_fault      = NULL;

    add_logo(scr, -22);
    return scr;
}
