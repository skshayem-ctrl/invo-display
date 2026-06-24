#include "invo_wifi.h"
#include "invo_screens.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_NETS 20
#define SSID_LEN 64

typedef struct {
    char ssid[SSID_LEN];
    int  signal;
    bool secured;
} wifi_net_t;

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

/* ── custom keyboard (4 rows) ────────────── */
/*
 * Row geometry — each row centered at x=400, all corners verified inside
 * the 800×800 circle (radius 400, center (400,400)):
 *
 *   Row 1: y=410, h=65, w=760  → corner dist ≈ 388px < 400 ✓
 *   Row 2: y=480, h=65, w=720  → corner dist ≈ 388px < 400 ✓
 *   Row 3: y=550, h=65, w=650  → corner dist ≈ 390px < 400 ✓
 *   Row 4: y=620, h=65, w=540  → corner dist ≈ 393px < 400 ✓
 */
#define KB_ROW_H  65
#define KB_Y1     410
#define KB_Y2     480
#define KB_Y3     550
#define KB_Y4     620
#define KB_W1     760
#define KB_W2     720
#define KB_W3     650
#define KB_W4     540

static lv_obj_t * kb_row[4];
static bool       kb_upper = false;
static bool       kb_num   = false;

/* Alpha maps */
static const char * const lower_r1[] = {"q","w","e","r","t","y","u","i","o","p",""};
static const char * const lower_r2[] = {"a","s","d","f","g","h","j","k","l",""};
static const char * const lower_r3[] = {LV_SYMBOL_UP,"z","x","c","v","b","n","m",LV_SYMBOL_BACKSPACE,""};
static const char * const upper_r1[] = {"Q","W","E","R","T","Y","U","I","O","P",""};
static const char * const upper_r2[] = {"A","S","D","F","G","H","J","K","L",""};
static const char * const upper_r3[] = {LV_SYMBOL_UP,"Z","X","C","V","B","N","M",LV_SYMBOL_BACKSPACE,""};

/* Number/symbol maps */
static const char * const num_r1[]   = {"1","2","3","4","5","6","7","8","9","0",""};
static const char * const num_r2[]   = {"-","_","@","#","!","?","/",".",",",""};
static const char * const num_r3[]   = {"(",")","+","=","$","%","&","*",LV_SYMBOL_BACKSPACE,""};

/* Bottom row */
static const char * const bot_alpha[] = {"123"," ",LV_SYMBOL_OK,""};
static const char * const bot_num[]   = {"ABC"," ",LV_SYMBOL_OK,""};

/* ── shared state ────────────────────────── */
static wifi_net_t nets[MAX_NETS];
static int        net_count  = 0;
static char       sel_ssid[SSID_LEN];
static char       connected_ssid[SSID_LEN] = {0};
static bool       show_pwd = false;

typedef enum { ST_IDLE, ST_RUNNING, ST_OK, ST_FAIL } async_st_t;
static void do_connect(void); /* forward declaration for kb_row_event_cb */

static volatile async_st_t scan_st = ST_IDLE;
static volatile async_st_t conn_st = ST_IDLE;
static char conn_cmd[512];
static lv_timer_t * scan_poll_tmr = NULL;

/* ── WiFi scan ───────────────────────────── */
static void do_scan(void) {
    net_count = 0;
    connected_ssid[0] = '\0';

    FILE * fp = popen(
        "nmcli --escape no -t -f ACTIVE,SSID,SIGNAL,SECURITY device wifi list"
        " --rescan yes 2>/dev/null",
        "r");
    if (!fp) return;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;

        char * p0 = strchr(line, ':');
        if (!p0) continue;
        *p0 = '\0';
        bool active = (strcmp(line, "yes") == 0);
        char * rest = p0 + 1;

        char * p1 = strrchr(rest, ':');
        if (!p1) continue;
        *p1 = '\0';
        const char * sec = p1 + 1;

        char * p2 = strrchr(rest, ':');
        if (!p2) continue;
        *p2 = '\0';
        int sig = atoi(p2 + 1);

        if (rest[0] == '\0') continue;

        if (active) {
            strncpy(connected_ssid, rest, SSID_LEN - 1);
            connected_ssid[SSID_LEN - 1] = '\0';
        }

        /* Deduplicate: keep best signal per SSID */
        bool dup = false;
        for (int j = 0; j < net_count; j++) {
            if (strcmp(nets[j].ssid, rest) == 0) {
                if (sig > nets[j].signal) nets[j].signal = sig;
                dup = true;
                break;
            }
        }
        if (dup || net_count >= MAX_NETS) continue;

        strncpy(nets[net_count].ssid, rest, SSID_LEN - 1);
        nets[net_count].ssid[SSID_LEN - 1] = '\0';
        nets[net_count].signal  = sig;
        nets[net_count].secured = (strcmp(sec, "--") != 0 && sec[0] != '\0');
        net_count++;
    }
    pclose(fp);
}

static void * scan_thread(void * arg) {
    (void)arg;
    do_scan();
    scan_st = ST_OK;
    return NULL;
}

/* ── connect thread ──────────────────────── */
static char conn_result_msg[256] = {0};

static void * conn_thread(void * arg) {
    (void)arg;
    FILE * fp = popen(conn_cmd, "r");
    if (!fp) {
        snprintf(conn_result_msg, sizeof(conn_result_msg), "popen failed");
        conn_st = ST_FAIL;
        return NULL;
    }
    /* Read ALL output lines (nmcli can print multiple before the result) */
    char full[1024] = {0};
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        size_t used = strlen(full);
        size_t avail = sizeof(full) - used - 1;
        if (avail > 0) strncat(full, line, avail);
    }
    int r = pclose(fp);

    /* Write debug log for diagnosis */
    FILE * dbg = fopen("/tmp/invo_wifi.log", "w");
    if (dbg) {
        fprintf(dbg, "CMD: %s\nEXIT: %d\nOUT:\n%s\n", conn_cmd, r, full);
        fclose(dbg);
    }

    if (r == 0 || strstr(full, "successfully")) {
        conn_st = ST_OK;
    } else {
        /* Strip ANSI escapes and keep first meaningful line for display */
        char * nl = strchr(full, '\n');
        if (nl) *nl = '\0';
        /* Skip leading ESC sequences */
        char * p = full;
        while (*p == '\033' || *p == '[' || (*p >= '0' && *p <= '9') || *p == 'K')
            p++;
        snprintf(conn_result_msg, sizeof(conn_result_msg), "%.200s", p[0] ? p : "Failed");
        conn_st = ST_FAIL;
    }
    return NULL;
}

static void spawn_detached(void * (*fn)(void *)) {
    pthread_t th;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&th, &attr, fn, NULL);
    pthread_attr_destroy(&attr);
}

/* ── keyboard logic ──────────────────────── */
static void kb_apply_widths(void) {
    /* Row 3: shift(2) + 7 letters(1 each) + bksp(2) = 11 units */
    lv_buttonmatrix_set_button_width(kb_row[2], 0, 2);
    lv_buttonmatrix_set_button_width(kb_row[2], 8, 2);
    /* Row 4: mode(2) + space(6) + ok(2) = 10 units */
    lv_buttonmatrix_set_button_width(kb_row[3], 0, 2);
    lv_buttonmatrix_set_button_width(kb_row[3], 1, 6);
    lv_buttonmatrix_set_button_width(kb_row[3], 2, 2);
}

static void kb_update_maps(void) {
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

    /* Shift button: checkable + visually active when caps on */
    lv_buttonmatrix_set_button_ctrl(kb_row[2], 0, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    if (!kb_num && kb_upper) {
        lv_buttonmatrix_set_button_ctrl(kb_row[2], 0, LV_BUTTONMATRIX_CTRL_CHECKED);
    } else {
        lv_buttonmatrix_clear_button_ctrl(kb_row[2], 0, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
}

static void kb_row_event_cb(lv_event_t * e) {
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
        kb_num   = false;
        kb_upper = false;
        kb_update_maps();
    } else if (strcmp(txt, LV_SYMBOL_OK) == 0) {
        do_connect();
    } else {
        lv_textarea_add_text(pwd_ta, txt);
        /* Auto-release shift after one character */
        if (kb_upper && !kb_num) {
            kb_upper = false;
            kb_update_maps();
        }
    }
}

/* ── list population ─────────────────────── */
static void net_btn_cb(lv_event_t * e) {
    const char * ssid = (const char *)lv_event_get_user_data(e);
    strncpy(sel_ssid, ssid, SSID_LEN - 1);
    sel_ssid[SSID_LEN - 1] = '\0';

    char hdr[80];
    lv_snprintf(hdr, sizeof(hdr), LV_SYMBOL_WIFI "  %s", sel_ssid);
    lv_label_set_text(pwd_ssid_lbl, hdr);
    lv_textarea_set_text(pwd_ta, "");
    lv_label_set_text(pwd_status_lbl, "");
    conn_st = ST_IDLE;

    /* Reset password visibility */
    show_pwd = false;
    lv_textarea_set_password_mode(pwd_ta, true);
    lv_label_set_text(eye_lbl, LV_SYMBOL_EYE_CLOSE);

    /* Reset keyboard to lowercase alpha */
    kb_upper = false;
    kb_num   = false;
    kb_update_maps();

    lv_scr_load_anim(pwd_scr, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

static void populate_list(void) {
    lv_obj_add_flag(scan_spin, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scan_lbl,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(list_widget);

    if (net_count == 0) {
        lv_obj_t * t = lv_list_add_text(list_widget, "No networks found");
        lv_obj_set_style_text_color(t, C_GRAY, 0);
        return;
    }

    /* Sort: connected network first */
    if (connected_ssid[0]) {
        for (int i = 1; i < net_count; i++) {
            if (strcmp(nets[i].ssid, connected_ssid) == 0) {
                wifi_net_t tmp = nets[0];
                nets[0] = nets[i];
                nets[i] = tmp;
                break;
            }
        }
    }

    for (int i = 0; i < net_count; i++) {
        bool is_connected = (connected_ssid[0] &&
                             strcmp(nets[i].ssid, connected_ssid) == 0);
        char txt[80];
        lv_snprintf(txt, sizeof(txt), "%-24s%3d%%", nets[i].ssid, nets[i].signal);
        /* Connected → green tick icon; secured → charge icon; open → no icon */
        const char * icon = is_connected ? LV_SYMBOL_OK
                          : nets[i].secured ? LV_SYMBOL_CHARGE : NULL;
        lv_obj_t * btn = lv_list_add_button(list_widget, icon, txt);
        lv_obj_set_height(btn, 66);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x141414), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a2a1a), LV_STATE_PRESSED);
        lv_obj_set_style_text_color(btn, is_connected ? C_GREEN : C_WHITE, 0);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_16, 0);
        lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x2a2a2a), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_add_event_cb(btn, net_btn_cb, LV_EVENT_CLICKED, nets[i].ssid);
    }
}

/* ── poll timers ─────────────────────────── */
static void scan_poll_cb(lv_timer_t * t) {
    if (scan_st == ST_IDLE || scan_st == ST_RUNNING) return;
    lv_timer_delete(t);
    scan_poll_tmr = NULL;
    scan_st = ST_IDLE;
    populate_list();
}

static void go_home_cb(lv_timer_t * t) { LV_UNUSED(t); nav_to_home(); }

static void conn_poll_cb(lv_timer_t * t) {
    if (conn_st == ST_IDLE || conn_st == ST_RUNNING) return;
    lv_timer_delete(t);
    if (conn_st == ST_OK) {
        lv_label_set_text(pwd_status_lbl, LV_SYMBOL_OK "  Connected!");
        lv_obj_set_style_text_color(pwd_status_lbl, C_GREEN, 0);
        lv_timer_t * gt = lv_timer_create(go_home_cb, 1500, NULL);
        lv_timer_set_repeat_count(gt, 1);
    } else {
        char msg[280];
        if (conn_result_msg[0])
            lv_snprintf(msg, sizeof(msg), LV_SYMBOL_CLOSE "  %s", conn_result_msg);
        else
            lv_snprintf(msg, sizeof(msg), LV_SYMBOL_CLOSE "  Failed — check password");
        lv_label_set_text(pwd_status_lbl, msg);
        lv_obj_set_style_text_color(pwd_status_lbl, lv_color_hex(0xff4444), 0);
    }
    conn_st = ST_IDLE;
}

/* ── connect action ──────────────────────── */
static void do_connect(void) {
    if (conn_st == ST_RUNNING) return;
    conn_result_msg[0] = '\0';
    const char * pw = lv_textarea_get_text(pwd_ta);
    bool has_pw = (pw && pw[0] != '\0');

    /*
     * Write a helper script to /tmp.  Using a script lets us check whether
     * a saved profile already exists and take the correct path:
     *   - Saved profile: modify password (if any) then bring it up via
     *     `nmcli connection up` — avoids the "key-mgmt: property is missing"
     *     error that `device wifi connect` triggers when scan results are stale.
     *   - No saved profile: use `device wifi connect` with the password to
     *     create a new profile on the fly.
     */
    FILE * sh = fopen("/tmp/invo_connect.sh", "w");
    if (!sh) { conn_st = ST_FAIL; return; }

    if (has_pw) {
        fprintf(sh,
            "#!/bin/sh\n"
            "if nmcli connection show '%s' >/dev/null 2>&1; then\n"
            "  nmcli connection modify '%s' wifi-sec.psk '%s' 2>&1 &&"
            " nmcli connection up '%s' 2>&1\n"
            "else\n"
            "  nmcli device wifi connect '%s' password '%s' 2>&1\n"
            "fi\n",
            sel_ssid, sel_ssid, pw, sel_ssid, sel_ssid, pw);
    } else {
        fprintf(sh,
            "#!/bin/sh\n"
            "nmcli connection up '%s' 2>&1\n",
            sel_ssid);
    }
    fclose(sh);

    lv_snprintf(conn_cmd, sizeof(conn_cmd), "/bin/sh /tmp/invo_connect.sh");
    lv_label_set_text(pwd_status_lbl, LV_SYMBOL_REFRESH "  Connecting...");
    lv_obj_set_style_text_color(pwd_status_lbl, C_BLUE, 0);
    conn_st = ST_RUNNING;
    spawn_detached(conn_thread);
    lv_timer_create(conn_poll_cb, 500, NULL);
}

/* ── event callbacks ─────────────────────── */
static void connect_btn_cb(lv_event_t * e) { LV_UNUSED(e); do_connect(); }
static void back_to_home_cb(lv_event_t * e) { LV_UNUSED(e); nav_to_home(); }
static void back_to_list_cb(lv_event_t * e) { LV_UNUSED(e); invo_wifi_show_list(); }
static void refresh_btn_cb(lv_event_t * e)  { LV_UNUSED(e); invo_wifi_show_list(); }

static void eye_cb(lv_event_t * e) {
    LV_UNUSED(e);
    show_pwd = !show_pwd;
    lv_textarea_set_password_mode(pwd_ta, !show_pwd);
    lv_label_set_text(eye_lbl, show_pwd ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
}

/* ── list screen back button ─────────────── */
/* BOTTOM_MID -185,-80: center (215,690) → dist=344px < 400 ✓ */
static void make_back_btn(lv_obj_t * scr, lv_event_cb_t cb) {
    lv_obj_t * btn = lv_button_create(scr);
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

/* ── build WiFi list screen ──────────────── */
static void build_list_screen(void) {
    lv_obj_set_style_bg_color(list_scr, C_BG, 0);
    lv_obj_set_style_bg_opa(list_scr, LV_OPA_COVER, 0);

    lv_obj_t * title = lv_label_create(list_scr);
    lv_label_set_text(title, LV_SYMBOL_WIFI "  WiFi Networks");
    lv_obj_set_style_text_color(title, C_WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, -40, 58);

    lv_obj_t * ref = lv_button_create(list_scr);
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
    lv_obj_remove_flag(scan_spin, LV_OBJ_FLAG_CLICKABLE);

    scan_lbl = lv_label_create(list_scr);
    lv_label_set_text(scan_lbl, "Scanning...");
    lv_obj_set_style_text_color(scan_lbl, C_GRAY, 0);
    lv_obj_set_style_text_font(scan_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(scan_lbl, LV_ALIGN_CENTER, 0, 78);

    make_back_btn(list_scr, back_to_home_cb);
}

/* ── helper: style one keyboard row ─────── */
static void style_kb_row(lv_obj_t * bm) {
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
    lv_obj_set_style_text_font(bm, &lv_font_montserrat_18, LV_PART_ITEMS);
}

/* ── build password screen ───────────────── */
/*
 * Form area: y=82..361 (above keyboard)
 * Keyboard:  y=410..685 (rows filling the circular lower half)
 *
 * Textarea 350px wide at (-25, 164) + eye button right of it ✓
 * Back button CENTER,0,-58: center y=342, well above keyboard ✓
 */
static void build_pwd_screen(void) {
    lv_obj_set_style_bg_color(pwd_scr, C_BG, 0);
    lv_obj_set_style_bg_opa(pwd_scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(pwd_scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * hdr = lv_label_create(pwd_scr);
    lv_label_set_text(hdr, "Connect to WiFi");
    lv_obj_set_style_text_color(hdr, C_GRAY, 0);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_14, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 82);

    pwd_ssid_lbl = lv_label_create(pwd_scr);
    lv_label_set_text(pwd_ssid_lbl, "");
    lv_obj_set_style_text_color(pwd_ssid_lbl, C_WHITE, 0);
    lv_obj_set_style_text_font(pwd_ssid_lbl, &lv_font_montserrat_18, 0);
    lv_label_set_long_mode(pwd_ssid_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(pwd_ssid_lbl, 380);
    lv_obj_set_style_text_align(pwd_ssid_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(pwd_ssid_lbl, LV_ALIGN_TOP_MID, 0, 108);

    /* Password textarea (350px) + eye button to its right */
    pwd_ta = lv_textarea_create(pwd_scr);
    lv_obj_set_size(pwd_ta, 350, 52);
    lv_obj_align(pwd_ta, LV_ALIGN_TOP_MID, -25, 164);
    lv_textarea_set_password_mode(pwd_ta, true);
    lv_textarea_set_one_line(pwd_ta, true);
    lv_textarea_set_placeholder_text(pwd_ta, "Password");
    lv_obj_set_style_bg_color(pwd_ta, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_color(pwd_ta, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(pwd_ta, 1, 0);
    lv_obj_set_style_radius(pwd_ta, 10, 0);
    lv_obj_set_style_text_color(pwd_ta, C_WHITE, 0);

    lv_obj_t * eye_btn = lv_button_create(pwd_scr);
    lv_obj_set_size(eye_btn, 46, 52);
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

    lv_obj_t * conn = lv_button_create(pwd_scr);
    lv_obj_set_size(conn, 160, 46);
    lv_obj_align(conn, LV_ALIGN_TOP_MID, 0, 230);
    lv_obj_set_style_bg_color(conn, C_GREEN, 0);
    lv_obj_set_style_bg_color(conn, lv_color_hex(0x00c060), LV_STATE_PRESSED);
    lv_obj_set_style_radius(conn, 10, 0);
    lv_obj_add_event_cb(conn, connect_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * cl = lv_label_create(conn);
    lv_label_set_text(cl, "Connect");
    lv_obj_set_style_text_color(cl, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_16, 0);
    lv_obj_center(cl);

    pwd_status_lbl = lv_label_create(pwd_scr);
    lv_label_set_text(pwd_status_lbl, "");
    lv_obj_set_style_text_font(pwd_status_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(pwd_status_lbl, LV_ALIGN_TOP_MID, 0, 288);

    /* ← Networks back button in gap between form and keyboard */
    lv_obj_t * back_btn = lv_button_create(pwd_scr);
    lv_obj_set_size(back_btn, 130, 38);
    lv_obj_align(back_btn, LV_ALIGN_CENTER, 0, -58);
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

    /* ── Custom round keyboard: 4 rows, each sized to fit the circle ── */
    static const int ys[] = { KB_Y1, KB_Y2, KB_Y3, KB_Y4 };
    static const int ws[] = { KB_W1, KB_W2, KB_W3, KB_W4 };

    for (int i = 0; i < 4; i++) {
        kb_row[i] = lv_buttonmatrix_create(pwd_scr);
        lv_obj_set_size(kb_row[i], ws[i], KB_ROW_H);
        lv_obj_align(kb_row[i], LV_ALIGN_TOP_MID, 0, ys[i]);
        style_kb_row(kb_row[i]);
        lv_obj_add_event_cb(kb_row[i], kb_row_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    /* Set initial maps and widths */
    kb_update_maps();
}

/* ── public API ──────────────────────────── */
void invo_wifi_init(void) {
    list_scr = lv_obj_create(NULL);
    pwd_scr  = lv_obj_create(NULL);
    lv_obj_remove_flag(list_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(pwd_scr,  LV_OBJ_FLAG_SCROLLABLE);
    build_list_screen();
    build_pwd_screen();
}

void invo_wifi_show_list(void) {
    if (scan_st == ST_RUNNING) {
        lv_scr_load_anim(list_scr, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
        return;
    }
    lv_obj_clean(list_widget);
    lv_obj_remove_flag(scan_spin, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(scan_lbl,  LV_OBJ_FLAG_HIDDEN);
    lv_scr_load_anim(list_scr, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    scan_st = ST_RUNNING;
    if (scan_poll_tmr) {
        lv_timer_delete(scan_poll_tmr);
        scan_poll_tmr = NULL;
    }
    spawn_detached(scan_thread);
    scan_poll_tmr = lv_timer_create(scan_poll_cb, 500, NULL);
}
