#include <stdio.h>
#include <string.h>
#include "ui_common.h"
#include "wifi_manager.h"

/* ── module state ────────────────────────────────────────────────── */
static lv_obj_t   *s_status_lbl;
static lv_obj_t   *s_list;
static lv_obj_t   *s_pw_panel;
static lv_obj_t   *s_pw_ssid_lbl;
static lv_obj_t   *s_pw_ta;
static lv_obj_t   *s_pw_eye_lbl;
static lv_obj_t   *s_pw_status;
static lv_timer_t *s_scan_tmr;
static lv_timer_t *s_conn_tmr;
static char        s_ap_ssids[20][33];
static char        s_sel_ssid[33];   /* SSID selected for connection */

/* ── helpers ─────────────────────────────────────────────────────── */

static lv_color_t rssi_color(int8_t r)
{
    if (r >= -60) return C_GREEN;
    if (r >= -75) return C_AMBER;
    return C_RED;
}

/* ── forward declarations ────────────────────────────────────────── */
static void hide_pw_panel(void);

/* ── go-home timer ───────────────────────────────────────────────── */

static void go_home_tmr_cb(lv_timer_t *t)
{
    lv_timer_delete(t);
    hide_pw_panel();
    lv_screen_load_anim(app.scr_main, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 250, 0, false);
}

/* ── connection polling ──────────────────────────────────────────── */

static void conn_poll_cb(lv_timer_t *t)
{
    wm_state_t st = wifi_manager_conn_state();
    if (st == WM_CONNECTED) {
        lv_timer_delete(s_conn_tmr); s_conn_tmr = NULL;
        lv_label_set_text(s_pw_status, LV_SYMBOL_OK "  Connected!");
        lv_obj_set_style_text_color(s_pw_status, C_GREEN, 0);
        lv_timer_create(go_home_tmr_cb, 1500, NULL);
    } else if (st == WM_FAILED) {
        lv_timer_delete(s_conn_tmr); s_conn_tmr = NULL;
        lv_label_set_text(s_pw_status, LV_SYMBOL_WARNING "  Failed. Try again.");
        lv_obj_set_style_text_color(s_pw_status, C_RED, 0);
    }
}

static void do_connect(void)
{
    const char *pass = lv_textarea_get_text(s_pw_ta);
    lv_label_set_text(s_pw_status, "Connecting…");
    lv_obj_set_style_text_color(s_pw_status, C_GRAY, 0);
    wifi_manager_connect_to(s_sel_ssid, pass);
    if (s_conn_tmr) { lv_timer_delete(s_conn_tmr); s_conn_tmr = NULL; }
    s_conn_tmr = lv_timer_create(conn_poll_cb, 500, NULL);
}

/* ── password panel ──────────────────────────────────────────────── */

static void hide_pw_panel(void)
{
    if (s_conn_tmr) { lv_timer_delete(s_conn_tmr); s_conn_tmr = NULL; }
    lv_obj_add_flag(s_pw_panel, LV_OBJ_FLAG_HIDDEN);
}

static void show_pw_panel(const char *ssid)
{
    strncpy(s_sel_ssid, ssid, 32);
    char hdr[48];
    snprintf(hdr, sizeof(hdr), "Connect to: %s", ssid);
    lv_label_set_text(s_pw_ssid_lbl, hdr);
    lv_textarea_set_text(s_pw_ta, "");
    lv_textarea_set_password_mode(s_pw_ta, true);
    lv_label_set_text(s_pw_eye_lbl, LV_SYMBOL_EYE_CLOSE);
    lv_label_set_text(s_pw_status, "");
    lv_obj_clear_flag(s_pw_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_pw_panel);
    lv_obj_scroll_to_y(s_pw_panel, 0, LV_ANIM_OFF);
}

static void eye_toggle_cb(lv_event_t *e)
{
    bool hidden = lv_textarea_get_password_mode(s_pw_ta);
    lv_textarea_set_password_mode(s_pw_ta, !hidden);
    lv_label_set_text(s_pw_eye_lbl, hidden ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
}

static void connect_btn_cb(lv_event_t *e) { do_connect(); }
static void cancel_btn_cb(lv_event_t *e)  { hide_pw_panel(); }

static void kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if      (code == LV_EVENT_READY)  do_connect();
    else if (code == LV_EVENT_CANCEL) hide_pw_panel();
}

static void ap_click_cb(lv_event_t *e)
{
    show_pw_panel((const char *)lv_event_get_user_data(e));
}

/* ── scan poll ───────────────────────────────────────────────────── */

static void scan_poll_cb(lv_timer_t *t)
{
    if (!wifi_manager_scan_done()) return;
    lv_timer_delete(s_scan_tmr); s_scan_tmr = NULL;

    int n = wifi_manager_ap_count();
    char buf[48];
    snprintf(buf, sizeof(buf), "Found %d network%s", n, n == 1 ? "" : "s");
    lv_label_set_text(s_status_lbl, buf);
    lv_obj_clean(s_list);

    for (int i = 0; i < n && i < 20; i++) {
        int8_t rssi; bool secured;
        wifi_manager_ap_info(i, s_ap_ssids[i], &rssi, &secured);

        lv_obj_t *btn = lv_list_add_button(s_list, NULL, s_ap_ssids[i]);
        lv_obj_set_style_bg_color(btn, C_CARD, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, C_LINE, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_pad_ver(btn, 12, 0);

        lv_obj_t *lbl = lv_obj_get_child(btn, 0);
        lv_obj_set_style_text_color(lbl, C_WHITE, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);

        /* right-side signal badge */
        lv_obj_t *sig = lv_label_create(btn);
        lv_label_set_text(sig, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(sig, rssi_color(rssi), 0);
        lv_obj_set_style_text_font(sig, &lv_font_montserrat_16, 0);
        lv_obj_align(sig, LV_ALIGN_RIGHT_MID, secured ? -28 : -4, 0);

        if (secured) {
            lv_obj_t *lck = lv_label_create(btn);
            lv_label_set_text(lck, LV_SYMBOL_EYE_CLOSE);
            lv_obj_set_style_text_color(lck, C_GRAY, 0);
            lv_obj_set_style_text_font(lck, &lv_font_montserrat_14, 0);
            lv_obj_align(lck, LV_ALIGN_RIGHT_MID, -4, 0);
        }

        lv_obj_add_event_cb(btn, ap_click_cb, LV_EVENT_CLICKED, s_ap_ssids[i]);
    }

    if (n == 0) lv_list_add_text(s_list, "No networks found");
}

/* ── screen lifecycle ────────────────────────────────────────────── */

static void do_scan(void)
{
    lv_obj_clean(s_list);
    lv_label_set_text(s_status_lbl, "Scanning…");
    wifi_manager_scan_start();
    if (s_scan_tmr) { lv_timer_delete(s_scan_tmr); s_scan_tmr = NULL; }
    s_scan_tmr = lv_timer_create(scan_poll_cb, 500, NULL);
}

static void on_screen_loaded(lv_event_t *e) { do_scan(); hide_pw_panel(); }
static void on_screen_unload(lv_event_t *e)
{
    if (s_scan_tmr) { lv_timer_delete(s_scan_tmr); s_scan_tmr = NULL; }
    if (s_conn_tmr) { lv_timer_delete(s_conn_tmr); s_conn_tmr = NULL; }
}
static void rescan_cb(lv_event_t *e) { do_scan(); }
static void wifi_back_cb(lv_event_t *e)
{
    lv_screen_load_anim(app.scr_main, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 250, 0, false);
}

/* ── build ───────────────────────────────────────────────────────── */

lv_obj_t *screen_wifi_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    style_screen(scr);
    lv_obj_add_event_cb(scr, on_screen_loaded, LV_EVENT_SCREEN_LOADED,       NULL);
    lv_obj_add_event_cb(scr, on_screen_unload, LV_EVENT_SCREEN_UNLOAD_START, NULL);

    /* header */
    mk_lbl(scr, LV_SYMBOL_WIFI, &lv_font_montserrat_20, C_GREEN,
           LV_ALIGN_TOP_MID, 0, 28);
    mk_lbl(scr, "WiFi Settings", &lv_font_montserrat_16, C_GRAY,
           LV_ALIGN_TOP_MID, 0, 56);

    /* status + rescan */
    s_status_lbl = mk_lbl(scr, "Scanning…", &lv_font_montserrat_14, C_GRAY,
                          LV_ALIGN_TOP_MID, 0, 88);

    lv_obj_t *rs = lv_btn_create(scr);
    lv_obj_set_size(rs, 120, 36);
    lv_obj_align(rs, LV_ALIGN_TOP_MID, 0, 114);
    lv_obj_set_style_bg_color(rs, C_DGRAY, 0);
    lv_obj_set_style_radius(rs, 18, 0);
    lv_obj_add_event_cb(rs, rescan_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rl = lv_label_create(rs);
    lv_label_set_text(rl, LV_SYMBOL_REFRESH "  Rescan");
    lv_obj_set_style_text_color(rl, C_WHITE, 0);
    lv_obj_set_style_text_font(rl, &lv_font_montserrat_14, 0);
    lv_obj_center(rl);

    /* AP list */
    s_list = lv_list_create(scr);
    lv_obj_set_size(s_list, 500, 450);
    lv_obj_align(s_list, LV_ALIGN_TOP_MID, 0, 162);
    lv_obj_set_style_bg_color(s_list, C_BG, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 6, 0);

    /* back button */
    lv_obj_t *back = lv_btn_create(scr);
    lv_obj_set_size(back, 110, 42);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -72);
    lv_obj_set_style_bg_color(back, C_DGRAY, 0);
    lv_obj_set_style_radius(back, 21, 0);
    lv_obj_add_event_cb(back, wifi_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *br = mk_row(back); lv_obj_center(br);
    lv_obj_t *ba = lv_label_create(br);
    lv_label_set_text(ba, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(ba, C_WHITE, 0);
    lv_obj_set_style_text_font(ba, &lv_font_montserrat_16, 0);
    lv_obj_t *bbl = lv_label_create(br);
    lv_label_set_text(bbl, "Home");
    lv_obj_set_style_text_color(bbl, C_WHITE, 0);
    lv_obj_set_style_text_font(bbl, &lv_font_montserrat_16, 0);

    /* ── Password overlay (starts hidden) ──────────────────────── */
    s_pw_panel = lv_obj_create(scr);
    lv_obj_set_size(s_pw_panel, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(s_pw_panel, 0, 0);
    lv_obj_set_style_bg_color(s_pw_panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_pw_panel, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s_pw_panel, 0, 0);
    lv_obj_set_style_pad_all(s_pw_panel, 0, 0);
    lv_obj_clear_flag(s_pw_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_pw_panel, LV_OBJ_FLAG_HIDDEN);

    /* ── Compact info card (460px stays inside the circle at y=100) */
    lv_obj_t *card = lv_obj_create(s_pw_panel);
    lv_obj_set_size(card, 460, 100);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_bg_color(card, C_CARD, 0);
    lv_obj_set_style_border_color(card, C_LINE, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_hor(card, 14, 0);
    lv_obj_set_style_pad_ver(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    s_pw_ssid_lbl = lv_label_create(card);
    lv_label_set_text(s_pw_ssid_lbl, "Connect to: …");
    lv_obj_set_style_text_color(s_pw_ssid_lbl, C_AMBER, 0);
    lv_obj_set_style_text_font(s_pw_ssid_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(s_pw_ssid_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    s_pw_ta = lv_textarea_create(card);
    lv_obj_set_size(s_pw_ta, 386, 44);
    lv_obj_align(s_pw_ta, LV_ALIGN_TOP_LEFT, 0, 22);
    lv_textarea_set_one_line(s_pw_ta, true);
    lv_textarea_set_password_mode(s_pw_ta, true);
    lv_textarea_set_placeholder_text(s_pw_ta, "Enter password…");
    lv_obj_set_style_bg_color(s_pw_ta, lv_color_hex(0x141C2E), 0);
    lv_obj_set_style_border_color(s_pw_ta, C_LINE, 0);
    lv_obj_set_style_border_width(s_pw_ta, 1, 0);
    lv_obj_set_style_radius(s_pw_ta, 8, 0);
    lv_obj_set_style_text_color(s_pw_ta, C_WHITE, 0);
    lv_obj_set_style_text_font(s_pw_ta, &lv_font_montserrat_16, 0);

    lv_obj_t *eye_btn = lv_btn_create(card);
    lv_obj_set_size(eye_btn, 42, 42);
    lv_obj_align(eye_btn, LV_ALIGN_TOP_RIGHT, 0, 22);
    lv_obj_set_style_bg_color(eye_btn, lv_color_hex(0x141C2E), 0);
    lv_obj_set_style_border_color(eye_btn, C_LINE, 0);
    lv_obj_set_style_border_width(eye_btn, 1, 0);
    lv_obj_set_style_radius(eye_btn, 8, 0);
    lv_obj_set_style_pad_all(eye_btn, 0, 0);
    lv_obj_add_event_cb(eye_btn, eye_toggle_cb, LV_EVENT_CLICKED, NULL);
    s_pw_eye_lbl = lv_label_create(eye_btn);
    lv_label_set_text(s_pw_eye_lbl, LV_SYMBOL_EYE_CLOSE);
    lv_obj_set_style_text_color(s_pw_eye_lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(s_pw_eye_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(s_pw_eye_lbl);

    /* ── Cancel / Status / Connect row ─────────────────────────── */
    lv_obj_t *cancel_b = lv_btn_create(s_pw_panel);
    lv_obj_set_size(cancel_b, 128, 44);
    lv_obj_align(cancel_b, LV_ALIGN_TOP_MID, -140, 212);
    lv_obj_set_style_bg_color(cancel_b, C_DGRAY, 0);
    lv_obj_set_style_radius(cancel_b, 22, 0);
    lv_obj_set_style_border_width(cancel_b, 0, 0);
    lv_obj_add_event_cb(cancel_b, cancel_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cl = lv_label_create(cancel_b);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE "  Cancel");
    lv_obj_set_style_text_color(cl, C_WHITE, 0);
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_14, 0);
    lv_obj_center(cl);

    s_pw_status = lv_label_create(s_pw_panel);
    lv_label_set_text(s_pw_status, "");
    lv_obj_set_style_text_font(s_pw_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_pw_status, C_GRAY, 0);
    lv_obj_align(s_pw_status, LV_ALIGN_TOP_MID, 0, 220);

    lv_obj_t *conn_b = lv_btn_create(s_pw_panel);
    lv_obj_set_size(conn_b, 128, 44);
    lv_obj_align(conn_b, LV_ALIGN_TOP_MID, 140, 212);
    lv_obj_set_style_bg_color(conn_b, C_GREEN, 0);
    lv_obj_set_style_radius(conn_b, 22, 0);
    lv_obj_set_style_border_width(conn_b, 0, 0);
    lv_obj_add_event_cb(conn_b, connect_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cnl = lv_label_create(conn_b);
    lv_label_set_text(cnl, LV_SYMBOL_OK "  Connect");
    lv_obj_set_style_text_color(cnl, C_WHITE, 0);
    lv_obj_set_style_text_font(cnl, &lv_font_montserrat_14, 0);
    lv_obj_center(cnl);

    /* ── Styled keyboard — 540px wide stays inside the circle ─── */
    lv_obj_t *kb = lv_keyboard_create(s_pw_panel);
    lv_obj_set_size(kb, 540, 310);
    lv_obj_align(kb, LV_ALIGN_TOP_MID, 0, 268);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb, s_pw_ta);

    /* container */
    lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(kb, 0, 0);
    lv_obj_set_style_pad_all(kb, 4, 0);
    lv_obj_set_style_pad_row(kb, 5, 0);
    lv_obj_set_style_pad_column(kb, 5, 0);

    /* keys — normal */
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x1A2540), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 10, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, C_WHITE, LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_16, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, lv_color_hex(0x243050), LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);

    /* keys — pressed */
    lv_obj_set_style_bg_color(kb, C_BLUE, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(kb, C_WHITE, LV_PART_ITEMS | LV_STATE_PRESSED);

    /* keys — checked (Shift active) */
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x00C896), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(kb, C_WHITE, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_READY,  NULL);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_CANCEL, NULL);

    return scr;
}
