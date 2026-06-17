#ifndef INVO_SCREENS_H
#define INVO_SCREENS_H

#include "lvgl/lvgl.h"

/* Colors shared across all screens */
#define C_BG        lv_color_hex(0x050a05)
#define C_GREEN     lv_color_hex(0x00e676)
#define C_BLUE      lv_color_hex(0x4fc3f7)
#define C_WHITE     lv_color_hex(0xffffff)
#define C_GRAY      lv_color_hex(0x666666)
#define C_DARKGRAY  lv_color_hex(0x1a1a1a)
#define C_ORANGE    lv_color_hex(0xffa500)
#define C_PURPLE    lv_color_hex(0xb388ff)

/* Weather icon fonts (FontAwesome 5 glyphs) */
extern lv_font_t lv_font_weather_20;
extern lv_font_t lv_font_weather_36;

/* UTF-8 encoded FA5 codepoints for weather icons */
#define WI_SUN       "\xEF\x86\x85"   /* fa-sun           ☀  */
#define WI_CLOUD_SUN "\xEF\x9B\x84"   /* fa-cloud-sun     ⛅  */
#define WI_CLOUD     "\xEF\x83\x82"   /* fa-cloud         ☁  */
#define WI_RAIN      "\xEF\x9C\xBD"   /* fa-cloud-rain    🌧  */
#define WI_SNOW      "\xEF\x8B\x9C"   /* fa-snowflake     ❄  */
#define WI_BOLT      "\xEF\x83\xA7"   /* fa-bolt          ⚡  */
#define WI_WIND      "\xEF\x9C\xAE"   /* fa-wind          🌬  */
#define WI_SMOG      "\xEF\x9D\x9F"   /* fa-smog          🌫  */
#define WI_THERM     "\xEF\x8B\x89"   /* fa-thermometer   🌡  */
#define WI_TINT      "\xEF\x81\x83"   /* fa-tint          💧  */

/* Initialize all screens */
void invo_screens_init(void);

/* Navigation */
void nav_to_home(void);
void nav_to_solar(void);
void nav_to_battery(void);
void nav_to_weather(void);
void nav_to_home_load(void);
void nav_to_wifi(void);

#endif