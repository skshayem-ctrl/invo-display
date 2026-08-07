#include "ui_common.h"

lv_obj_t *screen_solar_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_solar_cb, LV_EVENT_GESTURE, NULL);

    add_detail_header(scr, "Solar");

    /* ── Solar generation arc ─────────────────────────────────────── */
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
        app.w_sd_arc = arc;

        /* kW value centered inside the ring */
        app.w_sd_arc_val = lv_label_create(scr);
        lv_label_set_text(app.w_sd_arc_val, "--");
        lv_obj_set_style_text_color(app.w_sd_arc_val, C_WHITE, 0);
        lv_obj_set_style_text_font(app.w_sd_arc_val, &lv_font_montserrat_20, 0);
        lv_obj_align(app.w_sd_arc_val, LV_ALIGN_CENTER, 0, -192);
    }

    /* 4 stat rows shifted down to give space for the arc above */
    app.w_sd_kw   = mk_stat_row(scr,  -52, "Input now",     "--");
    app.w_sd_volt = mk_stat_row(scr,    0, "PV Voltage",    "--");
    app.w_sd_cur  = mk_stat_row(scr,  +52, "PV Current",    "--");
    app.w_sd_kwh  = mk_stat_row(scr, +104, "Today's yield", "--");

    app.w_sd_grid_v  = NULL;
    app.w_sd_grid_hz = NULL;
    app.w_sd_chart   = NULL;
    app.w_sd_ser     = NULL;

    add_logo(scr, -22);
    return scr;
}
