#ifndef INVO_WIFI_H
#define INVO_WIFI_H

#include "lvgl/lvgl.h"

/* Call from invo_screens_init() to create WiFi screens */
void invo_wifi_init(void);

/* Navigate to WiFi list and trigger a fresh scan */
void invo_wifi_show_list(void);

#endif
