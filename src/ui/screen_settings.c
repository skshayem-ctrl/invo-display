#include "ui_common.h"
#include "modbus_inverter.h"
#include "nrf_protocol.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

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

/* ── Smart Plug nRF test ──────────────────────────────────────────── */
static lv_obj_t *s_plug_ack_cb    = NULL;
static lv_obj_t *s_plug_num_lbl   = NULL;
static lv_obj_t *s_numpad_panel   = NULL;
static lv_obj_t *s_numpad_display = NULL;
static int       s_plug_num       = 1;
static char      s_numpad_buf[4]  = "1";

static void plug_send_cb(lv_event_t *e)
{
    uint8_t ack = (s_plug_ack_cb && lv_obj_has_state(s_plug_ack_cb, LV_STATE_CHECKED)) ? 1 : 0;
    nrf_send_plug_ctrl((uint8_t)s_plug_num, 1, ack);
}

static void numpad_digit_cb(lv_event_t *e)
{
    const char *d = (const char *)lv_event_get_user_data(e);
    size_t len = strlen(s_numpad_buf);
    if (len < 3) {
        s_numpad_buf[len]     = d[0];
        s_numpad_buf[len + 1] = '\0';
    }
    lv_label_set_text(s_numpad_display, s_numpad_buf);
}

static void numpad_del_cb(lv_event_t *e)
{
    size_t len = strlen(s_numpad_buf);
    if (len > 0) s_numpad_buf[len - 1] = '\0';
    lv_label_set_text(s_numpad_display, s_numpad_buf[0] ? s_numpad_buf : "_");
}

static void numpad_ok_cb(lv_event_t *e)
{
    if (s_numpad_buf[0]) {
        int v = atoi(s_numpad_buf);
        if (v >= 1 && v <= 127) {
            s_plug_num = v;
            char t[4];
            snprintf(t, sizeof(t), "%d", s_plug_num);
            lv_label_set_text(s_plug_num_lbl, t);
        }
    }
    lv_obj_add_flag(s_numpad_panel, LV_OBJ_FLAG_HIDDEN);
}

static void numpad_open_cb(lv_event_t *e)
{
    snprintf(s_numpad_buf, sizeof(s_numpad_buf), "%d", s_plug_num);
    lv_label_set_text(s_numpad_display, s_numpad_buf);
    lv_obj_clear_flag(s_numpad_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_numpad_panel);
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

    /* ── nRF Smart Plug test row ─────────────────────────────────── */
    {
        lv_obj_t *row = lv_obj_create(scr);
        lv_obj_set_size(row, 440, 60);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 292);
        lv_obj_set_style_bg_color(row, C_CARD, 0);
        lv_obj_set_style_border_color(row, C_LINE, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_set_style_pad_hor(row, 16, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *ic = lv_label_create(row);
        lv_label_set_text(ic, LV_SYMBOL_BLUETOOTH);
        lv_obj_set_style_text_color(ic, C_PURPLE, 0);
        lv_obj_set_style_text_font(ic, &lv_font_montserrat_20, 0);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, "Smart Plug");
        lv_obj_set_style_text_color(lbl, C_WHITE, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 36, 0);

        /* tappable plug number display — opens numpad */
        lv_obj_t *num_box = lv_btn_create(row);
        lv_obj_set_size(num_box, 48, 34);
        lv_obj_align(num_box, LV_ALIGN_LEFT_MID, 148, 0);
        lv_obj_set_style_bg_color(num_box, C_DGRAY, 0);
        lv_obj_set_style_border_color(num_box, C_PURPLE, 0);
        lv_obj_set_style_border_width(num_box, 1, 0);
        lv_obj_set_style_radius(num_box, 8, 0);
        lv_obj_set_style_pad_all(num_box, 0, 0);
        lv_obj_add_event_cb(num_box, numpad_open_cb, LV_EVENT_CLICKED, NULL);
        s_plug_num_lbl = lv_label_create(num_box);
        lv_label_set_text(s_plug_num_lbl, "1");
        lv_obj_set_style_text_color(s_plug_num_lbl, C_WHITE, 0);
        lv_obj_set_style_text_font(s_plug_num_lbl, &lv_font_montserrat_16, 0);
        lv_obj_center(s_plug_num_lbl);

        s_plug_ack_cb = lv_checkbox_create(row);
        lv_checkbox_set_text(s_plug_ack_cb, "ACK");
        lv_obj_set_style_text_color(s_plug_ack_cb, C_GRAY, LV_PART_MAIN);
        lv_obj_set_style_text_font(s_plug_ack_cb, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_plug_ack_cb, C_DGRAY, LV_PART_INDICATOR);
        lv_obj_set_style_border_color(s_plug_ack_cb, C_GRAY, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s_plug_ack_cb, C_PURPLE, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(s_plug_ack_cb, C_PURPLE, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_align(s_plug_ack_cb, LV_ALIGN_RIGHT_MID, -100, 0);

        lv_obj_t *btn = lv_btn_create(row);
        lv_obj_set_size(btn, 82, 34);
        lv_obj_align(btn, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x0d2a0d), 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_border_color(btn, C_GREEN, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_add_event_cb(btn, plug_send_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *blbl = lv_label_create(btn);
        lv_label_set_text(blbl, "Send ON");
        lv_obj_set_style_text_color(blbl, C_GREEN, 0);
        lv_obj_set_style_text_font(blbl, &lv_font_montserrat_12, 0);
        lv_obj_center(blbl);
    }

    /* ── App download banner ─────────────────────────────────────── */
    {
        lv_obj_t *ban = lv_obj_create(scr);
        lv_obj_set_size(ban, 440, 80);
        lv_obj_align(ban, LV_ALIGN_TOP_MID, 0, 364);
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

    /* ── Numpad overlay (hidden until plug number box tapped) ──────── */
    s_numpad_panel = lv_obj_create(scr);
    lv_obj_set_size(s_numpad_panel, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(s_numpad_panel, 0, 0);
    lv_obj_set_style_bg_color(s_numpad_panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_numpad_panel, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_numpad_panel, 0, 0);
    lv_obj_set_style_pad_all(s_numpad_panel, 0, 0);
    lv_obj_clear_flag(s_numpad_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_numpad_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *np_card = lv_obj_create(s_numpad_panel);
    lv_obj_set_size(np_card, 280, 316);
    lv_obj_align(np_card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(np_card, C_CARD, 0);
    lv_obj_set_style_border_color(np_card, C_PURPLE, 0);
    lv_obj_set_style_border_width(np_card, 1, 0);
    lv_obj_set_style_radius(np_card, 16, 0);
    lv_obj_set_style_pad_all(np_card, 12, 0);
    lv_obj_clear_flag(np_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *np_title = lv_label_create(np_card);
    lv_label_set_text(np_title, "Plug Number");
    lv_obj_set_style_text_color(np_title, C_PURPLE, 0);
    lv_obj_set_style_text_font(np_title, &lv_font_montserrat_14, 0);
    lv_obj_align(np_title, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *np_disp = lv_obj_create(np_card);
    lv_obj_set_size(np_disp, 256, 44);
    lv_obj_align(np_disp, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_set_style_bg_color(np_disp, C_DGRAY, 0);
    lv_obj_set_style_border_color(np_disp, C_PURPLE, 0);
    lv_obj_set_style_border_width(np_disp, 1, 0);
    lv_obj_set_style_radius(np_disp, 8, 0);
    lv_obj_set_style_pad_all(np_disp, 0, 0);
    lv_obj_clear_flag(np_disp, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    s_numpad_display = lv_label_create(np_disp);
    lv_label_set_text(s_numpad_display, "1");
    lv_obj_set_style_text_color(s_numpad_display, C_WHITE, 0);
    lv_obj_set_style_text_font(s_numpad_display, &lv_font_montserrat_20, 0);
    lv_obj_center(s_numpad_display);

    /* 3×4 button grid: digits 1-9, then DEL / 0 / OK */
    static const char * const np_keys[4][3] = {
        { "1", "2", "3" },
        { "4", "5", "6" },
        { "7", "8", "9" },
        { "DEL", "0", "OK" },
    };
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 3; c++) {
            const char *k = np_keys[r][c];
            lv_obj_t *kb = lv_btn_create(np_card);
            lv_obj_set_size(kb, 81, 50);
            lv_obj_set_pos(kb, c * 87, 74 + r * 56);
            lv_obj_set_style_radius(kb, 8, 0);
            lv_obj_set_style_border_width(kb, 1, 0);
            if (k[0] == 'O') {                              /* OK */
                lv_obj_set_style_bg_color(kb, lv_color_hex(0x0d2a0d), 0);
                lv_obj_set_style_border_color(kb, C_GREEN, 0);
                lv_obj_add_event_cb(kb, numpad_ok_cb, LV_EVENT_CLICKED, NULL);
            } else if (k[0] == 'D') {                      /* DEL */
                lv_obj_set_style_bg_color(kb, lv_color_hex(0x2a0d0d), 0);
                lv_obj_set_style_border_color(kb, C_RED, 0);
                lv_obj_add_event_cb(kb, numpad_del_cb, LV_EVENT_CLICKED, NULL);
            } else {                                        /* digit */
                lv_obj_set_style_bg_color(kb, C_DGRAY, 0);
                lv_obj_set_style_border_color(kb, C_LINE, 0);
                lv_obj_add_event_cb(kb, numpad_digit_cb, LV_EVENT_CLICKED, (void *)k);
            }
            lv_obj_t *kl = lv_label_create(kb);
            lv_label_set_text(kl, k);
            lv_obj_set_style_text_color(kl,
                k[0] == 'O' ? C_GREEN : k[0] == 'D' ? C_RED : C_WHITE, 0);
            lv_obj_set_style_text_font(kl, &lv_font_montserrat_16, 0);
            lv_obj_center(kl);
        }
    }

    return scr;
}
