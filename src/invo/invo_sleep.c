#include "invo_sleep.h"
#include "invo_screens.h"
#include <stdio.h>
#include <time.h>

#define SLEEP_TIMEOUT_MS  (1 * 60 * 1000)   /* 1 minute */

static lv_obj_t * sleep_scr;
static lv_obj_t * sleep_time_lbl;
static lv_obj_t * sleep_date_lbl;
static bool        sleeping;

static void wake_cb(lv_event_t * e) {
    LV_UNUSED(e);
    if (!sleeping) return;
    sleeping = false;
    lv_display_trigger_activity(NULL);
    nav_to_home();
}

static void sleep_clock_cb(lv_timer_t * t) {
    LV_UNUSED(t);
    time_t now = time(NULL);
    struct tm * ti = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%I:%M %p", ti);
    lv_label_set_text(sleep_time_lbl, buf);
    strftime(buf, sizeof(buf), "%A, %B %d", ti);
    lv_label_set_text(sleep_date_lbl, buf);
}

static void sleep_check_cb(lv_timer_t * t) {
    LV_UNUSED(t);
    if (sleeping) return;
    if (lv_display_get_inactive_time(NULL) < SLEEP_TIMEOUT_MS) return;
    sleeping = true;
    lv_scr_load_anim(sleep_scr, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, false);
}

void invo_sleep_init(void) {
    sleeping = false;

    sleep_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(sleep_scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(sleep_scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(sleep_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Create labels, populate text, then align (align needs correct size) */
    sleep_time_lbl = lv_label_create(sleep_scr);
    lv_obj_set_style_text_color(sleep_time_lbl, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(sleep_time_lbl, &lv_font_montserrat_48, 0);

    sleep_date_lbl = lv_label_create(sleep_scr);
    lv_obj_set_style_text_color(sleep_date_lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(sleep_date_lbl, &lv_font_montserrat_24, 0);

    sleep_clock_cb(NULL);

    lv_obj_align(sleep_time_lbl, LV_ALIGN_CENTER, 0, -20);
    lv_obj_align(sleep_date_lbl, LV_ALIGN_CENTER, 0, 40);

    /* Any press on the sleep screen wakes the display */
    lv_obj_add_flag(sleep_scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sleep_scr, wake_cb, LV_EVENT_PRESSED, NULL);

    lv_timer_create(sleep_clock_cb, 1000, NULL);
    lv_timer_create(sleep_check_cb, 10000, NULL);
}
