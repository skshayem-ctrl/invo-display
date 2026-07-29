#include "ui_common.h"
#include "modbus_inverter.h"
#include "esp_timer.h"

/* ── Output on/off ────────────────────────────────────────────────── */
static lv_obj_t *s_out_status_lbl = NULL;
static lv_obj_t *s_out_btn        = NULL;
static lv_obj_t *s_out_btn_lbl    = NULL;

static void output_toggle_cb(lv_event_t *e)
{
    static int64_t s_last_ms = 0;
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - s_last_ms < 1000) return;
    s_last_ms = now_ms;
    modbus_inverter_request_output(gd.out_switch ? 0 : 1);
}

void screen_settings_set_output_state(int actual_on, int switch_on)
{
    if (s_out_status_lbl) {
        lv_label_set_text(s_out_status_lbl, actual_on ? "ON" : "OFF");
        lv_obj_set_style_text_color(s_out_status_lbl,
                                    actual_on ? C_GREEN : C_RED, 0);
    }
    if (s_out_btn) {
        lv_obj_set_style_bg_color(s_out_btn,
                                  switch_on ? lv_color_hex(0x2a0d0d) : lv_color_hex(0x0d2a0d), 0);
        lv_obj_set_style_border_color(s_out_btn, switch_on ? C_RED : C_GREEN, 0);
    }
    if (s_out_btn_lbl) {
        lv_label_set_text(s_out_btn_lbl, switch_on ? "Turn OFF" : "Turn ON");
        lv_obj_set_style_text_color(s_out_btn_lbl, switch_on ? C_RED : C_GREEN, 0);
    }
}

/* ── row builder (shared with battery settings row style) ──────── */
static lv_obj_t *settings_link_row(lv_obj_t *par, const char *icon,
                                   lv_color_t icon_col, const char *label,
                                   lv_event_cb_t cb, int yoff)
{
    lv_obj_t *row = lv_obj_create(par);
    lv_obj_set_size(row, 440, 60);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, yoff);
    lv_obj_set_style_bg_color(row, C_CARD, 0);
    lv_obj_set_style_border_color(row, C_LINE, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_pad_hor(row, 16, 0);
    lv_obj_set_style_pad_ver(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *ic = lv_label_create(row);
    lv_label_set_text(ic, icon);
    lv_obj_set_style_text_color(ic, icon_col, 0);
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_20, 0);
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, C_WHITE, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 36, 0);

    lv_obj_t *arr = lv_label_create(row);
    lv_label_set_text(arr, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(arr, C_GRAY, 0);
    lv_obj_set_style_text_font(arr, &lv_font_montserrat_16, 0);
    lv_obj_align(arr, LV_ALIGN_RIGHT_MID, 0, 0);

    return row;
}

lv_obj_t *screen_settings_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, swipe_back_cb, LV_EVENT_GESTURE, NULL);

    add_detail_header(scr, "Settings");

    settings_link_row(scr, LV_SYMBOL_SETTINGS, C_BLUE,
                      "General Settings", go_settings_general_cb, 76);
    settings_link_row(scr, LV_SYMBOL_BATTERY_3, C_GREEN,
                      "Battery Settings", go_batt_settings_cb, 148);

    /* ── Output on/off row ───────────────────────────────────────── */
    {
        lv_obj_t *row = lv_obj_create(scr);
        lv_obj_set_size(row, 440, 60);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 220);
        lv_obj_set_style_bg_color(row, C_CARD, 0);
        lv_obj_set_style_border_color(row, C_LINE, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_set_style_pad_hor(row, 16, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *ic = lv_label_create(row);
        lv_label_set_text(ic, LV_SYMBOL_POWER);
        lv_obj_set_style_text_color(ic, C_GREEN, 0);
        lv_obj_set_style_text_font(ic, &lv_font_montserrat_20, 0);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, "Output");
        lv_obj_set_style_text_color(lbl, C_WHITE, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 36, 0);

        s_out_status_lbl = lv_label_create(row);
        lv_label_set_text(s_out_status_lbl, "OFF");
        lv_obj_set_style_text_color(s_out_status_lbl, C_RED, 0);
        lv_obj_set_style_text_font(s_out_status_lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(s_out_status_lbl, LV_ALIGN_RIGHT_MID, -90, 0);

        s_out_btn = lv_btn_create(row);
        lv_obj_set_size(s_out_btn, 80, 34);
        lv_obj_align(s_out_btn, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_color(s_out_btn, lv_color_hex(0x0d2a0d), 0);
        lv_obj_set_style_radius(s_out_btn, 8, 0);
        lv_obj_set_style_border_color(s_out_btn, C_GREEN, 0);
        lv_obj_set_style_border_width(s_out_btn, 1, 0);
        lv_obj_add_event_cb(s_out_btn, output_toggle_cb, LV_EVENT_CLICKED, NULL);
        s_out_btn_lbl = lv_label_create(s_out_btn);
        lv_label_set_text(s_out_btn_lbl, "Turn ON");
        lv_obj_set_style_text_color(s_out_btn_lbl, C_GREEN, 0);
        lv_obj_set_style_text_font(s_out_btn_lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(s_out_btn_lbl);
    }

    /* ── App download banner ─────────────────────────────────────── */
    {
        lv_obj_t *ban = lv_obj_create(scr);
        lv_obj_set_size(ban, 440, 80);
        lv_obj_align(ban, LV_ALIGN_TOP_MID, 0, 296);
        lv_obj_set_style_bg_color(ban, lv_color_hex(0x0D131C), 0);
        lv_obj_set_style_bg_opa(ban, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(ban, C_LINE, 0);
        lv_obj_set_style_border_width(ban, 1, 0);
        lv_obj_set_style_radius(ban, 12, 0);
        lv_obj_set_style_pad_all(ban, 14, 0);
        lv_obj_clear_flag(ban, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        mk_lbl(ban, LV_SYMBOL_WIFI, &lv_font_montserrat_20, C_BLUE,
               LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_t *bt = lv_label_create(ban);
        lv_label_set_text(bt, "More settings via the INVO app");
        lv_obj_set_style_text_color(bt, C_LTGRAY, 0);
        lv_obj_set_style_text_font(bt, &lv_font_montserrat_14, 0);
        lv_obj_align(bt, LV_ALIGN_LEFT_MID, 32, -8);
        lv_obj_t *bs = lv_label_create(ban);
        lv_label_set_text(bs, "Power limits, load priorities, smart devices");
        lv_obj_set_style_text_color(bs, C_GRAY, 0);
        lv_obj_set_style_text_font(bs, &lv_font_montserrat_12, 0);
        lv_obj_align(bs, LV_ALIGN_LEFT_MID, 32, 10);
    }

    return scr;
}
