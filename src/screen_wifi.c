#include <string.h>
#include "ui_common.h"
#include "wifi_manager.h"

/* ── screens ─────────────────────────────── */
static lv_obj_t * list_scr;
static lv_obj_t * pwd_scr;

/* ── list screen widgets ─────────────────── */
static lv_obj_t * list_widget;
static lv_obj_t * scan_spin;
static lv_obj_t * scan_lbl;

/* ── password screen widgets ─────────────── */
static lv_obj_t * pwd_ssid_lbl;
static lv_obj_t * pwd_ta;
static lv_obj_t * pwd_status_lbl;
static lv_obj_t * eye_lbl;

/* ── custom keyboard (4 rows sized to fit the 720×720 round display) ─
 * Display: 720×720, center=(360,360), R=360.
 * Constraint: bottom outer corner (360+w/2, y+h) must be inside circle.
 *   Row 1: y=365, h=55, w=670 → corner dist ≈ 340 < 360 ✓ (20px margin)
 *   Row 2: y=424, h=55, w=640 → corner dist ≈ 341 < 360 ✓
 *   Row 3: y=483, h=55, w=585 → corner dist ≈ 343 < 360 ✓
 *   Row 4: y=542, h=55, w=500 → corner dist ≈ 345 < 360 ✓
 */
#define KB_ROW_H  55
#define KB_Y1     365
#define KB_Y2     424
#define KB_Y3     483
#define KB_Y4     542
#define KB_W1     670
#define KB_W2     640
#define KB_W3     585
#define KB_W4     500

static lv_obj_t * kb_row[4];
static bool       kb_upper = false;
static bool       kb_num   = false;

static const char * const lower_r1[] = {"q","w","e","r","t","y","u","i","o","p",""};
static const char * const lower_r2[] = {"a","s","d","f","g","h","j","k","l",""};
static const char * const lower_r3[] = {LV_SYMBOL_UP,"z","x","c","v","b","n","m",LV_SYMBOL_BACKSPACE,""};
static const char * const upper_r1[] = {"Q","W","E","R","T","Y","U","I","O","P",""};
static const char * const upper_r2[] = {"A","S","D","F","G","H","J","K","L",""};
static const char * const upper_r3[] = {LV_SYMBOL_UP,"Z","X","C","V","B","N","M",LV_SYMBOL_BACKSPACE,""};
static const char * const num_r1[]   = {"1","2","3","4","5","6","7","8","9","0",""};
static const char * const num_r2[]   = {"-","_","@","#","!","?","/",".",",",""};
static const char * const num_r3[]   = {"(",")","+","=","$","%","&","*",LV_SYMBOL_BACKSPACE,""};
static const char * const bot_alpha[] = {"123"," ",LV_SYMBOL_OK,""};
static const char * const bot_num[]   = {"ABC"," ",LV_SYMBOL_OK,""};

/* ── scan display list (deduped, built after each scan) ────── */
#define MAX_DISP_APS 20

typedef struct {
    char ssid[33];
    int  pct;       /* signal 0-100 (rssi+100 for ESP32, direct % for Pi) */
    bool secured;
} disp_ap_t;

static disp_ap_t s_aps[MAX_DISP_APS];
static int       s_ap_count    = 0;
static char      s_conn_ssid[33] = {0}; /* currently connected SSID */
static char      s_sel_ssid[33]  = {0}; /* SSID the user tapped */
static bool      s_show_pwd      = false;

static lv_timer_t * scan_poll_tmr = NULL;
static lv_timer_t * conn_poll_tmr = NULL;

/* ── forward ─────────────────────────────── */
static void do_connect(void);
static void populate_list(void);

/* ── keyboard ────────────────────────────── */

static void kb_apply_widths(void)
{
    lv_buttonmatrix_set_button_width(kb_row[2], 0, 2);
    lv_buttonmatrix_set_button_width(kb_row[2], 8, 2);
    lv_buttonmatrix_set_button_width(kb_row[3], 0, 2);
    lv_buttonmatrix_set_button_width(kb_row[3], 1, 6);
    lv_buttonmatrix_set_button_width(kb_row[3], 2, 2);
}

static void kb_update_maps(void)
{
    if (kb_num) {
        lv_buttonmatrix_set_map(kb_row[0], num_r1);
        lv_buttonmatrix_set_map(kb_row[1], num_r2);
        lv_buttonmatrix_set_map(kb_row[2], num_r3);
        lv_buttonmatrix_set_map(kb_row[3], bot_num);
    } else {
        lv_buttonmatrix_set_map(kb_row[0], kb_upper ? upper_r1 : lower_r1);
        lv_buttonmatrix_set_map(kb_row[1], kb_upper ? upper_r2 : lower_r2);
        lv_buttonmatrix_set_map(kb_row[2], kb_upper ? upper_r3 : lower_r3);
        lv_buttonmatrix_set_map(kb_row[3], bot_alpha);
    }
    kb_apply_widths();

    lv_buttonmatrix_set_button_ctrl(kb_row[2], 0, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    if (!kb_num && kb_upper)
        lv_buttonmatrix_set_button_ctrl(kb_row[2], 0, LV_BUTTONMATRIX_CTRL_CHECKED);
    else
        lv_buttonmatrix_clear_button_ctrl(kb_row[2], 0, LV_BUTTONMATRIX_CTRL_CHECKED);
}

static void kb_row_event_cb(lv_event_t * e)
{
    lv_obj_t * bm  = lv_event_get_target(e);
    uint32_t   idx = lv_buttonmatrix_get_selected_button(bm);
    if (idx == LV_BUTTONMATRIX_BUTTON_NONE) return;
    const char * txt = lv_buttonmatrix_get_button_text(bm, idx);
    if (!txt) return;

    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        lv_textarea_delete_char(pwd_ta);
    } else if (strcmp(txt, LV_SYMBOL_UP) == 0) {
        kb_upper = !kb_upper;
        kb_update_maps();
    } else if (strcmp(txt, "123") == 0) {
        kb_num = true;
        kb_update_maps();
    } else if (strcmp(txt, "ABC") == 0) {
        kb_num = false; kb_upper = false;
        kb_update_maps();
    } else if (strcmp(txt, LV_SYMBOL_OK) == 0) {
        do_connect();
    } else {
        lv_textarea_add_text(pwd_ta, txt);
        if (kb_upper && !kb_num) {
            kb_upper = false;
            kb_update_maps();
        }
    }
}

/* ── connection polling ──────────────────── */

static void go_home_tmr_cb(lv_timer_t * t)
{
    (void)t;
    lv_screen_load_anim(app.scr_main, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 250, 0, false);
}

static void conn_poll_cb(lv_timer_t * t)
{
    wm_state_t st = wifi_manager_conn_state();
    if (st == WM_CONNECTING) return;
    lv_timer_delete(t);
    conn_poll_tmr = NULL;

    if (st == WM_CONNECTED) {
        lv_label_set_text(pwd_status_lbl, LV_SYMBOL_OK "  Connected!");
        lv_obj_set_style_text_color(pwd_status_lbl, C_GREEN, 0);
        lv_timer_t * gt = lv_timer_create(go_home_tmr_cb, 1500, NULL);
        lv_timer_set_repeat_count(gt, 1);
    } else {
        lv_label_set_text(pwd_status_lbl, LV_SYMBOL_CLOSE "  Failed — check password");
        lv_obj_set_style_text_color(pwd_status_lbl, lv_color_hex(0xff4444), 0);
    }
}

static void do_connect(void)
{
    const char * pw = lv_textarea_get_text(pwd_ta);
    lv_label_set_text(pwd_status_lbl, LV_SYMBOL_REFRESH "  Connecting...");
    lv_obj_set_style_text_color(pwd_status_lbl, C_BLUE, 0);
    wifi_manager_connect_to(s_sel_ssid, pw);
    if (conn_poll_tmr) { lv_timer_delete(conn_poll_tmr); conn_poll_tmr = NULL; }
    conn_poll_tmr = lv_timer_create(conn_poll_cb, 500, NULL);
}

/* ── list population ─────────────────────── */

static void net_btn_cb(lv_event_t * e)
{
    const char * ssid = (const char *)lv_event_get_user_data(e);
    memcpy(s_sel_ssid, ssid, 33);

    char hdr[80];
    lv_snprintf(hdr, sizeof(hdr), LV_SYMBOL_WIFI "  %s", s_sel_ssid);
    lv_label_set_text(pwd_ssid_lbl, hdr);
    lv_textarea_set_text(pwd_ta, "");
    lv_label_set_text(pwd_status_lbl, "");

    s_show_pwd = false;
    lv_textarea_set_password_mode(pwd_ta, true);
    lv_label_set_text(eye_lbl, LV_SYMBOL_EYE_CLOSE);
    kb_upper = false; kb_num = false;
    kb_update_maps();

    lv_screen_load_anim(pwd_scr, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

static void populate_list(void)
{
    lv_obj_add_flag(scan_spin, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scan_lbl,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(list_widget);

    if (s_ap_count == 0) {
        lv_obj_t * t = lv_list_add_text(list_widget, "No networks found");
        lv_obj_set_style_text_color(t, C_GRAY, 0);
        return;
    }

    /* Sort: connected network first */
    if (s_conn_ssid[0]) {
        for (int i = 1; i < s_ap_count; i++) {
            if (strcmp(s_aps[i].ssid, s_conn_ssid) == 0) {
                disp_ap_t tmp = s_aps[0];
                s_aps[0] = s_aps[i];
                s_aps[i] = tmp;
                break;
            }
        }
    }

    for (int i = 0; i < s_ap_count; i++) {
        bool is_conn = (s_conn_ssid[0] && strcmp(s_aps[i].ssid, s_conn_ssid) == 0);
        char txt[80];
        lv_snprintf(txt, sizeof(txt), "%-24s%3d%%", s_aps[i].ssid, s_aps[i].pct);
        const char * icon = is_conn           ? LV_SYMBOL_OK
                          : s_aps[i].secured  ? LV_SYMBOL_CHARGE : NULL;
        lv_obj_t * btn = lv_list_add_button(list_widget, icon, txt);
        lv_obj_set_height(btn, 66);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x141414), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a2a1a), LV_STATE_PRESSED);
        lv_obj_set_style_text_color(btn, is_conn ? C_GREEN : C_WHITE, 0);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_16, 0);
        lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x2a2a2a), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_add_event_cb(btn, net_btn_cb, LV_EVENT_CLICKED, s_aps[i].ssid);
    }
}

/* ── scan polling ────────────────────────── */

static void scan_poll_cb(lv_timer_t * t)
{
    if (!wifi_manager_scan_done()) return;
    lv_timer_delete(t);
    scan_poll_tmr = NULL;

    /* Get connected SSID */
    wifi_manager_connected_ssid(s_conn_ssid);

    /* Build deduped display list from wifi_manager results */
    s_ap_count = 0;
    int n = wifi_manager_ap_count();
    for (int i = 0; i < n; i++) {
        char ssid[33]; int8_t rssi; bool secured;
        if (!wifi_manager_ap_info(i, ssid, &rssi, &secured)) continue;
        int pct = (int)rssi + 100;
        if (pct < 0)   pct = 0;
        if (pct > 100) pct = 100;

        /* Dedup: keep best signal per SSID (Pi already deduped, ESP32 may not) */
        bool dup = false;
        for (int j = 0; j < s_ap_count; j++) {
            if (strcmp(s_aps[j].ssid, ssid) == 0) {
                if (pct > s_aps[j].pct) s_aps[j].pct = pct;
                dup = true; break;
            }
        }
        if (dup || s_ap_count >= MAX_DISP_APS) continue;

        memcpy(s_aps[s_ap_count].ssid, ssid, 33);
        s_aps[s_ap_count].pct      = pct;
        s_aps[s_ap_count].secured  = secured;
        s_ap_count++;
    }

    populate_list();
}

static void start_scan(void)
{
    if (scan_poll_tmr) { lv_timer_delete(scan_poll_tmr); scan_poll_tmr = NULL; }
    lv_obj_clean(list_widget);
    lv_obj_clear_flag(scan_spin, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(scan_lbl,  LV_OBJ_FLAG_HIDDEN);
    wifi_manager_scan_start();
    scan_poll_tmr = lv_timer_create(scan_poll_cb, 500, NULL);
}

/* ── event callbacks ─────────────────────── */

static void list_loaded_cb(lv_event_t * e) { (void)e; start_scan(); }
static void list_unload_cb(lv_event_t * e)
{
    (void)e;
    if (scan_poll_tmr) { lv_timer_delete(scan_poll_tmr); scan_poll_tmr = NULL; }
}
static void refresh_btn_cb(lv_event_t * e)  { (void)e; start_scan(); }
static void connect_btn_cb(lv_event_t * e)  { (void)e; do_connect(); }
static void back_to_home_cb(lv_event_t * e) {
    (void)e;
    lv_screen_load_anim(app.scr_main, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 250, 0, false);
}
static void back_to_list_cb(lv_event_t * e) {
    (void)e;
    if (conn_poll_tmr) { lv_timer_delete(conn_poll_tmr); conn_poll_tmr = NULL; }
    lv_screen_load_anim(list_scr, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}
static void eye_cb(lv_event_t * e) {
    (void)e;
    s_show_pwd = !s_show_pwd;
    lv_textarea_set_password_mode(pwd_ta, !s_show_pwd);
    lv_label_set_text(eye_lbl, s_show_pwd ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
}

/* ── back button helper (BOTTOM_MID -185,-80 → inside circle ✓) */
static void make_back_btn(lv_obj_t * scr, lv_event_cb_t cb)
{
    lv_obj_t * btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 60, 60);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, -185, -80);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_radius(btn, 30, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lbl, C_WHITE, 0);
    lv_obj_center(lbl);
}

/* ── build list screen ───────────────────── */

static void build_list_screen(void)
{
    lv_obj_set_style_bg_color(list_scr, C_BG, 0);
    lv_obj_set_style_bg_opa(list_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(list_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(list_scr, list_loaded_cb, LV_EVENT_SCREEN_LOADED,       NULL);
    lv_obj_add_event_cb(list_scr, list_unload_cb, LV_EVENT_SCREEN_UNLOAD_START, NULL);

    lv_obj_t * title = lv_label_create(list_scr);
    lv_label_set_text(title, LV_SYMBOL_WIFI "  WiFi Networks");
    lv_obj_set_style_text_color(title, C_WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, -40, 58);

    lv_obj_t * ref = lv_btn_create(list_scr);
    lv_obj_set_size(ref, 52, 36);
    lv_obj_align(ref, LV_ALIGN_TOP_MID, +105, 54);
    lv_obj_set_style_bg_color(ref, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_border_color(ref, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(ref, 1, 0);
    lv_obj_set_style_radius(ref, 8, 0);
    lv_obj_add_event_cb(ref, refresh_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * rl = lv_label_create(ref);
    lv_label_set_text(rl, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(rl, C_GREEN, 0);
    lv_obj_center(rl);

    list_widget = lv_list_create(list_scr);
    lv_obj_set_size(list_widget, 490, 548);
    lv_obj_align(list_widget, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_bg_color(list_widget, lv_color_hex(0x0d0d0d), 0);
    lv_obj_set_style_border_color(list_widget, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(list_widget, 1, 0);
    lv_obj_set_style_radius(list_widget, 14, 0);
    lv_obj_set_style_pad_all(list_widget, 0, 0);

    scan_spin = lv_spinner_create(list_scr);
    lv_spinner_set_anim_params(scan_spin, 1000, 60);
    lv_obj_set_size(scan_spin, 56, 56);
    lv_obj_align(scan_spin, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_arc_color(scan_spin, C_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(scan_spin, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(scan_spin, lv_color_hex(0x1a2a1a), LV_PART_MAIN);
    lv_obj_set_style_arc_width(scan_spin, 5, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scan_spin, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_clear_flag(scan_spin, LV_OBJ_FLAG_CLICKABLE);

    scan_lbl = lv_label_create(list_scr);
    lv_label_set_text(scan_lbl, "Scanning...");
    lv_obj_set_style_text_color(scan_lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(scan_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(scan_lbl, LV_ALIGN_CENTER, 0, 78);

    make_back_btn(list_scr, back_to_home_cb);
}

/* ── keyboard row style ──────────────────── */

static void style_kb_row(lv_obj_t * bm)
{
    lv_obj_set_style_bg_color(bm, C_BG, 0);
    lv_obj_set_style_border_width(bm, 0, 0);
    lv_obj_set_style_pad_row(bm, 0, 0);
    lv_obj_set_style_pad_column(bm, 4, 0);
    lv_obj_set_style_pad_all(bm, 0, 0);

    lv_obj_set_style_bg_color(bm, lv_color_hex(0x242424), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(bm, lv_color_hex(0x3a3a3a), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(bm, C_GREEN,                LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(bm, C_WHITE,              LV_PART_ITEMS);
    lv_obj_set_style_text_color(bm, lv_color_hex(0x000000), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_radius(bm, 8, LV_PART_ITEMS);
    lv_obj_set_style_border_width(bm, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_opa(bm, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_text_font(bm, &lv_font_montserrat_16, LV_PART_ITEMS);
}

/* ── build password screen ───────────────── */

static void build_pwd_screen(void)
{
    lv_obj_set_style_bg_color(pwd_scr, C_BG, 0);
    lv_obj_set_style_bg_opa(pwd_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(pwd_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Layout for 720×720 display, center=(360,360):
     *   y= 50  "Connect to WiFi" header
     *   y= 70  SSID name
     *   y=118  Password textarea  (h=48, bottom y=166)
     *   y=178  Connect button     (h=40, bottom y=218)
     *   y=230  Status label       (clearly above back button)
     *   CENTER,0,-20 = y=323-357  Back button (h=34)
     *   Keyboard rows start at y=365 (8px gap after back button)
     */
    lv_obj_t * hdr = lv_label_create(pwd_scr);
    lv_label_set_text(hdr, "Connect to WiFi");
    lv_obj_set_style_text_color(hdr, C_GRAY, 0);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_14, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 50);

    pwd_ssid_lbl = lv_label_create(pwd_scr);
    lv_label_set_text(pwd_ssid_lbl, "");
    lv_obj_set_style_text_color(pwd_ssid_lbl, C_WHITE, 0);
    lv_obj_set_style_text_font(pwd_ssid_lbl, &lv_font_montserrat_20, 0);
    lv_label_set_long_mode(pwd_ssid_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(pwd_ssid_lbl, 380);
    lv_obj_set_style_text_align(pwd_ssid_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(pwd_ssid_lbl, LV_ALIGN_TOP_MID, 0, 70);

    pwd_ta = lv_textarea_create(pwd_scr);
    lv_obj_set_size(pwd_ta, 330, 48);
    lv_obj_align(pwd_ta, LV_ALIGN_TOP_MID, -25, 118);
    lv_textarea_set_password_mode(pwd_ta, true);
    lv_textarea_set_one_line(pwd_ta, true);
    lv_textarea_set_placeholder_text(pwd_ta, "Password");
    lv_obj_set_style_bg_color(pwd_ta, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_color(pwd_ta, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(pwd_ta, 1, 0);
    lv_obj_set_style_radius(pwd_ta, 10, 0);
    lv_obj_set_style_text_color(pwd_ta, C_WHITE, 0);

    lv_obj_t * eye_btn = lv_btn_create(pwd_scr);
    lv_obj_set_size(eye_btn, 44, 48);
    lv_obj_align_to(eye_btn, pwd_ta, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
    lv_obj_set_style_bg_color(eye_btn, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_color(eye_btn, lv_color_hex(0x2a2a2a), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(eye_btn, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(eye_btn, 1, 0);
    lv_obj_set_style_radius(eye_btn, 10, 0);
    lv_obj_set_style_shadow_opa(eye_btn, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(eye_btn, eye_cb, LV_EVENT_CLICKED, NULL);
    eye_lbl = lv_label_create(eye_btn);
    lv_label_set_text(eye_lbl, LV_SYMBOL_EYE_CLOSE);
    lv_obj_set_style_text_color(eye_lbl, C_GRAY, 0);
    lv_obj_center(eye_lbl);

    lv_obj_t * conn = lv_btn_create(pwd_scr);
    lv_obj_set_size(conn, 160, 40);
    lv_obj_align(conn, LV_ALIGN_TOP_MID, 0, 178);
    lv_obj_set_style_bg_color(conn, C_GREEN, 0);
    lv_obj_set_style_bg_color(conn, lv_color_hex(0x00c060), LV_STATE_PRESSED);
    lv_obj_set_style_radius(conn, 10, 0);
    lv_obj_add_event_cb(conn, connect_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * cl = lv_label_create(conn);
    lv_label_set_text(cl, "Connect");
    lv_obj_set_style_text_color(cl, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_16, 0);
    lv_obj_center(cl);

    /* Status label — clearly visible at y=230, well above back button (y=323) */
    pwd_status_lbl = lv_label_create(pwd_scr);
    lv_label_set_text(pwd_status_lbl, "");
    lv_obj_set_style_text_font(pwd_status_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(pwd_status_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(pwd_status_lbl, 400);
    lv_obj_align(pwd_status_lbl, LV_ALIGN_TOP_MID, 0, 230);

    /* ← Networks back button — CENTER,0,-20 → center y=340, y=323-357 */
    lv_obj_t * back_btn = lv_btn_create(pwd_scr);
    lv_obj_set_size(back_btn, 130, 34);
    lv_obj_align(back_btn, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_border_color(back_btn, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_add_event_cb(back_btn, back_to_list_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * bl = lv_label_create(back_btn);
    lv_label_set_text(bl, LV_SYMBOL_LEFT "  Networks");
    lv_obj_set_style_text_color(bl, C_GRAY, 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
    lv_obj_center(bl);

    /* Custom round keyboard — 4 rows */
    static const int ys[] = { KB_Y1, KB_Y2, KB_Y3, KB_Y4 };
    static const int ws[] = { KB_W1, KB_W2, KB_W3, KB_W4 };
    for (int i = 0; i < 4; i++) {
        kb_row[i] = lv_buttonmatrix_create(pwd_scr);
        lv_obj_set_size(kb_row[i], ws[i], KB_ROW_H);
        lv_obj_align(kb_row[i], LV_ALIGN_TOP_MID, 0, ys[i]);
        style_kb_row(kb_row[i]);
        lv_obj_add_event_cb(kb_row[i], kb_row_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    kb_update_maps();
}

/* ── public API ──────────────────────────── */

lv_obj_t *screen_wifi_create(void)
{
    list_scr = lv_obj_create(NULL);
    pwd_scr  = lv_obj_create(NULL);
    build_list_screen();
    build_pwd_screen();
    return list_scr;
}
