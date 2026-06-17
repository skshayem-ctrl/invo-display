#include "invo_home.h"
#include "invo_screens.h"
#include "invo_uart.h"
#include "invo_weather.h"
#include <time.h>

static lv_obj_t * time_label;
static lv_obj_t * date_label;
static lv_obj_t * battery_arc;
static lv_obj_t * battery_pct_label;
static lv_obj_t * backup_label;


static void clock_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    time_t now = time(NULL);
    struct tm * t = localtime(&now);
    char time_buf[16], date_buf[32];
    strftime(time_buf, sizeof(time_buf), "%I:%M %p", t);
    strftime(date_buf, sizeof(date_buf), "%A, %B %d", t);
    lv_label_set_text(time_label, time_buf);
    lv_label_set_text(date_label, date_buf);
}

/* Click callbacks */
static void solar_click_cb(lv_event_t * e)     { LV_UNUSED(e); nav_to_solar();     }
static void batt_click_cb(lv_event_t * e)      { LV_UNUSED(e); nav_to_battery();   }
static void weather_click_cb(lv_event_t * e)   { LV_UNUSED(e); nav_to_weather();   }
static void home_load_click_cb(lv_event_t * e) { LV_UNUSED(e); nav_to_home_load(); }
static void wifi_click_cb(lv_event_t * e)      { LV_UNUSED(e); nav_to_wifi();      }

/* Helper: invisible click zone */
static lv_obj_t * make_click_zone(lv_obj_t * scr, int w, int h, int x, int y, lv_event_cb_t cb) {
    lv_obj_t * zone = lv_obj_create(scr);
    lv_obj_set_size(zone, w, h);
    lv_obj_align(zone, LV_ALIGN_CENTER, x, y);
    lv_obj_set_style_bg_opa(zone, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(zone, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(zone, cb, LV_EVENT_CLICKED, NULL);
    return zone;
}

void invo_home_screen_create(lv_obj_t * scr)
{
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* WiFi button — taps navigate to WiFi screen */
    lv_obj_t * wifi_btn = lv_button_create(scr);
    lv_obj_set_size(wifi_btn, 52, 52);
    lv_obj_align(wifi_btn, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_style_bg_opa(wifi_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(wifi_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(wifi_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(wifi_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(wifi_btn, C_GREEN, LV_STATE_PRESSED);
    lv_obj_add_event_cb(wifi_btn, wifi_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * wifi = lv_label_create(wifi_btn);
    lv_label_set_text(wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi, C_GREEN, 0);
    lv_obj_set_style_text_font(wifi, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_opa(wifi, LV_OPA_60, 0);
    lv_obj_center(wifi);

    /* Time */
    time_label = lv_label_create(scr);
    lv_obj_set_style_text_color(time_label, C_WHITE, 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_40, 0);
    lv_label_set_text(time_label, "00:00 AM");
    lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 104);

    /* Date */
    date_label = lv_label_create(scr);
    lv_obj_set_style_text_color(date_label, C_GRAY, 0);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
    lv_label_set_text(date_label, "Wednesday, June 1");
    lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 152);

    /* Nav items: icon + label */
    const char * nav_icons[]  = { LV_SYMBOL_SETTINGS, LV_SYMBOL_LIST, LV_SYMBOL_BELL };
    const char * nav_labels[] = { "Settings",          "History",      "Alerts"       };
    int nav_x[] = { -220, 0, 220 };
    for(int i = 0; i < 3; i++) {
        lv_obj_t * ico = lv_label_create(scr);
        lv_label_set_text(ico, nav_icons[i]);
        lv_obj_set_style_text_color(ico, C_GRAY, 0);
        lv_obj_set_style_text_font(ico, &lv_font_montserrat_18, 0);
        lv_obj_align(ico, LV_ALIGN_TOP_MID, nav_x[i], 207);

        lv_obj_t * txt = lv_label_create(scr);
        lv_label_set_text(txt, nav_labels[i]);
        lv_obj_set_style_text_color(txt, C_GRAY, 0);
        lv_obj_set_style_text_font(txt, &lv_font_montserrat_12, 0);
        lv_obj_align(txt, LV_ALIGN_TOP_MID, nav_x[i], 229);
    }

    /* Battery arc */
    battery_arc = lv_arc_create(scr);
    lv_obj_set_size(battery_arc, 240, 240);
    lv_arc_set_rotation(battery_arc, 270);
    lv_arc_set_bg_angles(battery_arc, 0, 360);
    lv_arc_set_value(battery_arc, 0);
    lv_arc_set_range(battery_arc, 0, 100);
    lv_obj_set_style_arc_color(battery_arc, C_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(battery_arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(battery_arc, lv_color_hex(0x1a2a1a), LV_PART_MAIN);
    lv_obj_set_style_arc_width(battery_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(battery_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(battery_arc, 0, LV_PART_KNOB);
    lv_obj_remove_flag(battery_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(battery_arc, LV_ALIGN_CENTER, 0, 0);

    /* Battery % */
    battery_pct_label = lv_label_create(scr);
    lv_obj_set_style_text_font(battery_pct_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(battery_pct_label, C_WHITE, 0);
    lv_label_set_text(battery_pct_label, "0%");
    lv_obj_align(battery_pct_label, LV_ALIGN_CENTER, 0, -15);

    lv_obj_t * batt_title = lv_label_create(scr);
    lv_label_set_text(batt_title, "BATTERY");
    lv_obj_set_style_text_color(batt_title, C_GRAY, 0);
    lv_obj_set_style_text_font(batt_title, &lv_font_montserrat_12, 0);
    lv_obj_align(batt_title, LV_ALIGN_CENTER, 0, 15);

    backup_label = lv_label_create(scr);
    lv_label_set_text(backup_label, "--");
    lv_obj_set_style_text_color(backup_label, C_GREEN, 0);
    lv_obj_set_style_text_font(backup_label, &lv_font_montserrat_16, 0);
    lv_obj_align(backup_label, LV_ALIGN_CENTER, 0, 35);

    /* ── Solar card ── */
    lv_obj_t * solar_card = lv_obj_create(scr);
    lv_obj_set_size(solar_card, 164, 140);
    lv_obj_align(solar_card, LV_ALIGN_CENTER, -244, 0);
    lv_obj_set_style_bg_color(solar_card, lv_color_hex(0x0a1a08), 0);
    lv_obj_set_style_border_color(solar_card, lv_color_hex(0x1e3a18), 0);
    lv_obj_set_style_border_width(solar_card, 1, 0);
    lv_obj_set_style_radius(solar_card, 14, 0);
    lv_obj_set_style_pad_all(solar_card, 8, 0);
    lv_obj_remove_flag(solar_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(solar_card, solar_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * solar_icon = lv_label_create(solar_card);
    lv_label_set_text(solar_icon, WI_SUN);
    lv_obj_set_style_text_color(solar_icon, C_ORANGE, 0);
    lv_obj_set_style_text_font(solar_icon, &lv_font_weather_36, 0);
    lv_obj_align(solar_icon, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t * solar_lbl = lv_label_create(solar_card);
    lv_label_set_text(solar_lbl, "Solar Input");
    lv_obj_set_style_text_color(solar_lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(solar_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(solar_lbl, LV_ALIGN_CENTER, 0, 10);

    static lv_obj_t * solar_val;
    solar_val = lv_label_create(solar_card);
    lv_label_set_text(solar_val, "0.0 kW");
    lv_obj_set_style_text_color(solar_val, C_GREEN, 0);
    lv_obj_set_style_text_font(solar_val, &lv_font_montserrat_20, 0);
    lv_obj_align(solar_val, LV_ALIGN_BOTTOM_MID, 0, -2);

    /* ── Home Load card ── */
    lv_obj_t * load_card = lv_obj_create(scr);
    lv_obj_set_size(load_card, 164, 140);
    lv_obj_align(load_card, LV_ALIGN_CENTER, 244, 0);
    lv_obj_set_style_bg_color(load_card, lv_color_hex(0x08101a), 0);
    lv_obj_set_style_border_color(load_card, lv_color_hex(0x182838), 0);
    lv_obj_set_style_border_width(load_card, 1, 0);
    lv_obj_set_style_radius(load_card, 14, 0);
    lv_obj_set_style_pad_all(load_card, 8, 0);
    lv_obj_remove_flag(load_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(load_card, home_load_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * home_icon = lv_label_create(load_card);
    lv_label_set_text(home_icon, LV_SYMBOL_HOME);
    lv_obj_set_style_text_color(home_icon, C_BLUE, 0);
    lv_obj_set_style_text_font(home_icon, &lv_font_montserrat_36, 0);
    lv_obj_align(home_icon, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t * home_lbl = lv_label_create(load_card);
    lv_label_set_text(home_lbl, "Home Load");
    lv_obj_set_style_text_color(home_lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(home_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(home_lbl, LV_ALIGN_CENTER, 0, 10);

    static lv_obj_t * load_val;
    load_val = lv_label_create(load_card);
    lv_label_set_text(load_val, "0.0 kW");
    lv_obj_set_style_text_color(load_val, C_BLUE, 0);
    lv_obj_set_style_text_font(load_val, &lv_font_montserrat_20, 0);
    lv_obj_align(load_val, LV_ALIGN_BOTTOM_MID, 0, -2);

    invo_uart_register_home(solar_val, load_val);
    invo_uart_register_home_battery(battery_arc, battery_pct_label, backup_label);

    /* ── Bottom row: 3 info cards ── */
    /* Weather card */
    lv_obj_t * wx_card = lv_obj_create(scr);
    lv_obj_set_size(wx_card, 126, 90);
    lv_obj_align(wx_card, LV_ALIGN_CENTER, -182, 200);
    lv_obj_set_style_bg_color(wx_card, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_color(wx_card, lv_color_hex(0x282828), 0);
    lv_obj_set_style_border_width(wx_card, 1, 0);
    lv_obj_set_style_radius(wx_card, 12, 0);
    lv_obj_set_style_pad_all(wx_card, 6, 0);
    lv_obj_remove_flag(wx_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(wx_card, weather_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * wx_ico = lv_label_create(wx_card);
    lv_label_set_text(wx_ico, WI_THERM);
    lv_obj_set_style_text_color(wx_ico, C_ORANGE, 0);
    lv_obj_set_style_text_font(wx_ico, &lv_font_weather_20, 0);
    lv_obj_align(wx_ico, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * temp_lbl = lv_label_create(wx_card);
    lv_label_set_text(temp_lbl, "--");
    lv_obj_set_style_text_color(temp_lbl, C_WHITE, 0);
    lv_obj_set_style_text_font(temp_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(temp_lbl, LV_ALIGN_CENTER, 0, 6);

    lv_obj_t * city_lbl = lv_label_create(wx_card);
    lv_label_set_text(city_lbl, "--");
    lv_obj_set_style_text_color(city_lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(city_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(city_lbl, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* AQI card */
    lv_obj_t * aqi_card = lv_obj_create(scr);
    lv_obj_set_size(aqi_card, 126, 90);
    lv_obj_align(aqi_card, LV_ALIGN_CENTER, 0, 200);
    lv_obj_set_style_bg_color(aqi_card, lv_color_hex(0x0a1a0a), 0);
    lv_obj_set_style_border_color(aqi_card, lv_color_hex(0x1a2e1a), 0);
    lv_obj_set_style_border_width(aqi_card, 1, 0);
    lv_obj_set_style_radius(aqi_card, 12, 0);
    lv_obj_set_style_pad_all(aqi_card, 6, 0);
    lv_obj_remove_flag(aqi_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(aqi_card, weather_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * aqi_ico = lv_label_create(aqi_card);
    lv_label_set_text(aqi_ico, WI_SMOG);
    lv_obj_set_style_text_color(aqi_ico, C_GREEN, 0);
    lv_obj_set_style_text_font(aqi_ico, &lv_font_weather_20, 0);
    lv_obj_align(aqi_ico, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * aqi_val = lv_label_create(aqi_card);
    lv_label_set_text(aqi_val, "--");
    lv_obj_set_style_text_color(aqi_val, C_WHITE, 0);
    lv_obj_set_style_text_font(aqi_val, &lv_font_montserrat_14, 0);
    lv_obj_align(aqi_val, LV_ALIGN_CENTER, 0, 6);

    lv_obj_t * aqi_lbl = lv_label_create(aqi_card);
    lv_label_set_text(aqi_lbl, "--");
    lv_obj_set_style_text_color(aqi_lbl, C_GREEN, 0);
    lv_obj_set_style_text_font(aqi_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(aqi_lbl, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* Room temp card */
    lv_obj_t * room_card = lv_obj_create(scr);
    lv_obj_set_size(room_card, 126, 90);
    lv_obj_align(room_card, LV_ALIGN_CENTER, 182, 200);
    lv_obj_set_style_bg_color(room_card, lv_color_hex(0x08101a), 0);
    lv_obj_set_style_border_color(room_card, lv_color_hex(0x182838), 0);
    lv_obj_set_style_border_width(room_card, 1, 0);
    lv_obj_set_style_radius(room_card, 12, 0);
    lv_obj_set_style_pad_all(room_card, 6, 0);
    lv_obj_remove_flag(room_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(room_card, weather_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * room_ico = lv_label_create(room_card);
    lv_label_set_text(room_ico, WI_THERM);
    lv_obj_set_style_text_color(room_ico, C_BLUE, 0);
    lv_obj_set_style_text_font(room_ico, &lv_font_weather_20, 0);
    lv_obj_align(room_ico, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * room_lbl = lv_label_create(room_card);
    lv_label_set_text(room_lbl, "--");
    lv_obj_set_style_text_color(room_lbl, C_WHITE, 0);
    lv_obj_set_style_text_font(room_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(room_lbl, LV_ALIGN_CENTER, 0, 6);

    lv_obj_t * room_title = lv_label_create(room_card);
    lv_label_set_text(room_title, "Room Temp");
    lv_obj_set_style_text_color(room_title, C_GRAY, 0);
    lv_obj_set_style_text_font(room_title, &lv_font_montserrat_12, 0);
    lv_obj_align(room_title, LV_ALIGN_BOTTOM_MID, 0, 0);

    invo_weather_register_home(temp_lbl, city_lbl, aqi_val, aqi_lbl);

    /* ── INVO logo (INV green + O orange) ── */
    lv_obj_t * logo_cont = lv_obj_create(scr);
    lv_obj_set_size(logo_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(logo_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(logo_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(logo_cont, 0, 0);
    lv_obj_set_style_pad_column(logo_cont, 0, 0);
    lv_obj_remove_flag(logo_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(logo_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(logo_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(logo_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(logo_cont, LV_ALIGN_CENTER, 0, 295);

    lv_obj_t * logo1 = lv_label_create(logo_cont);
    lv_label_set_text(logo1, "INV");
    lv_obj_set_style_text_color(logo1, C_WHITE, 0);
    lv_obj_set_style_text_font(logo1, &lv_font_montserrat_26, 0);

    lv_obj_t * logo2 = lv_label_create(logo_cont);
    lv_label_set_text(logo2, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(logo2, C_GREEN, 0);
    lv_obj_set_style_text_font(logo2, &lv_font_montserrat_26, 0);

    /* ── Click zones ── */
    make_click_zone(scr, 250, 250, 0, 0, batt_click_cb);   /* Battery arc */

    /* Clock timer */
    lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL);
}