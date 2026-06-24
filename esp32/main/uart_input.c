#include "uart_input.h"
#include "driver/uart.h"
#include "ui_common.h"
#include "lvgl_port.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "esp_log.h"

#define UART_PORT   UART_NUM_0
#define UART_TX     37
#define UART_RX     38
#define UART_BAUD   115200
#define LINE_MAX    48

static const char *TAG = "uart_in";

/* validity flags — false until first valid UART value received */
static volatile bool s_batt_valid    = false;
static volatile bool s_solar_valid   = false;
static volatile bool s_load_valid    = false;
static volatile bool s_voltage_valid = false;
static volatile bool s_current_valid = false;

bool uart_batt_valid(void)    { return s_batt_valid; }
bool uart_solar_valid(void)   { return s_solar_valid; }
bool uart_load_valid(void)    { return s_load_valid; }
bool uart_voltage_valid(void) { return s_voltage_valid; }
bool uart_current_valid(void) { return s_current_valid; }

/* ── battery UI update (called with lvgl lock held) ─────────────── */

static void update_batt_ui(void)
{
    lv_color_t arc_col = gd.batt_pct >= 50 ? C_GREEN :
                         gd.batt_pct >= 20 ? C_AMBER : C_RED;
    if (app.w_batt_pct)
        s_batt_valid ? lv_label_set_text_fmt(app.w_batt_pct, "%d%%", gd.batt_pct)
                     : lv_label_set_text(app.w_batt_pct, "--");
    if (app.w_batt_arc) {
        lv_arc_set_value(app.w_batt_arc, s_batt_valid ? gd.batt_pct : 0);
        lv_obj_set_style_arc_color(app.w_batt_arc,
                                   s_batt_valid ? arc_col : C_GRAY, LV_PART_INDICATOR);
    }
    if (app.w_bd_pct)
        s_batt_valid ? lv_label_set_text_fmt(app.w_bd_pct, "%d%%", gd.batt_pct)
                     : lv_label_set_text(app.w_bd_pct, "--");
}

/* ── parse helpers ────────────────────────────────────────────────── */

/* Returns true if str is a valid integer or decimal like "2.4", "-5", "380" */
static bool parse_float(const char *s, float *out)
{
    char *end;
    *out = strtof(s, &end);
    return (end != s && *end == '\0');
}

static bool parse_int(const char *s, int *out)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return false;
    *out = (int)v;
    return true;
}

/* ── UART reader task ────────────────────────────────────────────── */

static void uart_task(void *arg)
{
    char line[LINE_MAX];
    int  pos = 0;

    ESP_LOGI(TAG, "Protocol: '75'=battery%% | 'S:2.4'=solar kW | "
                  "'L:1.6'=load kW | 'V:380'=voltage | 'I:6.3'=current");

    while (1) {
        uint8_t ch;
        if (uart_read_bytes(UART_PORT, &ch, 1, portMAX_DELAY) <= 0) continue;

        if (ch == '\r' || ch == '\n') {
            if (pos == 0) continue;
            line[pos] = '\0';
            pos = 0;

            /* trim whitespace */
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            char *end = p + strlen(p) - 1;
            while (end > p && (*end == ' ' || *end == '\t')) *end-- = '\0';

            /* ── tagged fields: S:, L:, V:, I: ─────────────── */
            if ((p[1] == ':' || p[1] == '=') && p[2] != '\0') {
                char tag = toupper((unsigned char)p[0]);
                char *val_str = p + 2;
                float fval; int ival;

                lvgl_acquire();
                if (tag == 'S' && parse_float(val_str, &fval) && fval >= 0) {
                    gd.solar_kw = fval < 20.0f ? fval : 20.0f;
                    s_solar_valid = true;
                    lv_lbl_setf(app.w_solar_val, "%.1f kw", gd.solar_kw);
                    lv_lbl_setf(app.w_sd_kw,     "%.1f kw", gd.solar_kw);
                    ESP_LOGI(TAG, "Solar → %.2f kW", gd.solar_kw);

                } else if (tag == 'L' && parse_float(val_str, &fval) && fval >= 0) {
                    gd.load_kw = fval < 20.0f ? fval : 20.0f;
                    s_load_valid = true;
                    lv_lbl_setf(app.w_load_val, "%.1f kw", gd.load_kw);
                    lv_lbl_setf(app.w_ld_kw,    "%.1f kw", gd.load_kw);
                    ESP_LOGI(TAG, "Load → %.2f kW", gd.load_kw);

                } else if (tag == 'V' && parse_int(val_str, &ival) && ival > 0) {
                    gd.voltage = ival < 999 ? ival : 999;
                    s_voltage_valid = true;
                    if (app.w_sd_volt)
                        lv_label_set_text_fmt(app.w_sd_volt, "%d V", gd.voltage);
                    ESP_LOGI(TAG, "Voltage → %d V", gd.voltage);

                } else if (tag == 'I' && parse_float(val_str, &fval) && fval >= 0) {
                    gd.current = fval < 999.0f ? fval : 999.0f;
                    s_current_valid = true;
                    lv_lbl_setf(app.w_sd_cur, "%.1f A", gd.current);
                    ESP_LOGI(TAG, "Current → %.2f A", gd.current);

                } else {
                    ESP_LOGW(TAG, "Unknown tag or bad value: '%s'", p);
                }
                lvgl_release();
                continue;
            }

            /* ── bare number → battery % ─────────────────────── */
            char *q = p;
            if (*q == '-' || *q == '+') q++;
            bool digit_found = false, ok = true;
            for (char *c = q; *c; c++) {
                if (isdigit((unsigned char)*c)) digit_found = true;
                else { ok = false; break; }
            }
            ok = ok && digit_found;

            if (ok) {
                int val = atoi(p);
                if (val < 0)   val = 0;
                if (val > 100) val = 100;
                gd.batt_pct = val;
                s_batt_valid = true;
                ESP_LOGI(TAG, "Battery → %d%%", val);
            } else {
                s_batt_valid = false;
                ESP_LOGW(TAG, "Invalid: '%s' → battery showing --", p);
            }

            lvgl_acquire();
            update_batt_ui();
            lvgl_release();

        } else if (pos < LINE_MAX - 1) {
            line[pos++] = (char)ch;
        }
    }
}

/* ── public init ─────────────────────────────────────────────────── */

void uart_input_start(void)
{
    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX, UART_RX,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 256, 0, 0, NULL, 0));

    xTaskCreate(uart_task, "uart_in", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "UART0 ready on TX=%d RX=%d @ %d baud", UART_TX, UART_RX, UART_BAUD);
}
