#include "invo_uart.h"
#include <stdio.h>
#define IS_C_GREEN  lv_color_hex(0x00e676)
#define IS_C_ORANGE lv_color_hex(0xffa500)
#define IS_C_RED    lv_color_hex(0xff3300)
#define IS_C_GRAY   lv_color_hex(0x666666)
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#define DATA_FILE "/tmp/invo_data"

/* ── Registered UI labels ────────────────────────────────── */
static lv_obj_t * lbl_solar_main;
static lv_obj_t * lbl_solar_v;
static lv_obj_t * lbl_solar_a;
static lv_obj_t * lbl_load_main;
static lv_obj_t * lbl_load_draw;
static lv_obj_t * lbl_load_peak;
static lv_obj_t * lbl_home_solar;
static lv_obj_t * lbl_home_load;
static lv_obj_t * lbl_batt_pct;
static lv_obj_t * lbl_batt_chg;
static lv_obj_t * lbl_batt_temp;
static lv_obj_t * lbl_batt_backup;
static lv_obj_t * lbl_grid_v;
static lv_obj_t * lbl_grid_hz;
static lv_obj_t * lbl_grid_w;
static lv_obj_t * lbl_batt_v;
static lv_obj_t * lbl_out_v;
static lv_obj_t * lbl_out_hz;
static lv_obj_t * home_batt_arc;
static lv_obj_t * home_batt_pct;
static lv_obj_t * home_batt_backup;
static lv_obj_t * lbl_solar_pw;
static lv_obj_t * lbl_solar_gv;
static lv_obj_t * lbl_load_ov;
static lv_obj_t * lbl_load_ohz;

/* ── Inverter screen labels ──────────────────────────────── */
static lv_obj_t * is_out_v, * is_out_hz, * is_out_w;
static lv_obj_t * is_grid_v, * is_grid_hz, * is_grid_w, * is_grid_a;
static lv_obj_t * is_batt_v, * is_batt_a, * is_batt_temp;
static lv_obj_t * is_inv_on, * is_bypass, * is_ac_chg, * is_fault;

/* ── Shared data (bg thread → UI timer) ─────────────────── */
typedef struct {
    float solar_kw, solar_v, solar_a;
    float load_kw, load_peak;
    float batt_pct, batt_chg_kw, batt_temp, batt_backup_min;
    float grid_v, grid_hz, grid_w, grid_a;
    float batt_v, batt_a;
    float out_v, out_hz, out_w, out_a;
    int   inv_on, bypassing, ac_chg, fault;
    int   dirty;
} uart_data_t;

static uart_data_t       ud;
static pthread_mutex_t   ud_lock = PTHREAD_MUTEX_INITIALIZER;

/* ── Registration ─────────────────────────────────────────── */
void invo_uart_register_solar(lv_obj_t *main_val, lv_obj_t *voltage, lv_obj_t *current) {
    lbl_solar_main = main_val;
    lbl_solar_v    = voltage;
    lbl_solar_a    = current;
}

void invo_uart_register_load(lv_obj_t *main_val, lv_obj_t *cur_draw, lv_obj_t *peak) {
    lbl_load_main = main_val;
    lbl_load_draw = cur_draw;
    lbl_load_peak = peak;
}

void invo_uart_register_home(lv_obj_t *solar_val, lv_obj_t *load_val) {
    lbl_home_solar = solar_val;
    lbl_home_load  = load_val;
}

void invo_uart_register_battery(lv_obj_t *soc_pct, lv_obj_t *chg_kw,
                                 lv_obj_t *temp, lv_obj_t *backup) {
    lbl_batt_pct    = soc_pct;
    lbl_batt_chg    = chg_kw;
    lbl_batt_temp   = temp;
    lbl_batt_backup = backup;
}

void invo_uart_register_grid(lv_obj_t *grid_v, lv_obj_t *batt_v,
                              lv_obj_t *grid_hz, lv_obj_t *grid_w,
                              lv_obj_t *out_v,  lv_obj_t *out_hz) {
    lbl_grid_v  = grid_v;
    lbl_batt_v  = batt_v;
    lbl_grid_hz = grid_hz;
    lbl_grid_w  = grid_w;
    lbl_out_v   = out_v;
    lbl_out_hz  = out_hz;
}

void invo_uart_register_solar_detail(lv_obj_t *pv_w, lv_obj_t *grid_v_lbl) {
    lbl_solar_pw = pv_w;
    lbl_solar_gv = grid_v_lbl;
}

void invo_uart_register_load_detail(lv_obj_t *out_v_lbl, lv_obj_t *out_hz_lbl) {
    lbl_load_ov  = out_v_lbl;
    lbl_load_ohz = out_hz_lbl;
}

void invo_uart_register_home_battery(lv_obj_t *arc, lv_obj_t *pct_lbl, lv_obj_t *backup_lbl) {
    home_batt_arc    = arc;
    home_batt_pct    = pct_lbl;
    home_batt_backup = backup_lbl;
}

void invo_uart_register_inverter_screen(
    lv_obj_t *out_v, lv_obj_t *out_hz, lv_obj_t *out_w,
    lv_obj_t *grid_v, lv_obj_t *grid_hz, lv_obj_t *grid_w, lv_obj_t *grid_a,
    lv_obj_t *batt_v, lv_obj_t *batt_a, lv_obj_t *batt_temp,
    lv_obj_t *inv_on, lv_obj_t *bypass, lv_obj_t *ac_chg, lv_obj_t *fault)
{
    is_out_v = out_v; is_out_hz = out_hz; is_out_w = out_w;
    is_grid_v = grid_v; is_grid_hz = grid_hz; is_grid_w = grid_w; is_grid_a = grid_a;
    is_batt_v = batt_v; is_batt_a = batt_a; is_batt_temp = batt_temp;
    is_inv_on = inv_on; is_bypass = bypass; is_ac_chg = ac_chg; is_fault = fault;
}

/* ── Background reader thread ────────────────────────────── */
static void * uart_thread(void * arg) {
    (void)arg;

    while (1) {
        FILE * f = fopen(DATA_FILE, "r");
        if (f) {
            char line[128];
            pthread_mutex_lock(&ud_lock);
            while (fgets(line, sizeof(line), f)) {
                line[strcspn(line, "\r\n")] = '\0';
                char * eq = strchr(line, '=');
                if (!eq) continue;
                *eq = '\0';
                float fval = strtof(eq + 1, NULL);
                if      (strcmp(line, "solar_kw")    == 0) { ud.solar_kw       = fval; ud.dirty = 1; }
                else if (strcmp(line, "solar_v")     == 0) { ud.solar_v        = fval; ud.dirty = 1; }
                else if (strcmp(line, "solar_a")     == 0) { ud.solar_a        = fval; ud.dirty = 1; }
                else if (strcmp(line, "load_kw")     == 0) { ud.load_kw        = fval; ud.dirty = 1; }
                else if (strcmp(line, "load_peak")   == 0) { ud.load_peak      = fval; ud.dirty = 1; }
                else if (strcmp(line, "batt_pct")    == 0) { ud.batt_pct       = fval; ud.dirty = 1; }
                else if (strcmp(line, "batt_chg_kw") == 0) { ud.batt_chg_kw   = fval; ud.dirty = 1; }
                else if (strcmp(line, "batt_temp")   == 0) { ud.batt_temp      = fval; ud.dirty = 1; }
                else if (strcmp(line, "batt_backup") == 0) { ud.batt_backup_min= fval; ud.dirty = 1; }
                else if (strcmp(line, "grid_v")   == 0) { ud.grid_v   = fval; ud.dirty = 1; }
                else if (strcmp(line, "grid_hz")  == 0) { ud.grid_hz  = fval; ud.dirty = 1; }
                else if (strcmp(line, "grid_w")   == 0) { ud.grid_w   = fval; ud.dirty = 1; }
                else if (strcmp(line, "batt_v")   == 0) { ud.batt_v   = fval; ud.dirty = 1; }
                else if (strcmp(line, "out_v")     == 0) { ud.out_v    = fval; ud.dirty = 1; }
                else if (strcmp(line, "out_hz")    == 0) { ud.out_hz   = fval; ud.dirty = 1; }
                else if (strcmp(line, "out_w")     == 0) { ud.out_w    = fval; ud.dirty = 1; }
                else if (strcmp(line, "out_a")     == 0) { ud.out_a    = fval; ud.dirty = 1; }
                else if (strcmp(line, "grid_a")    == 0) { ud.grid_a   = fval; ud.dirty = 1; }
                else if (strcmp(line, "batt_a")    == 0) { ud.batt_a   = fval; ud.dirty = 1; }
                else if (strcmp(line, "inv_on")    == 0) { ud.inv_on   = (int)fval; ud.dirty = 1; }
                else if (strcmp(line, "bypassing") == 0) { ud.bypassing= (int)fval; ud.dirty = 1; }
                else if (strcmp(line, "ac_chg")    == 0) { ud.ac_chg   = (int)fval; ud.dirty = 1; }
                else if (strcmp(line, "fault")     == 0) { ud.fault    = (int)fval; ud.dirty = 1; }
            }
            pthread_mutex_unlock(&ud_lock);
            fclose(f);
        }
        usleep(3000000); /* poll every 3 seconds */
    }

    return NULL;
}

static void format_backup(char * buf, size_t n, float mins) {
    int t = (int)mins;
    if (t >= 60) lv_snprintf(buf, n, "%dh %dm", t / 60, t % 60);
    else         lv_snprintf(buf, n, "%dm", t);
}

/* ── LVGL poll timer (main thread) ───────────────────────── */
static void uart_poll_cb(lv_timer_t * t) {
    LV_UNUSED(t);

    pthread_mutex_lock(&ud_lock);
    if (!ud.dirty) { pthread_mutex_unlock(&ud_lock); return; }
    uart_data_t d = ud;
    ud.dirty = 0;
    pthread_mutex_unlock(&ud_lock);

    char buf[32];

    if (lbl_solar_main) {
        lv_snprintf(buf, sizeof(buf), "%.1f kW", d.solar_kw);
        lv_label_set_text(lbl_solar_main, buf);
    }
    if (lbl_solar_v) {
        lv_snprintf(buf, sizeof(buf), "%.0f V", d.solar_v);
        lv_label_set_text(lbl_solar_v, buf);
    }
    if (lbl_solar_a) {
        lv_snprintf(buf, sizeof(buf), "%.1f A", d.solar_a);
        lv_label_set_text(lbl_solar_a, buf);
    }
    if (lbl_load_main) {
        lv_snprintf(buf, sizeof(buf), "%.1f kW", d.load_kw);
        lv_label_set_text(lbl_load_main, buf);
    }
    if (lbl_load_draw) {
        lv_snprintf(buf, sizeof(buf), "%.0f W", d.out_w);
        lv_label_set_text(lbl_load_draw, buf);
    }
    if (lbl_load_peak) {
        lv_snprintf(buf, sizeof(buf), "%.2f A", d.out_a);
        lv_label_set_text(lbl_load_peak, buf);
    }
    if (lbl_home_solar) {
        lv_snprintf(buf, sizeof(buf), "%.1f kW", d.solar_kw);
        lv_label_set_text(lbl_home_solar, buf);
    }
    if (lbl_home_load) {
        lv_snprintf(buf, sizeof(buf), "%.1f kW", d.load_kw);
        lv_label_set_text(lbl_home_load, buf);
    }
    if (lbl_batt_pct) {
        lv_snprintf(buf, sizeof(buf), "%.0f%%", d.batt_pct);
        lv_label_set_text(lbl_batt_pct, buf);
    }
    if (lbl_batt_chg) {
        lv_snprintf(buf, sizeof(buf), "%.1f kW", d.batt_chg_kw);
        lv_label_set_text(lbl_batt_chg, buf);
    }
    if (lbl_batt_temp) {
        lv_snprintf(buf, sizeof(buf), "%.0f\xc2\xb0""C", d.batt_temp);
        lv_label_set_text(lbl_batt_temp, buf);
    }
    if (lbl_batt_backup) {
        format_backup(buf, sizeof(buf), d.batt_backup_min);
        lv_label_set_text(lbl_batt_backup, buf);
    }
    if (lbl_grid_v) {
        lv_snprintf(buf, sizeof(buf), "%.0f V", d.grid_v);
        lv_label_set_text(lbl_grid_v, buf);
    }
    if (lbl_grid_hz) {
        lv_snprintf(buf, sizeof(buf), "%.2f Hz", d.grid_hz);
        lv_label_set_text(lbl_grid_hz, buf);
    }
    if (lbl_grid_w) {
        lv_snprintf(buf, sizeof(buf), "%.0f W", d.grid_w);
        lv_label_set_text(lbl_grid_w, buf);
    }
    if (lbl_batt_v) {
        lv_snprintf(buf, sizeof(buf), "%.1f V", d.batt_v);
        lv_label_set_text(lbl_batt_v, buf);
    }
    if (lbl_out_v) {
        lv_snprintf(buf, sizeof(buf), "%.0f V", d.out_v);
        lv_label_set_text(lbl_out_v, buf);
    }
    if (lbl_out_hz) {
        lv_snprintf(buf, sizeof(buf), "%.2f Hz", d.out_hz);
        lv_label_set_text(lbl_out_hz, buf);
    }
    if (lbl_solar_pw) {
        lv_snprintf(buf, sizeof(buf), "%.2f Hz", d.grid_hz);
        lv_label_set_text(lbl_solar_pw, buf);
    }
    if (lbl_solar_gv) {
        lv_snprintf(buf, sizeof(buf), "%.0f V", d.grid_v);
        lv_label_set_text(lbl_solar_gv, buf);
    }
    if (lbl_load_ov) {
        lv_snprintf(buf, sizeof(buf), "%.0f V", d.out_v);
        lv_label_set_text(lbl_load_ov, buf);
    }
    if (lbl_load_ohz) {
        lv_snprintf(buf, sizeof(buf), "%.2f Hz", d.out_hz);
        lv_label_set_text(lbl_load_ohz, buf);
    }

    /* ── Inverter screen ─────────────────────────────────── */
    if (is_out_v)   { lv_snprintf(buf, sizeof(buf), "%.1f V",  d.out_v);  lv_label_set_text(is_out_v,  buf); }
    if (is_out_hz)  { lv_snprintf(buf, sizeof(buf), "%.2f Hz", d.out_hz); lv_label_set_text(is_out_hz, buf); }
    if (is_out_w)   { lv_snprintf(buf, sizeof(buf), "%.0f W",  d.out_w);  lv_label_set_text(is_out_w,  buf); }
    if (is_grid_v)  { lv_snprintf(buf, sizeof(buf), "%.0f V",  d.grid_v); lv_label_set_text(is_grid_v, buf); }
    if (is_grid_hz) { lv_snprintf(buf, sizeof(buf), "%.2f Hz", d.grid_hz);lv_label_set_text(is_grid_hz,buf); }
    if (is_grid_w)  { lv_snprintf(buf, sizeof(buf), "%.0f W",  d.grid_w); lv_label_set_text(is_grid_w, buf); }
    if (is_grid_a)  { lv_snprintf(buf, sizeof(buf), "%.2f A",  d.grid_a); lv_label_set_text(is_grid_a, buf); }
    if (is_batt_v)  { lv_snprintf(buf, sizeof(buf), "%.1f V",  d.batt_v); lv_label_set_text(is_batt_v, buf); }
    if (is_batt_a)  { lv_snprintf(buf, sizeof(buf), "%.2f A",  d.batt_a); lv_label_set_text(is_batt_a, buf); }
    if (is_batt_temp) {
        lv_snprintf(buf, sizeof(buf), "%.0f\xc2\xb0""C", d.batt_temp);
        lv_label_set_text(is_batt_temp, buf);
    }
    if (is_inv_on) {
        lv_label_set_text(is_inv_on, d.inv_on ? "ON" : "OFF");
        lv_obj_set_style_text_color(is_inv_on, d.inv_on ? IS_C_GREEN : IS_C_GRAY, 0);
    }
    if (is_bypass) {
        lv_label_set_text(is_bypass, d.bypassing ? "ACTIVE" : "INACTIVE");
        lv_obj_set_style_text_color(is_bypass, d.bypassing ? IS_C_ORANGE : IS_C_GRAY, 0);
    }
    if (is_ac_chg) {
        lv_label_set_text(is_ac_chg, d.ac_chg ? "ON" : "OFF");
        lv_obj_set_style_text_color(is_ac_chg, d.ac_chg ? IS_C_GREEN : IS_C_GRAY, 0);
    }
    if (is_fault) {
        lv_label_set_text(is_fault, d.fault ? "FAULT" : "OK");
        lv_obj_set_style_text_color(is_fault, d.fault ? IS_C_RED : IS_C_GREEN, 0);
    }

    /* Home screen battery arc + pct + backup */
    if (home_batt_arc)
        lv_arc_set_value(home_batt_arc, (int)d.batt_pct);
    if (home_batt_pct) {
        lv_snprintf(buf, sizeof(buf), "%.0f%%", d.batt_pct);
        lv_label_set_text(home_batt_pct, buf);
    }
    if (home_batt_backup) {
        format_backup(buf, sizeof(buf), d.batt_backup_min);
        lv_label_set_text(home_batt_backup, buf);
    }
}

/* ── Init ─────────────────────────────────────────────────── */
void invo_uart_init(void) {
    memset(&ud, 0, sizeof(ud));

    pthread_t th;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&th, &attr, uart_thread, NULL);
    pthread_attr_destroy(&attr);

    lv_timer_create(uart_poll_cb, 200, NULL);
}

void invo_uart_get_live(float *batt_pct, float *solar_kw, float *load_kw,
                         int *fault, int *bypassing, int *inv_on, float *batt_temp) {
    pthread_mutex_lock(&ud_lock);
    if (batt_pct)  *batt_pct  = ud.batt_pct;
    if (solar_kw)  *solar_kw  = ud.solar_kw;
    if (load_kw)   *load_kw   = ud.load_kw;
    if (fault)     *fault     = ud.fault;
    if (bypassing) *bypassing = ud.bypassing;
    if (inv_on)    *inv_on    = ud.inv_on;
    if (batt_temp) *batt_temp = ud.batt_temp;
    pthread_mutex_unlock(&ud_lock);
}

