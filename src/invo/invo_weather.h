#ifndef INVO_WEATHER_H
#define INVO_WEATHER_H
#include "lvgl/lvgl.h"
void invo_weather_init(lv_obj_t * scr);
void invo_weather_refresh(void);
void invo_weather_sync_clock(void);
void invo_weather_register_home(lv_obj_t * temp, lv_obj_t * city,
                                 lv_obj_t * aqi_val, lv_obj_t * aqi_lbl);
#endif
