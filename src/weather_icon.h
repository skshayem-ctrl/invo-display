#pragma once
#include "lvgl.h"

/*
 * Create a canvas-drawn weather icon for the given WMO weather code.
 * Returns an lv_canvas widget sized sz×sz.
 * The caller owns the canvas and its pixel buffer — both live as long as
 * the parent object.
 */
lv_obj_t *weather_icon_create(lv_obj_t *parent, int sz, int wmo_code);

/* Redraw an existing icon canvas with a new WMO code. */
void weather_icon_update(lv_obj_t *icon, int wmo_code);

/* Virtual codes for fixed home-screen indicator icons */
#define WX_ICON_TEMPERATURE  999   /* thermometer */
#define WX_ICON_HUMIDITY     998   /* water drop  */
#define WX_ICON_AQI          997   /* leaf / air  */
