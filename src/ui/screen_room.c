#include "ui_common.h"

lv_obj_t *screen_room_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_room_cb, LV_EVENT_GESTURE, NULL);

    add_detail_header(scr, "Room");

    lv_obj_t *therm = weather_icon_create(scr, 48, 999); /* WX_ICON_TEMPERATURE */
    lv_obj_align(therm, LV_ALIGN_CENTER, 0, -180);

    mk_lbl(scr, "24\xC2\xB0""C", &lv_font_montserrat_48, C_WHITE,
           LV_ALIGN_CENTER, 0, -112);

    mk_lbl(scr, "Comfortable", &lv_font_montserrat_20, C_GREEN,
           LV_ALIGN_CENTER, 0, -58);

    mk_lbl(scr, "Humidity 52%  |  feels comfortable",
           &lv_font_montserrat_14, C_GRAY,
           LV_ALIGN_CENTER, 0, -30);

    mk_lbl(scr, "AQI 42  |  Good", &lv_font_montserrat_14, C_GREEN,
           LV_ALIGN_CENTER, 0, -4);

    lv_obj_t *hum  = mk_stat_row(scr,  +52, "Humidity",  "52%");
    lv_obj_t *pm25 = mk_stat_row(scr, +104, "PM2.5",     "18 ug/m3");
    lv_obj_t *pm10 = mk_stat_row(scr, +156, "PM10",      "24 ug/m3");

    (void)hum; (void)pm25; (void)pm10;

    add_logo(scr, -22);
    return scr;
}
