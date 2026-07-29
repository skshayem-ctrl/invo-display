#include "ui_common.h"

lv_obj_t *screen_solar_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    add_detail_header(scr, "Solar");

    /* 4 stat rows, symmetric around centre: -78, -26, +26, +78 */
    app.w_sd_kw   = mk_stat_row(scr,  -78, "Input now",      "--");
    app.w_sd_volt = mk_stat_row(scr,  -26, "PV Voltage",     "--");
    app.w_sd_cur  = mk_stat_row(scr,  +26, "PV Current",     "--");
    app.w_sd_kwh  = mk_stat_row(scr,  +78, "Today's yield",  "--");

    /* grid V/Hz now live on AC Input screen — null these handles */
    app.w_sd_grid_v  = NULL;
    app.w_sd_grid_hz = NULL;
    app.w_sd_chart = NULL;
    app.w_sd_ser   = NULL;

    add_logo(scr, -22);
    return scr;
}
