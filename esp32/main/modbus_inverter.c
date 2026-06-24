/*
 * modbus_inverter.c — Megmeet inverter RS485 Modbus RTU reader for ESP32
 *
 * Wiring (MAX3485 8-pin DIP):
 *   Pin 1 RO  ── ESP32 GPIO 15  (UART1 RX)
 *   Pin 2 RE ─┐
 *   Pin 3 DE ─┘── ESP32 GPIO 16  (direction control, HIGH=TX LOW=RX)
 *   Pin 4 DI  ── ESP32 GPIO 17  (UART1 TX)
 *   Pin 5 GND ── GND
 *   Pin 6 A   ── Inverter A+
 *   Pin 7 B   ── Inverter B-
 *   Pin 8 VCC ── 3.3V
 */
#include "modbus_inverter.h"
#include "ui_common.h"
#include "lvgl_port.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

#define MB_UART     UART_NUM_1
#define MB_TX_PIN   17          /* → MAX3485 DI */
#define MB_RX_PIN   15          /* ← MAX3485 RO */
#define MB_DE_PIN   16          /* → MAX3485 DE+RE tied (HIGH=TX, LOW=RX) */
#define MB_BAUD     9600

#define MB_SLAVE    1

/* Megmeet register map — matches invo_bridge.py exactly */
#define REG_B1_START  4017
#define REG_B1_COUNT  17        /* 4017-4033 */
#define REG_B2_START  4036
#define REG_B2_COUNT  8         /* 4036-4043 */
#define REG_OUT_CTRL  4049

/* 48V battery system — adjust to match your bank */
#define BATT_RATED_V  48.0f
#define BATT_AH       100.0f
#define BATT_EMA      0.15f

static const char *TAG = "modbus";
static volatile bool s_valid       = false;
static volatile int  s_pending_cmd = -1;   /* -1=none  0=OFF  1=ON */

bool modbus_inverter_valid(void)          { return s_valid; }
void modbus_inverter_request_output(int on) { s_pending_cmd = on; }

/* ── CRC16 (Modbus polynomial) ─────────────────────────────────────── */

static uint16_t crc16(const uint8_t *data, int len)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

static inline int16_t s16(uint16_t v) { return (int16_t)v; }

/* ── FC 0x03 — read holding registers ──────────────────────────────── */

static int mb_read_regs(uint16_t start, uint8_t count, uint16_t *out)
{
    uint8_t req[8];
    req[0] = MB_SLAVE;
    req[1] = 0x03;
    req[2] = start >> 8;
    req[3] = start & 0xFF;
    req[4] = 0;
    req[5] = count;
    uint16_t crc = crc16(req, 6);
    req[6] = crc & 0xFF;        /* CRC low byte first — Modbus spec */
    req[7] = crc >> 8;

    uart_flush_input(MB_UART);

    gpio_set_level(MB_DE_PIN, 1);
    uart_write_bytes(MB_UART, req, 8);
    uart_wait_tx_done(MB_UART, pdMS_TO_TICKS(50));
    gpio_set_level(MB_DE_PIN, 0);

    int expected = 5 + count * 2;
    uint8_t resp[64];
    int n = uart_read_bytes(MB_UART, resp, expected, pdMS_TO_TICKS(300));

    if (n < expected) {
        ESP_LOGW(TAG, "Short resp %d/%d (reg %u)", n, expected, start);
        return -1;
    }
    if (resp[1] & 0x80) { ESP_LOGW(TAG, "Modbus exc 0x%02X", resp[2]); return -2; }
    if (resp[2] != count * 2) { ESP_LOGW(TAG, "Byte count mismatch");   return -3; }

    uint16_t resp_crc = (uint16_t)resp[n-2] | ((uint16_t)resp[n-1] << 8);
    if (resp_crc != crc16(resp, n - 2)) { ESP_LOGW(TAG, "CRC fail"); return -4; }

    for (int i = 0; i < count; i++)
        out[i] = ((uint16_t)resp[3 + i*2] << 8) | resp[4 + i*2];
    return count;
}

/* ── FC 0x06 — write single register ───────────────────────────────── */

static void mb_write_reg(uint16_t addr, uint16_t value)
{
    uint8_t req[8];
    req[0] = MB_SLAVE;
    req[1] = 0x06;
    req[2] = addr >> 8;
    req[3] = addr & 0xFF;
    req[4] = value >> 8;
    req[5] = value & 0xFF;
    uint16_t crc = crc16(req, 6);
    req[6] = crc & 0xFF;
    req[7] = crc >> 8;

    uart_flush_input(MB_UART);
    gpio_set_level(MB_DE_PIN, 1);
    uart_write_bytes(MB_UART, req, 8);
    uart_wait_tx_done(MB_UART, pdMS_TO_TICKS(100));
    gpio_set_level(MB_DE_PIN, 0);
}

/* ── Main poll task (runs every 3 s) ───────────────────────────────── */

static void modbus_task(void *arg)
{
    float smooth_pct  = -1.0f;  /* -1 = uninitialised */
    int   valid_streak = 0;

    while (1) {
        /* ── Check for pending output command ─────────────────── */
        int cmd = s_pending_cmd;
        if (cmd >= 0) {
            s_pending_cmd = -1;
            mb_write_reg(REG_OUT_CTRL, cmd ? 1 : 0);
            ESP_LOGI(TAG, "Output %s", cmd ? "ON" : "OFF");
            vTaskDelay(pdMS_TO_TICKS(4000));  /* inverter settling time */
        }

        /* ── Read two register blocks ─────────────────────────── */
        uint16_t r1[REG_B1_COUNT], r2[REG_B2_COUNT];
        int rc1 = mb_read_regs(REG_B1_START, REG_B1_COUNT, r1);
        vTaskDelay(pdMS_TO_TICKS(50));
        int rc2 = mb_read_regs(REG_B2_START, REG_B2_COUNT, r2);

        if (rc1 < 0 || rc2 < 0) {
            ESP_LOGW(TAG, "Poll failed (rc1=%d rc2=%d) — retry in 3 s", rc1, rc2);
            s_valid = false;
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        /* ── Decode — identical math to invo_bridge.py ─────────── */
        float pv_v       = r1[0]  * 0.1f;          /* 4017 PV voltage       */
        float pv_a       = s16(r1[1])  * 0.1f;     /* 4018 PV current       */
        int   pv_w       = s16(r1[2]);              /* 4019 PV power W       */
        float batt_v     = r1[7]  * 0.1f;          /* 4024 battery V        */
        float batt_a     = s16(r1[8])  * 0.1f;     /* 4025 battery A        */
        int   batt_w     = s16(r1[9]);              /* 4026 battery power W  */
        float raw_grid_v = r1[11] * 0.1f;          /* 4028 grid voltage     */
        float grid_hz    = r1[15] * 0.01f;         /* 4032 grid Hz          */
        float inv_out_v  = r2[0]  * 0.1f;          /* 4036 inv output V     */
        float out_a      = s16(r2[1])  * 0.1f;     /* 4037 output A         */
        int   out_w      = s16(r2[2]);              /* 4038 output W         */
        float out_hz     = r2[3]  * 0.01f;         /* 4039 output Hz        */
        uint16_t op_st   = r2[4];                   /* 4040 operating status */
        float inv_t      = s16(r2[7])  * 0.1f;     /* 4043 inverter temp °C */

        int is_bypassing = (op_st >> 10) & 1;
        int inv_on       = (op_st >>  8) & 1;
        int ac_chg       = (op_st >>  9) & 1;
        int fault        = (op_st >> 11) & 1;
        float out_v = is_bypassing ? raw_grid_v : inv_out_v;

        /* Sanity: discard frames with impossible AC frequency */
        if ((grid_hz > 0.0f && grid_hz < 44.0f) || grid_hz > 56.0f) {
            ESP_LOGW(TAG, "Bad grid_hz %.2f — discard", grid_hz);
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }
        if ((out_hz > 0.0f && out_hz < 44.0f) || out_hz > 56.0f) {
            ESP_LOGW(TAG, "Bad out_hz %.2f — discard", out_hz);
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        /* Battery validity: needs plausible voltage AND non-zero current */
        float batt_lo = BATT_RATED_V * 0.60f;
        float batt_hi = BATT_RATED_V * 1.20f;
        int batt_ok = (batt_v >= batt_lo && batt_v <= batt_hi &&
                       fabsf(batt_a) >= 0.5f);

        if (batt_ok) {
            valid_streak++;
        } else {
            valid_streak = 0;
            smooth_pct = 0.0f;
        }

        /* EMA SOC — only commit after 3 consecutive valid readings */
        if (batt_ok && valid_streak >= 3) {
            float raw_pct = (batt_v - 44.0f) / (57.6f - 44.0f) * 100.0f;
            if (raw_pct < 0.0f)   raw_pct = 0.0f;
            if (raw_pct > 100.0f) raw_pct = 100.0f;

            if (smooth_pct < 0.0f || smooth_pct == 0.0f)
                smooth_pct = raw_pct;
            else if (fabsf(raw_pct - smooth_pct) < 25.0f)
                smooth_pct = BATT_EMA * raw_pct + (1.0f - BATT_EMA) * smooth_pct;
        }
        if (smooth_pct < 0.0f) smooth_pct = 0.0f;
        int pct = (int)smooth_pct;

        /* Backup time */
        int backup_min = 0;
        if (out_w > 10 && pct > 0) {
            float remaining_wh = (pct / 100.0f) * BATT_AH * BATT_RATED_V;
            backup_min = (int)((remaining_wh / (float)out_w) * 60.0f);
            if (backup_min > 9999) backup_min = 9999;
        }

        /* Grid voltage — only trust when >80V (rules out floating ADC) */
        float grid_v = raw_grid_v >= 80.0f ? raw_grid_v : 0.0f;

        /* ── Write to gd (LVGL mutex held) ─────────────────────── */
        lvgl_acquire();
        gd.solar_kw  = pv_w > 0  ? (float)pv_w / 1000.0f : 0.0f;
        gd.pv_v      = pv_v  > 0.0f ? pv_v  : 0.0f;
        gd.pv_a      = pv_a  > 0.0f ? pv_a  : 0.0f;
        gd.load_kw   = out_w > 0  ? (float)out_w / 1000.0f : 0.0f;
        gd.batt_pct  = pct;
        gd.batt_v    = batt_ok ? batt_v : 0.0f;
        gd.batt_a    = batt_a;
        gd.chg_kw    = (float)batt_w / 1000.0f;
        gd.batt_temp = (-10.0f <= inv_t && inv_t <= 120.0f) ? inv_t : 0.0f;
        gd.backup_h  = backup_min / 60;
        gd.backup_m  = backup_min % 60;
        gd.grid_v    = grid_v;
        gd.grid_hz   = grid_hz;
        gd.out_v     = out_v;
        gd.out_hz    = out_hz;
        gd.out_a     = out_a;
        gd.inv_on    = inv_on;
        gd.ac_chg    = ac_chg;
        gd.bypassing = is_bypassing;
        gd.fault     = fault;
        gd.voltage   = (int)(batt_ok ? batt_v : 0.0f);  /* legacy compat */
        gd.current   = batt_a;                           /* legacy compat */
        s_valid = true;
        lvgl_release();

        ESP_LOGI(TAG,
            "pv=%.1fV/%.1fA/%dW batt=%.1fV/%.1fA/%d%% "
            "out=%.1fV/%.2fHz/%dW grid=%.1fV/%.2fHz temp=%.1fC op=0x%04X",
            pv_v, pv_a, pv_w, batt_v, batt_a, pct,
            out_v, out_hz, out_w, grid_v, grid_hz, inv_t, op_st);

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/* ── Init ───────────────────────────────────────────────────────────── */

void modbus_inverter_start(void)
{
    /* DE/RE direction pin — default LOW (receive mode) */
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << MB_DE_PIN,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(MB_DE_PIN, 0);

    uart_config_t cfg = {
        .baud_rate = MB_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(MB_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(MB_UART, MB_TX_PIN, MB_RX_PIN,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(MB_UART, 512, 0, 0, NULL, 0));

    xTaskCreate(modbus_task, "modbus_inv", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "Modbus RTU ready — UART1 TX=%d RX=%d DE=%d @ %d baud",
             MB_TX_PIN, MB_RX_PIN, MB_DE_PIN, MB_BAUD);
}
