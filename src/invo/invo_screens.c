#include "invo_screens.h"
#include "invo_home.h"
#include "invo_wifi.h"
#include "invo_weather.h"
#include "invo_uart.h"
#include "invo_sleep.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static lv_obj_t * home_scr;
static lv_obj_t * solar_scr;
static lv_obj_t * battery_scr;
static lv_obj_t * weather_scr;
static lv_obj_t * home_load_scr;

static lv_obj_t * hdr_time[6];
static lv_obj_t * hdr_date[6];

static lv_obj_t * settings_scr;
static lv_obj_t * history_scr;
static lv_obj_t * alerts_scr;

static lv_obj_t * update_status_lbl;

static lv_obj_t * hist_chart;
static lv_chart_series_t * hist_batt_ser;
static lv_chart_series_t * hist_solar_ser;

static lv_obj_t * alert_fault_lbl;
static lv_obj_t * alert_bypass_lbl;
static lv_obj_t * alert_inv_lbl;
static lv_obj_t * alert_batt_lbl;
static lv_obj_t * alert_temp_lbl;
static lv_obj_t * alert_none_lbl;

void nav_to_home(void)
{
    lv_scr_load_anim(home_scr, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}
void nav_to_solar(void)
{
    lv_scr_load_anim(solar_scr, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}
void nav_to_battery(void)
{
    lv_scr_load_anim(battery_scr, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}
void nav_to_home_load(void)
{
    lv_scr_load_anim(home_load_scr, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}
void nav_to_weather(void)
{
    lv_scr_load_anim(weather_scr, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
    invo_weather_refresh();
}
void nav_to_wifi(void)
{
    invo_wifi_show_list();
}
void nav_to_settings(void)
{
    lv_scr_load_anim(settings_scr, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}
void nav_to_history(void)
{
    lv_scr_load_anim(history_scr, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}
void nav_to_alerts(void)
{
    lv_scr_load_anim(alerts_scr, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}
static void back_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    nav_to_home();
}

static void swipe_back_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    if(lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT) nav_to_home();
}

static void check_update_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_label_set_text(update_status_lbl, LV_SYMBOL_REFRESH " Checking in background...");
    lv_obj_set_style_text_color(update_status_lbl, C_BLUE, 0);
    system("systemctl start invo-updater &");
}

static void alerts_update_cb(lv_timer_t * tmr)
{
    LV_UNUSED(tmr);
    float batt_pct = 0, batt_temp = 0;
    int fault = 0, bypass = 0, inv_on = 1;
    invo_uart_get_live(&batt_pct, NULL, NULL, &fault, &bypass, &inv_on, &batt_temp);
    bool any = false;
    char buf[48];

    if(fault) {
        lv_label_set_text(alert_fault_lbl, LV_SYMBOL_WARNING " FAULT detected");
        lv_obj_set_style_text_color(alert_fault_lbl, lv_color_hex(0xff4444), 0);
        lv_obj_remove_flag(alert_fault_lbl, LV_OBJ_FLAG_HIDDEN);
        any = true;
    } else {
        lv_obj_add_flag(alert_fault_lbl, LV_OBJ_FLAG_HIDDEN);
    }

    if(bypass) {
        lv_label_set_text(alert_bypass_lbl, LV_SYMBOL_CHARGE " Bypass ACTIVE");
        lv_obj_set_style_text_color(alert_bypass_lbl, C_ORANGE, 0);
        lv_obj_remove_flag(alert_bypass_lbl, LV_OBJ_FLAG_HIDDEN);
        any = true;
    } else {
        lv_obj_add_flag(alert_bypass_lbl, LV_OBJ_FLAG_HIDDEN);
    }

    if(!inv_on) {
        lv_label_set_text(alert_inv_lbl, LV_SYMBOL_POWER " Inverter is OFF");
        lv_obj_set_style_text_color(alert_inv_lbl, C_GRAY, 0);
        lv_obj_remove_flag(alert_inv_lbl, LV_OBJ_FLAG_HIDDEN);
        any = true;
    } else {
        lv_obj_add_flag(alert_inv_lbl, LV_OBJ_FLAG_HIDDEN);
    }

    if(batt_pct < 20.0f) {
        lv_snprintf(buf, sizeof(buf), LV_SYMBOL_BATTERY_1 " Battery low: %.0f%%", batt_pct);
        lv_label_set_text(alert_batt_lbl, buf);
        lv_obj_set_style_text_color(alert_batt_lbl, batt_pct < 10.0f ? lv_color_hex(0xff4444) : C_ORANGE, 0);
        lv_obj_remove_flag(alert_batt_lbl, LV_OBJ_FLAG_HIDDEN);
        any = true;
    } else {
        lv_obj_add_flag(alert_batt_lbl, LV_OBJ_FLAG_HIDDEN);
    }

    if(batt_temp > 45.0f) {
        lv_snprintf(buf, sizeof(buf),
                    LV_SYMBOL_WARNING " High temp: %.0f\xc2\xb0"
                                      "C",
                    batt_temp);
        lv_label_set_text(alert_temp_lbl, buf);
        lv_obj_set_style_text_color(alert_temp_lbl, C_ORANGE, 0);
        lv_obj_remove_flag(alert_temp_lbl, LV_OBJ_FLAG_HIDDEN);
        any = true;
    } else {
        lv_obj_add_flag(alert_temp_lbl, LV_OBJ_FLAG_HIDDEN);
    }

    if(any)
        lv_obj_add_flag(alert_none_lbl, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_remove_flag(alert_none_lbl, LV_OBJ_FLAG_HIDDEN);
}

static void history_sample_cb(lv_timer_t * tmr)
{
    LV_UNUSED(tmr);
    float batt_pct = 0, solar_kw = 0;
    invo_uart_get_live(&batt_pct, &solar_kw, NULL, NULL, NULL, NULL, NULL);
    lv_chart_set_next_value(hist_chart, hist_batt_ser, (lv_value_precise_t)batt_pct);
    lv_chart_set_next_value(hist_chart, hist_solar_ser, (lv_value_precise_t)(solar_kw * 10.0f));
}

static void output_on_cb(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);
    FILE * f       = fopen("/home/intelli/invo_cmd", "w");
    if(f) {
        fprintf(f, "output_on\n");
        fclose(f);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x003a00), 0);
        lv_obj_set_style_border_color(btn, C_GREEN, 0);
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a0000), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0xff2200), 0);
    }
}

static void output_off_cb(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);
    FILE * f       = fopen("/home/intelli/invo_cmd", "w");
    if(f) {
        fprintf(f, "output_off\n");
        fclose(f);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a0000), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0xff3300), 0);
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x333333), 0);
    }
}

static void make_back_button(lv_obj_t * scr)
{
    lv_obj_t * btn = lv_button_create(scr);
    lv_obj_set_size(btn, 72, 72);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, -200, -90);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_radius(btn, 36, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_add_event_cb(btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lbl, C_WHITE, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);
}

static void make_logo(lv_obj_t * scr)
{
    lv_obj_t * cont = lv_obj_create(scr);
    lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_pad_column(cont, 0, 0);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -22);

    lv_obj_t * l1 = lv_label_create(cont);
    lv_label_set_text(l1, "INV");
    lv_obj_set_style_text_color(l1, C_WHITE, 0);
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_26, 0);

    lv_obj_t * l2 = lv_label_create(cont);
    lv_label_set_text(l2, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(l2, C_GREEN, 0);
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_26, 0);
}

static void make_header(lv_obj_t * scr, lv_obj_t ** t_out, lv_obj_t ** d_out)
{
    time_t now    = time(NULL);
    struct tm * t = localtime(&now);
    char time_buf[16], date_buf[24];
    strftime(time_buf, sizeof(time_buf), "%I:%M %p", t);
    strftime(date_buf, sizeof(date_buf), "%b %d %A", t);

    lv_obj_t * wifi = lv_label_create(scr);
    lv_label_set_text(wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi, C_GREEN, 0);
    lv_obj_set_style_text_font(wifi, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_opa(wifi, LV_OPA_70, 0);
    lv_obj_align(wifi, LV_ALIGN_TOP_MID, 0, 52);

    lv_obj_t * time_lbl = lv_label_create(scr);
    lv_label_set_text(time_lbl, time_buf);
    lv_obj_set_style_text_color(time_lbl, C_WHITE, 0);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_28, 0);
    lv_obj_align(time_lbl, LV_ALIGN_TOP_MID, 0, 76);

    lv_obj_t * date_lbl = lv_label_create(scr);
    lv_label_set_text(date_lbl, date_buf);
    lv_obj_set_style_text_color(date_lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(date_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(date_lbl, LV_ALIGN_TOP_MID, 0, 113);

    if(t_out) *t_out = time_lbl;
    if(d_out) *d_out = date_lbl;
}

/* Reusable stat card: label (top) / value (center) / sub (bottom) */
static lv_obj_t * make_stat_card(lv_obj_t * scr, int w, int h, int ox, int oy, const char * label, const char * value,
                                 const char * sub, lv_color_t val_color, lv_color_t sub_color)
{
    lv_obj_t * card = lv_obj_create(scr);
    lv_obj_set_size(card, w, h);
    lv_obj_align(card, LV_ALIGN_CENTER, ox, oy);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x161616), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * lbl = lv_label_create(card);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t * val = lv_label_create(card);
    lv_label_set_text(val, value);
    lv_obj_set_style_text_color(val, val_color, 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
    lv_obj_align(val, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * sub_lbl = lv_label_create(card);
    lv_label_set_text(sub_lbl, sub);
    lv_obj_set_style_text_color(sub_lbl, sub_color, 0);
    lv_obj_set_style_text_font(sub_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(sub_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return val;
}

/* ─────────────────────────────────────────
 * Solar Screen
 * ───────────────────────────────────────── */
static void build_solar_screen(lv_obj_t * scr)
{
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    make_header(scr, &hdr_time[0], &hdr_date[0]);

    /* Left panel: icon / label / value */
    lv_obj_t * icon = lv_label_create(scr);
    lv_label_set_text(icon, WI_SUN);
    lv_obj_set_style_text_color(icon, C_ORANGE, 0);
    lv_obj_set_style_text_font(icon, &lv_font_weather_36, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, -215, -138);

    lv_obj_t * title = lv_label_create(scr);
    lv_label_set_text(title, "Solar Input");
    lv_obj_set_style_text_color(title, C_GRAY, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, -215, -82);

    lv_obj_t * val = lv_label_create(scr);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, C_BLUE, 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_28, 0);
    lv_obj_align(val, LV_ALIGN_CENTER, -215, -46);

    /* Divider */
    lv_obj_t * divider = lv_obj_create(scr);
    lv_obj_set_size(divider, 1, 230);
    lv_obj_align(divider, LV_ALIGN_CENTER, -122, -85);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_opa(divider, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(divider, LV_OBJ_FLAG_SCROLLABLE);

    /* Right 2x2 stat grid — cards 195x100 */
    lv_obj_t * pw_lbl = make_stat_card(scr, 195, 100, -18, -185, "Grid Hz", "--", "AC input freq", C_PURPLE, C_GRAY);
    lv_obj_t * gv_lbl = make_stat_card(scr, 195, 100, 192, -185, "Grid V", "--", "Mains voltage", C_PURPLE, C_GRAY);
    lv_obj_t * volt_lbl =
        make_stat_card(scr, 195, 100, -18, -70, "PV Voltage", "--", "Panel voltage", C_ORANGE, C_GRAY);
    lv_obj_t * amp_lbl = make_stat_card(scr, 195, 100, 192, -70, "PV Current", "--", "Panel current", C_ORANGE, C_GRAY);
    invo_uart_register_solar(val, volt_lbl, amp_lbl);
    invo_uart_register_solar_detail(pw_lbl, gv_lbl);

    /* Power generation chart */
    lv_obj_t * ct = lv_label_create(scr);
    lv_label_set_text(ct, "POWER GENERATION  (TODAY)");
    lv_obj_set_style_text_color(ct, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(ct, &lv_font_montserrat_12, 0);
    lv_obj_align(ct, LV_ALIGN_CENTER, 0, 58);

    lv_obj_t * chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 555, 125);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 138);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x0d1a0d), 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chart, lv_color_hex(0x1e2e1e), 0);
    lv_obj_set_style_border_width(chart, 1, 0);
    lv_obj_set_style_radius(chart, 10, 0);
    lv_chart_set_div_line_count(chart, 3, 4);
    lv_obj_set_style_line_color(chart, lv_color_hex(0x1a2a1a), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 30);
    lv_chart_set_point_count(chart, 13);

    lv_chart_series_t * ser = lv_chart_add_series(chart, C_BLUE, LV_CHART_AXIS_PRIMARY_Y);
    for(int i = 0; i < 13; i++) lv_chart_set_next_value(chart, ser, 0);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_width(chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(chart, 0, LV_PART_INDICATOR);

    make_back_button(scr);
    make_logo(scr);
}

/* ─────────────────────────────────────────
 * Home Load Screen
 * ───────────────────────────────────────── */
static void build_home_load_screen(lv_obj_t * scr)
{
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    make_header(scr, &hdr_time[1], &hdr_date[1]);

    /* Left panel: icon / label / value */
    lv_obj_t * icon = lv_label_create(scr);
    lv_label_set_text(icon, LV_SYMBOL_HOME);
    lv_obj_set_style_text_color(icon, C_BLUE, 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_44, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, -215, -138);

    lv_obj_t * title = lv_label_create(scr);
    lv_label_set_text(title, "Home Load");
    lv_obj_set_style_text_color(title, C_GRAY, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, -215, -82);

    lv_obj_t * val = lv_label_create(scr);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, C_BLUE, 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_28, 0);
    lv_obj_align(val, LV_ALIGN_CENTER, -215, -46);

    /* Divider */
    lv_obj_t * divider = lv_obj_create(scr);
    lv_obj_set_size(divider, 1, 230);
    lv_obj_align(divider, LV_ALIGN_CENTER, -122, -85);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_opa(divider, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(divider, LV_OBJ_FLAG_SCROLLABLE);

    /* Right 2x2 stat grid — cards 195x100 */
    lv_obj_t * ov_lbl   = make_stat_card(scr, 195, 100, -18, -185, "Output V", "--", "AC voltage", C_BLUE, C_GRAY);
    lv_obj_t * ohz_lbl  = make_stat_card(scr, 195, 100, 192, -185, "Output Hz", "--", "AC frequency", C_BLUE, C_GRAY);
    lv_obj_t * draw_lbl = make_stat_card(scr, 195, 100, -18, -70, "Output W", "--", "AC watts", C_BLUE, C_GRAY);
    lv_obj_t * peak_lbl = make_stat_card(scr, 195, 100, 192, -70, "Output A", "--", "AC current", C_ORANGE, C_GRAY);
    invo_uart_register_load(val, draw_lbl, peak_lbl);
    invo_uart_register_load_detail(ov_lbl, ohz_lbl);

    /* Consumption chart */
    lv_obj_t * ct = lv_label_create(scr);
    lv_label_set_text(ct, "CONSUMPTION  (TODAY)");
    lv_obj_set_style_text_color(ct, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(ct, &lv_font_montserrat_12, 0);
    lv_obj_align(ct, LV_ALIGN_CENTER, 0, 58);

    lv_obj_t * chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 555, 125);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 138);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x0a0d1a), 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chart, lv_color_hex(0x1a1e2e), 0);
    lv_obj_set_style_border_width(chart, 1, 0);
    lv_obj_set_style_radius(chart, 10, 0);
    lv_chart_set_div_line_count(chart, 3, 4);
    lv_obj_set_style_line_color(chart, lv_color_hex(0x1a1a2a), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 40);
    lv_chart_set_point_count(chart, 13);

    lv_chart_series_t * ser = lv_chart_add_series(chart, C_BLUE, LV_CHART_AXIS_PRIMARY_Y);
    for(int i = 0; i < 13; i++) lv_chart_set_next_value(chart, ser, 0);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_width(chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(chart, 0, LV_PART_INDICATOR);

    make_back_button(scr);
    make_logo(scr);
}

/* ─────────────────────────────────────────
 * Battery / Inverter Screen
 * Mirrors the Python web UI dashboard layout:
 *   Left:   SYS STATUS card (inverter on, AC charge, bypass, fault)
 *   Right:  2×3 stat grid  (grid V/Hz/W  |  output V/Hz/W)
 *   Bottom: 3 stat cards   (batt V, batt A, inv temp)
 *   Buttons: Back | Output ON | Output OFF
 * ───────────────────────────────────────── */
static void build_battery_screen(lv_obj_t * scr)
{
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    make_header(scr, &hdr_time[2], &hdr_date[2]);

    /* ── Left: System Status card ─────────────────────────── */
    lv_obj_t * scard = lv_obj_create(scr);
    lv_obj_set_size(scard, 185, 200);
    lv_obj_align(scard, LV_ALIGN_CENTER, -195, -20);
    lv_obj_set_style_bg_color(scard, lv_color_hex(0x0a0f1a), 0);
    lv_obj_set_style_border_color(scard, lv_color_hex(0x00aacc), 0);
    lv_obj_set_style_border_width(scard, 1, 0);
    lv_obj_set_style_border_opa(scard, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(scard, 12, 0);
    lv_obj_set_style_pad_all(scard, 10, 0);
    lv_obj_remove_flag(scard, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * stitle = lv_label_create(scard);
    lv_label_set_text(stitle, "SYS STATUS");
    lv_obj_set_style_text_color(stitle, lv_color_hex(0x00aacc), 0);
    lv_obj_set_style_text_font(stitle, &lv_font_montserrat_12, 0);
    lv_obj_align(stitle, LV_ALIGN_TOP_MID, 0, 0);

    /* 4 status rows: row label (left) + value label (right) */
    static const char * row_names[] = {"INVERTER", "AC CHARGE", "BYPASS", "FAULT"};
    lv_obj_t * status_vals[4];
    for(int i = 0; i < 4; i++) {
        int oy = 22 + i * 40;

        lv_obj_t * rl = lv_label_create(scard);
        lv_label_set_text(rl, row_names[i]);
        lv_obj_set_style_text_color(rl, C_GRAY, 0);
        lv_obj_set_style_text_font(rl, &lv_font_montserrat_12, 0);
        lv_obj_align(rl, LV_ALIGN_TOP_LEFT, 4, oy);

        lv_obj_t * rv = lv_label_create(scard);
        lv_label_set_text(rv, "--");
        lv_obj_set_style_text_color(rv, C_GRAY, 0);
        lv_obj_set_style_text_font(rv, &lv_font_montserrat_14, 0);
        lv_obj_align(rv, LV_ALIGN_TOP_RIGHT, -4, oy);
        status_vals[i] = rv;
    }
    lv_obj_t * inv_on_lbl = status_vals[0];
    lv_obj_t * ac_chg_lbl = status_vals[1];
    lv_obj_t * bypass_lbl = status_vals[2];
    lv_obj_t * fault_lbl  = status_vals[3];

    /* ── Right 2×3 stat grid — battery data ─────────────── */
    lv_obj_t * soc_val  = make_stat_card(scr, 175, 80, -8, -170, "SOC", "--", "Charge level", C_GREEN, C_GRAY);
    lv_obj_t * bv_val   = make_stat_card(scr, 175, 80, 177, -170, "Battery V", "--", "DC bus voltage", C_GREEN, C_GRAY);
    lv_obj_t * ba_val   = make_stat_card(scr, 175, 80, -8, -75, "Battery A", "--", "Batt current", C_GREEN, C_GRAY);
    lv_obj_t * chg_val  = make_stat_card(scr, 175, 80, 177, -75, "Charge", "--", "Charge power", C_ORANGE, C_GRAY);
    lv_obj_t * temp_val = make_stat_card(scr, 175, 80, -8, 20, "Inv Temp", "--", "Inverter temp", C_ORANGE, C_GRAY);
    lv_obj_t * bkp_val  = make_stat_card(scr, 175, 80, 177, 20, "Backup", "--", "Est. runtime", C_BLUE, C_GRAY);

    invo_uart_register_battery(soc_val, chg_val, temp_val, bkp_val);
    invo_uart_register_grid(NULL, bv_val, NULL, NULL, NULL, NULL);
    invo_uart_register_inverter_screen(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, ba_val, NULL, inv_on_lbl,
                                       bypass_lbl, ac_chg_lbl, fault_lbl);

    /* ── Output ON button ─────────────────────────────────── */
    lv_obj_t * on_btn = lv_button_create(scr);
    lv_obj_set_size(on_btn, 85, 55);
    lv_obj_align(on_btn, LV_ALIGN_BOTTOM_MID, 10, -90);
    lv_obj_set_style_bg_color(on_btn, lv_color_hex(0x0d2a0d), 0);
    lv_obj_set_style_radius(on_btn, 8, 0);
    lv_obj_set_style_border_color(on_btn, C_GREEN, 0);
    lv_obj_set_style_border_width(on_btn, 2, 0);
    lv_obj_add_event_cb(on_btn, output_on_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * on_lbl = lv_label_create(on_btn);
    lv_label_set_text(on_lbl, "OUT\nON");
    lv_obj_set_style_text_color(on_lbl, C_GREEN, 0);
    lv_obj_set_style_text_font(on_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(on_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(on_lbl);

    /* ── Output OFF button ────────────────────────────────── */
    lv_obj_t * off_btn = lv_button_create(scr);
    lv_obj_set_size(off_btn, 85, 55);
    lv_obj_align(off_btn, LV_ALIGN_BOTTOM_MID, 110, -90);
    lv_obj_set_style_bg_color(off_btn, lv_color_hex(0x2a0d0d), 0);
    lv_obj_set_style_radius(off_btn, 8, 0);
    lv_obj_set_style_border_color(off_btn, lv_color_hex(0xff3300), 0);
    lv_obj_set_style_border_width(off_btn, 2, 0);
    lv_obj_add_event_cb(off_btn, output_off_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * off_lbl = lv_label_create(off_btn);
    lv_label_set_text(off_lbl, "OUT\nOFF");
    lv_obj_set_style_text_color(off_lbl, lv_color_hex(0xff3300), 0);
    lv_obj_set_style_text_font(off_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(off_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(off_lbl);

    make_back_button(scr);
    make_logo(scr);
}

static void build_settings_screen(lv_obj_t * scr)
{
    make_header(scr, &hdr_time[3], &hdr_date[3]);

    lv_obj_t * title = lv_label_create(scr);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_color(title, C_WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 140);

    char ver_buf[32] = "---";
    FILE * vf        = fopen("/etc/invo/version", "r");
    if(vf) {
        fgets(ver_buf, sizeof(ver_buf), vf);
        fclose(vf);
    }
    char * nl = strchr(ver_buf, '\n');
    if(nl) *nl = '\0';

    lv_obj_t * ver_card = lv_obj_create(scr);
    lv_obj_set_size(ver_card, 280, 80);
    lv_obj_align(ver_card, LV_ALIGN_CENTER, 0, -80);
    lv_obj_set_style_bg_color(ver_card, lv_color_hex(0x161616), 0);
    lv_obj_set_style_border_color(ver_card, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(ver_card, 1, 0);
    lv_obj_set_style_radius(ver_card, 12, 0);
    lv_obj_remove_flag(ver_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * ver_lbl = lv_label_create(ver_card);
    lv_label_set_text(ver_lbl, "FIRMWARE VERSION");
    lv_obj_set_style_text_color(ver_lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(ver_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(ver_lbl, LV_ALIGN_TOP_MID, 0, 5);

    lv_obj_t * ver_val = lv_label_create(ver_card);
    char full_ver[40];
    lv_snprintf(full_ver, sizeof(full_ver), "v%s", ver_buf);
    lv_label_set_text(ver_val, full_ver);
    lv_obj_set_style_text_color(ver_val, C_GREEN, 0);
    lv_obj_set_style_text_font(ver_val, &lv_font_montserrat_24, 0);
    lv_obj_align(ver_val, LV_ALIGN_CENTER, 0, 8);

    lv_obj_t * upd_btn = lv_button_create(scr);
    lv_obj_set_size(upd_btn, 240, 55);
    lv_obj_align(upd_btn, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(upd_btn, lv_color_hex(0x0a1f0a), 0);
    lv_obj_set_style_border_color(upd_btn, C_GREEN, 0);
    lv_obj_set_style_border_width(upd_btn, 1, 0);
    lv_obj_set_style_radius(upd_btn, 12, 0);
    lv_obj_add_event_cb(upd_btn, check_update_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * upd_lbl = lv_label_create(upd_btn);
    lv_label_set_text(upd_lbl, LV_SYMBOL_REFRESH "  Check for Update");
    lv_obj_set_style_text_color(upd_lbl, C_GREEN, 0);
    lv_obj_set_style_text_font(upd_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(upd_lbl);

    update_status_lbl = lv_label_create(scr);
    lv_label_set_text(update_status_lbl, "Tap to check for firmware updates");
    lv_obj_set_style_text_color(update_status_lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(update_status_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_width(update_status_lbl, 300);
    lv_label_set_long_mode(update_status_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(update_status_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(update_status_lbl, LV_ALIGN_CENTER, 0, 80);

    make_back_button(scr);
    make_logo(scr);
}

static void build_history_screen(lv_obj_t * scr)
{
    make_header(scr, &hdr_time[4], &hdr_date[4]);

    lv_obj_t * title = lv_label_create(scr);
    lv_label_set_text(title, "POWER HISTORY");
    lv_obj_set_style_text_color(title, C_WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 140);

    hist_chart = lv_chart_create(scr);
    lv_obj_set_size(hist_chart, 480, 200);
    lv_obj_align(hist_chart, LV_ALIGN_CENTER, 0, -30);
    lv_chart_set_type(hist_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(hist_chart, 24);
    lv_chart_set_range(hist_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_obj_set_style_bg_color(hist_chart, lv_color_hex(0x0d1a0d), 0);
    lv_obj_set_style_border_color(hist_chart, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(hist_chart, 1, 0);
    lv_obj_set_style_radius(hist_chart, 8, 0);
    lv_obj_set_style_size(hist_chart, 0, 0, LV_PART_INDICATOR);
    lv_chart_set_div_line_count(hist_chart, 4, 6);

    hist_batt_ser  = lv_chart_add_series(hist_chart, C_GREEN, LV_CHART_AXIS_PRIMARY_Y);
    hist_solar_ser = lv_chart_add_series(hist_chart, C_ORANGE, LV_CHART_AXIS_PRIMARY_Y);

    for(int i = 0; i < 24; i++) {
        lv_chart_set_next_value(hist_chart, hist_batt_ser, 0);
        lv_chart_set_next_value(hist_chart, hist_solar_ser, 0);
    }

    lv_obj_t * leg = lv_label_create(scr);
    lv_label_set_text(leg, "#00e676 \u25cf Battery %#    #ffa500 \u25cf Solar kW\u00d710#");
    lv_obj_set_style_text_font(leg, &lv_font_montserrat_12, 0);
    lv_label_set_recolor(leg, true);
    lv_obj_align(leg, LV_ALIGN_CENTER, 0, 82);

    lv_obj_t * note = lv_label_create(scr);
    lv_label_set_text(note, "Sampled every 5 min \u00b7 last 24 readings");
    lv_obj_set_style_text_color(note, C_GRAY, 0);
    lv_obj_set_style_text_font(note, &lv_font_montserrat_12, 0);
    lv_obj_align(note, LV_ALIGN_CENTER, 0, 102);

    make_back_button(scr);
    make_logo(scr);
}

static void build_alerts_screen(lv_obj_t * scr)
{
    make_header(scr, &hdr_time[5], &hdr_date[5]);

    lv_obj_t * title = lv_label_create(scr);
    lv_label_set_text(title, "ALERTS");
    lv_obj_set_style_text_color(title, C_WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 140);

    lv_obj_t * cont = lv_obj_create(scr);
    lv_obj_set_size(cont, 440, 250);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, -5);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x0d0d0d), 0);
    lv_obj_set_style_border_color(cont, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(cont, 1, 0);
    lv_obj_set_style_radius(cont, 12, 0);
    lv_obj_set_style_pad_all(cont, 16, 0);
    lv_obj_set_style_pad_row(cont, 12, 0);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    alert_fault_lbl  = lv_label_create(cont);
    alert_bypass_lbl = lv_label_create(cont);
    alert_inv_lbl    = lv_label_create(cont);
    alert_batt_lbl   = lv_label_create(cont);
    alert_temp_lbl   = lv_label_create(cont);

    lv_obj_t * albs[] = {alert_fault_lbl, alert_bypass_lbl, alert_inv_lbl, alert_batt_lbl, alert_temp_lbl};
    for(int i = 0; i < 5; i++) {
        lv_obj_set_style_text_font(albs[i], &lv_font_montserrat_16, 0);
        lv_obj_add_flag(albs[i], LV_OBJ_FLAG_HIDDEN);
    }

    alert_none_lbl = lv_label_create(cont);
    lv_label_set_text(alert_none_lbl, LV_SYMBOL_OK " All systems normal");
    lv_obj_set_style_text_color(alert_none_lbl, C_GREEN, 0);
    lv_obj_set_style_text_font(alert_none_lbl, &lv_font_montserrat_18, 0);

    make_back_button(scr);
    make_logo(scr);
}

/* ─────────────────────────────────────────
 * Init
 * ───────────────────────────────────────── */
static void weather_auto_refresh_cb(lv_timer_t * t)
{
    LV_UNUSED(t);
    invo_weather_refresh();
}

static void detail_clock_cb(lv_timer_t * tmr)
{
    LV_UNUSED(tmr);
    time_t now          = time(NULL);
    struct tm * tm_info = localtime(&now);
    char tb[16], db[24];
    strftime(tb, sizeof(tb), "%I:%M %p", tm_info);
    strftime(db, sizeof(db), "%b %d %A", tm_info);
    for(int i = 0; i < 6; i++) {
        if(hdr_time[i]) lv_label_set_text(hdr_time[i], tb);
        if(hdr_date[i]) lv_label_set_text(hdr_date[i], db);
    }
    invo_weather_sync_clock();
}

void invo_screens_init(void)
{
    home_scr      = lv_obj_create(NULL);
    solar_scr     = lv_obj_create(NULL);
    battery_scr   = lv_obj_create(NULL);
    weather_scr   = lv_obj_create(NULL);
    home_load_scr = lv_obj_create(NULL);

    lv_obj_remove_flag(solar_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(battery_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(weather_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(home_load_scr, LV_OBJ_FLAG_SCROLLABLE);

    settings_scr = lv_obj_create(NULL);
    history_scr  = lv_obj_create(NULL);
    alerts_scr   = lv_obj_create(NULL);
    lv_obj_remove_flag(settings_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(history_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(alerts_scr, LV_OBJ_FLAG_SCROLLABLE);

    invo_home_screen_create(home_scr);
    build_solar_screen(solar_scr);
    build_battery_screen(battery_scr);
    invo_weather_init(weather_scr);
    build_home_load_screen(home_load_scr);
    build_settings_screen(settings_scr);
    build_history_screen(history_scr);
    build_alerts_screen(alerts_scr);
    invo_wifi_init();

    lv_timer_create(detail_clock_cb, 1000, NULL);
    lv_timer_create(alerts_update_cb, 2000, NULL);
    lv_timer_create(history_sample_cb, 5 * 60 * 1000, NULL);

    invo_uart_init();
    invo_sleep_init();

    lv_scr_load(home_scr);

    /* Fetch weather immediately on startup, then every 30 minutes */
    invo_weather_refresh();
    lv_timer_create(weather_auto_refresh_cb, 30 * 60 * 1000, NULL);
}
