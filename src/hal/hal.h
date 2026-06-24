#ifndef INVO_HAL_H
#define INVO_HAL_H

#ifdef ESP_PLATFORM
#  include "lvgl.h"        /* ESP-IDF managed component */
#else
#  include "lvgl/lvgl.h"   /* Pi submodule */
#endif

/* ── Shared inverter data structure ─────────────────────────────────────────
 * Both platforms populate this struct. Screens read only from this struct —
 * never from platform-specific sources directly.
 * ─────────────────────────────────────────────────────────────────────────── */
typedef struct {
    /* Battery */
    float batt_pct;        /* 0–100 % */
    float batt_v;
    float batt_a;
    float batt_chg_kw;
    float batt_temp;
    int   batt_backup_min; /* remaining backup in minutes */

    /* Solar */
    float solar_kw;
    float solar_v;
    float solar_a;

    /* Grid */
    float grid_v;
    float grid_hz;

    /* Output / Home load */
    float load_kw;
    float out_v;
    float out_hz;
    float out_a;

    /* Status flags */
    int fault;
    int bypassing;
    int inv_on;
    int ac_chg;
} invo_data_t;

/* ── Display & Touch ─────────────────────────────────────────────────────────
 * Called once from main / app_main during startup.
 * ─────────────────────────────────────────────────────────────────────────── */
void hal_display_init(void);
void hal_touch_init(void);

/* ── Backlight ───────────────────────────────────────────────────────────────
 * percent: 0–100
 * Pi:    writes to /sys/class/backlight/.../brightness
 * ESP32: LEDC PWM duty cycle
 * ─────────────────────────────────────────────────────────────────────────── */
void hal_brightness_set(int percent);
int  hal_brightness_get(void);

/* ── Live inverter data ──────────────────────────────────────────────────────
 * Thread-safe read of the latest inverter values.
 * Pi:    wraps invo_uart_get_live()
 * ESP32: wraps gd (global data struct) under mutex
 * ─────────────────────────────────────────────────────────────────────────── */
void hal_data_get(invo_data_t *out);

/* ── FOTA ────────────────────────────────────────────────────────────────────
 * Trigger a firmware update check.
 * Pi:    system("systemctl start invo-updater &")
 * ESP32: starts fota_check_and_update() task
 * ─────────────────────────────────────────────────────────────────────────── */
void hal_fota_trigger(void);

/* ── Key-value storage ───────────────────────────────────────────────────────
 * Persist small settings (brightness level, sleep timeout, etc.)
 * Pi:    reads/writes /etc/invo/settings.conf (key=value lines)
 * ESP32: NVS flash namespace "invo"
 * Returns 0 on success, -1 on error.
 * ─────────────────────────────────────────────────────────────────────────── */
int  hal_kv_set(const char *key, const char *val);
int  hal_kv_get(const char *key, char *out, int out_len);

#endif /* INVO_HAL_H */
