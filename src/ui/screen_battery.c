#include "ui_common.h"


lv_obj_t *screen_battery_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    app.w_wifi_bd = add_detail_header(scr, "Battery");

    /* 5 stat rows centred on the circle, 52 px apart              */
    /* w_bd_chg repurposed: shows Battery state text               */
    app.w_bd_chg  = mk_stat_row(scr, -104, "Battery state",   "--");
    app.w_bd_pct  = mk_stat_row(scr,  -52, "State of charge", "--");
    app.w_bd_tmp  = mk_stat_row(scr,    0, "Battery temp",    "--");
    app.w_bd_full = mk_stat_row(scr,  +52, "Total capacity",  "--");
    app.w_bd_bkp  = mk_stat_row(scr, +104, "Backup time",     "--");

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
