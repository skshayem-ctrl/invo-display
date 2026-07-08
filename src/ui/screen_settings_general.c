#include <stdio.h>
#include "ui_common.h"
#include "lvgl_port.h"
#include "fota.h"
#include "hal.h"

/* ── Brightness ─────────────────────────────────────────────────── */
static lv_obj_t * s_brightness_pct_lbl;

/* Prevent slider horizontal drag from bubbling up as a swipe-back gesture. */
static void slider_block_gesture_cb(lv_event_t * e)
{
    lv_event_stop_bubbling(e);
}

static void brightness_slider_cb(lv_event_t * e)
{
    lv_obj_t * sl = lv_event_get_target(e);
    int pct       = lv_slider_get_value(sl);
    /* Display is x-mirrored so slider direction is physically reversed.
     * Invert here: knob right (high value) = brighter on screen. */
    int actual = 100 - pct;
    if(actual > 75) actual = 75; /* floor: never fully dark */
    hal_brightness_set(actual);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(s_brightness_pct_lbl, buf);
}

/* ── FOTA overlay handles (created once, toggled hidden/visible) ── */
static lv_obj_t * s_fota_panel;
static lv_obj_t * s_fota_msg;
static lv_obj_t * s_fota_bar;
static lv_obj_t * s_fota_bar_ind;
static lv_obj_t * s_fota_close;
static lv_obj_t * s_update_row;

/* ── FOTA UI callback (called from fota_task — acquires LVGL lock) */
static void fota_ui_cb(fota_state_t state, int pct, const char * msg)
{
    lvgl_acquire();

    lv_label_set_text(s_fota_msg, msg);

    /* progress bar */
    lv_obj_set_width(s_fota_bar_ind, (lv_coord_t)((float)lv_obj_get_width(s_fota_bar) * pct / 100.0f));

    bool show_close = (state == FOTA_DONE || state == FOTA_UP_TO_DATE || state == FOTA_NO_WIFI || state == FOTA_ERROR);
    if(show_close)
        lv_obj_clear_flag(s_fota_close, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(s_fota_close, LV_OBJ_FLAG_HIDDEN);

    lvgl_release();
}

static void fota_close_cb(lv_event_t * e)
{
    lv_obj_add_flag(s_fota_panel, LV_OBJ_FLAG_HIDDEN);
    /* re-enable the update row */
    lv_obj_add_flag(s_update_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(s_update_row, LV_OPA_COVER, 0);
}

static void check_update_cb(lv_event_t * e)
{
    /* disable row while running */
    lv_obj_clear_flag(s_update_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(s_update_row, LV_OPA_50, 0);

    /* reset and show overlay */
    lv_label_set_text(s_fota_msg, "Starting...");
    lv_obj_set_width(s_fota_bar_ind, 0);
    lv_obj_add_flag(s_fota_close, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_fota_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_fota_panel);

    fota_start(fota_ui_cb);
}

/* ── row builder ──────────────────────────────────────────────────*/
static lv_obj_t * settings_row(lv_obj_t * par, const char * icon, lv_color_t icon_col, const char * label,
                               const char * value, bool clickable, lv_event_cb_t cb, int yoff)
{
    lv_obj_t * row = lv_obj_create(par);
    lv_obj_set_size(row, 440, 60);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, yoff);
    lv_obj_set_style_bg_color(row, C_CARD, 0);
    lv_obj_set_style_border_color(row, C_LINE, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_pad_hor(row, 16, 0);
    lv_obj_set_style_pad_ver(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    if(clickable) {
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, NULL);
    } else {
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t * ic = lv_label_create(row);
    lv_label_set_text(ic, icon);
    lv_obj_set_style_text_color(ic, icon_col, 0);
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_20, 0);
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t * lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, C_WHITE, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 36, 0);

    lv_obj_t * val = lv_label_create(row);
    lv_label_set_text(val, value);
    lv_obj_set_style_text_color(val, C_GRAY, 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_16, 0);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);

    return row;
}

/* ── screen builder ─────────────────────────────────────────────── */
lv_obj_t * screen_settings_general_create(void)
{
    lv_obj_t * scr = lv_obj_create(NULL);
    style_screen(scr);
    /* No swipe-back on settings — the horizontal brightness slider
     * causes the gesture to fire whenever the touch escapes the slider
     * bounds during rapid movement, navigating away unexpectedly.
     * Use the Home button at the bottom to go back. */

    mk_lbl(scr, LV_SYMBOL_SETTINGS, &lv_font_montserrat_20, C_GRAY, LV_ALIGN_TOP_MID, 0, 28);
    mk_lbl(scr, "Settings", &lv_font_montserrat_16, C_GRAY, LV_ALIGN_TOP_MID, 0, 56);

    settings_row(scr, LV_SYMBOL_WIFI, C_BLUE, "WiFi", LV_SYMBOL_RIGHT, true, go_wifi_cb, 96);

    /* ── Brightness row with live slider ────────────────────────── */
    {
        lv_obj_t * row = lv_obj_create(scr);
        lv_obj_set_size(row, 440, 60);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 164);
        lv_obj_set_style_bg_color(row, C_CARD, 0);
        lv_obj_set_style_border_color(row, C_LINE, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_set_style_pad_hor(row, 16, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t * ic = lv_label_create(row);
        lv_label_set_text(ic, LV_SYMBOL_IMAGE);
        lv_obj_set_style_text_color(ic, C_AMBER, 0);
        lv_obj_set_style_text_font(ic, &lv_font_montserrat_20, 0);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t * lbl = lv_label_create(row);
        lv_label_set_text(lbl, "Brightness");
        lv_obj_set_style_text_color(lbl, C_WHITE, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 36, 0);

        s_brightness_pct_lbl = lv_label_create(row);
        lv_label_set_text(s_brightness_pct_lbl, "100%");
        lv_obj_set_style_text_color(s_brightness_pct_lbl, C_GRAY, 0);
        lv_obj_set_style_text_font(s_brightness_pct_lbl, &lv_font_montserrat_16, 0);
        lv_obj_align(s_brightness_pct_lbl, LV_ALIGN_RIGHT_MID, 0, 0);

        lv_obj_t * sl = lv_slider_create(row);
        lv_obj_set_size(sl, 180, 6);
        lv_obj_align(sl, LV_ALIGN_LEFT_MID, 155, 0);
        lv_slider_set_range(sl, 0, 100);           /* 90→actual=20% floor, 10→actual=100% ceiling */
        lv_slider_set_value(sl, 100, LV_ANIM_OFF); /* start at full brightness */
        lv_obj_set_style_bg_color(sl, C_DGRAY, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(sl, 3, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sl, C_AMBER, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(sl, 3, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(sl, C_WHITE, LV_PART_KNOB);
        lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_KNOB);
        lv_obj_set_style_pad_all(sl, 5, LV_PART_KNOB);
        lv_obj_set_style_radius(sl, LV_RADIUS_CIRCLE, LV_PART_KNOB);
        lv_obj_add_event_cb(sl, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(sl, slider_block_gesture_cb, LV_EVENT_GESTURE, NULL);
    }

    /* show actual running firmware version */
    char ver_str[24];
    snprintf(ver_str, sizeof(ver_str), "v%s", fota_current_version());
    settings_row(scr, LV_SYMBOL_HOME, C_GREEN, "System", ver_str, false, NULL, 232);

    /* ── Check Update row ──────────────────────────────────────── */
    s_update_row =
        settings_row(scr, LV_SYMBOL_DOWNLOAD, C_BLUE, "Check Update", LV_SYMBOL_RIGHT, true, check_update_cb, 300);

    /* ── back button ────────────────────────────────────────────── */
    lv_obj_t * back = lv_btn_create(scr);
    lv_obj_set_size(back, 110, 42);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -72);
    lv_obj_set_style_bg_color(back, C_DGRAY, 0);
    lv_obj_set_style_radius(back, 21, 0);
    lv_obj_add_event_cb(back, go_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * brow = mk_row(back);
    lv_obj_center(brow);
    lv_obj_t * ba = lv_label_create(brow);
    lv_label_set_text(ba, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(ba, C_WHITE, 0);
    lv_obj_set_style_text_font(ba, &lv_font_montserrat_16, 0);
    lv_obj_t * bbl = lv_label_create(brow);
    lv_label_set_text(bbl, "Home");
    lv_obj_set_style_text_color(bbl, C_WHITE, 0);
    lv_obj_set_style_text_font(bbl, &lv_font_montserrat_16, 0);

    /* ── FOTA status overlay (hidden until Check Update tapped) ─── */
    s_fota_panel = lv_obj_create(scr);
    lv_obj_set_size(s_fota_panel, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(s_fota_panel, 0, 0);
    lv_obj_set_style_bg_color(s_fota_panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_fota_panel, LV_OPA_80, 0);
    lv_obj_set_style_border_width(s_fota_panel, 0, 0);
    lv_obj_set_style_pad_all(s_fota_panel, 0, 0);
    lv_obj_clear_flag(s_fota_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_fota_panel, LV_OBJ_FLAG_HIDDEN);

    /* card inside overlay — 360px wide, centred at y=280 */
    lv_obj_t * card = lv_obj_create(s_fota_panel);
    lv_obj_set_size(card, 360, 210);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(card, C_CARD, 0);
    lv_obj_set_style_border_color(card, C_BLUE, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_all(card, 20, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* title */
    lv_obj_t * title = lv_label_create(card);
    lv_label_set_text(title, LV_SYMBOL_DOWNLOAD "  Firmware Update");
    lv_obj_set_style_text_color(title, C_BLUE, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    /* status message */
    s_fota_msg = lv_label_create(card);
    lv_label_set_text(s_fota_msg, "Starting...");
    lv_obj_set_style_text_color(s_fota_msg, C_WHITE, 0);
    lv_obj_set_style_text_font(s_fota_msg, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(s_fota_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_fota_msg, 300);
    lv_obj_align(s_fota_msg, LV_ALIGN_TOP_MID, 0, 30);

    /* progress track */
    s_fota_bar = lv_obj_create(card);
    lv_obj_set_size(s_fota_bar, 300, 8);
    lv_obj_align(s_fota_bar, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_bg_color(s_fota_bar, C_DGRAY, 0);
    lv_obj_set_style_bg_opa(s_fota_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_fota_bar, 0, 0);
    lv_obj_set_style_radius(s_fota_bar, 4, 0);
    lv_obj_set_style_pad_all(s_fota_bar, 0, 0);
    lv_obj_clear_flag(s_fota_bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* progress fill */
    s_fota_bar_ind = lv_obj_create(s_fota_bar);
    lv_obj_set_size(s_fota_bar_ind, 0, 8);
    lv_obj_align(s_fota_bar_ind, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(s_fota_bar_ind, C_BLUE, 0);
    lv_obj_set_style_bg_opa(s_fota_bar_ind, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_fota_bar_ind, 0, 0);
    lv_obj_set_style_radius(s_fota_bar_ind, 4, 0);
    lv_obj_set_style_pad_all(s_fota_bar_ind, 0, 0);
    lv_obj_clear_flag(s_fota_bar_ind, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* close button — hidden until complete */
    s_fota_close = lv_btn_create(card);
    lv_obj_set_size(s_fota_close, 120, 36);
    lv_obj_align(s_fota_close, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_fota_close, C_DGRAY, 0);
    lv_obj_set_style_radius(s_fota_close, 18, 0);
    lv_obj_add_event_cb(s_fota_close, fota_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_fota_close, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t * cl = lv_label_create(s_fota_close);
    lv_label_set_text(cl, "Close");
    lv_obj_set_style_text_color(cl, C_WHITE, 0);
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_14, 0);
    lv_obj_center(cl);

    return scr;
}
