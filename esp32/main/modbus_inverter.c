/*
 * modbus_inverter.c — Megmeet inverter Modbus RTU via hardware UART + RS485 module
 *
 * Wiring (TTL side of module → ESP32 header):
 *   Module VCC  → 3.3V
 *   Module GND  → GND
 *   Module RXD  → GPIO 4  (MB_UART_TX)
 *   Module TXD  → GPIO 5  (MB_UART_RX)
 *   Module EN   → GPIO 22 (MB_UART_DE)  HIGH=transmit, LOW=receive
 *
 * RS485 side of module → Inverter:
 *   Module A → Inverter RX_485A
 *   Module B → Inverter TX_485B
 *   Module GND → Inverter TTL_DGND
 *   Inverter TTL_12V — do NOT connect
 *   Inverter PCS_EN  — leave unconnected
 */
#include "modbus_inverter.h"
#include "hw_config.h"
#include "ui_common.h"
#include "lvgl_port.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

#define TAG           "modbus"

#define MB_SLAVE      1
#define REG_B1_START  4017
#define REG_B1_COUNT  17      /* 4017-4033 */
#define REG_B2_START  4036
#define REG_B2_COUNT  8       /* 4036-4043 */
#define REG_OUT_CTRL  4049

#define BATT_RATED_V  48.0f
#define BATT_AH       100.0f
#define BATT_EMA      0.15f

static volatile bool s_valid      = false;
static volatile int  s_pending_cmd = -1;

bool modbus_inverter_valid(void)            { return s_valid; }
void modbus_inverter_request_output(int on) { s_pending_cmd = on; }

/* ── CRC16 ───────────────────────────────────────────────────────────── */

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

/* ── RS485 half-duplex helpers ───────────────────────────────────────── */

static void rs485_tx_mode(void)
{
    gpio_set_level(MB_UART_DE, 1);
}

static void rs485_rx_mode(void)
{
    /* Wait for TX FIFO to drain before releasing the bus */
    uart_wait_tx_done(MB_UART_NUM, pdMS_TO_TICKS(50));
    gpio_set_level(MB_UART_DE, 0);
}

/* ── Modbus transport ────────────────────────────────────────────────── */

static int mb_read_regs(uint16_t start, uint8_t count, uint16_t *out)
{
    uint8_t req[8];
    req[0] = MB_SLAVE; req[1] = 0x03;
    req[2] = start >> 8; req[3] = start & 0xFF;
    req[4] = 0;          req[5] = count;
    uint16_t c = crc16(req, 6);
    req[6] = c & 0xFF;   req[7] = c >> 8;

    uart_flush_input(MB_UART_NUM);
    rs485_tx_mode();
    uart_write_bytes(MB_UART_NUM, req, 8);
    rs485_rx_mode();

    int expected = 5 + count * 2;
    uint8_t resp[64];
    int got = uart_read_bytes(MB_UART_NUM, resp, expected, pdMS_TO_TICKS(300));
    if (got < expected) {
        ESP_LOGW(TAG, "RX timeout: got %d/%d (reg %u)", got, expected, start);
        return -1;
    }

    if (resp[1] & 0x80) { ESP_LOGW(TAG, "Modbus exc 0x%02X", resp[2]); return -2; }
    if (resp[2] != count * 2) return -3;
    uint16_t resp_crc = (uint16_t)resp[expected-2] | ((uint16_t)resp[expected-1] << 8);
    if (resp_crc != crc16(resp, expected - 2)) { ESP_LOGW(TAG, "CRC fail"); return -4; }

    for (int i = 0; i < count; i++)
        out[i] = ((uint16_t)resp[3 + i*2] << 8) | resp[4 + i*2];
    return count;
}

static void mb_write_reg(uint16_t addr, uint16_t value)
{
    uint8_t req[8];
    req[0] = MB_SLAVE; req[1] = 0x06;
    req[2] = addr >> 8; req[3] = addr & 0xFF;
    req[4] = value >> 8; req[5] = value & 0xFF;
    uint16_t c = crc16(req, 6);
    req[6] = c & 0xFF; req[7] = c >> 8;

    uart_flush_input(MB_UART_NUM);
    rs485_tx_mode();
    uart_write_bytes(MB_UART_NUM, req, 8);
    rs485_rx_mode();
    /* Read and discard the echo/response */
    uint8_t resp[8];
    uart_read_bytes(MB_UART_NUM, resp, 8, pdMS_TO_TICKS(200));
}

/* ── Main poll task ──────────────────────────────────────────────────── */

static void modbus_task(void *arg)
{
    float smooth_pct   = -1.0f;
    int   valid_streak = 0;

    ESP_LOGI(TAG, "Modbus RTU polling started (UART%d TX=%d RX=%d DE=%d)",
             MB_UART_NUM, MB_UART_TX, MB_UART_RX, MB_UART_DE);

    while (1) {
        /* Pending output command */
        int cmd = s_pending_cmd;
        if (cmd >= 0) {
            s_pending_cmd = -1;
            mb_write_reg(REG_OUT_CTRL, cmd ? 1 : 0);
            ESP_LOGI(TAG, "Output %s", cmd ? "ON" : "OFF");
            vTaskDelay(pdMS_TO_TICKS(4000));
        }

        /* ── Read two register blocks ─────────────────────────── */
        uint16_t r1[REG_B1_COUNT], r2[REG_B2_COUNT];
        int rc1 = mb_read_regs(REG_B1_START, REG_B1_COUNT, r1);
        vTaskDelay(pdMS_TO_TICKS(50));
        int rc2 = mb_read_regs(REG_B2_START, REG_B2_COUNT, r2);

        if (rc1 < 0 || rc2 < 0) {
            ESP_LOGW(TAG, "Poll failed (rc1=%d rc2=%d)", rc1, rc2);
            s_valid = false;
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        /* ── Decode ───────────────────────────────────────────── */
        float pv_v       = r1[0]  * 0.1f;
        float pv_a       = s16(r1[1])  * 0.1f;
        int   pv_w       = s16(r1[2]);
        float batt_v     = r1[7]  * 0.1f;
        float batt_a     = s16(r1[8])  * 0.1f;
        int   batt_w     = s16(r1[9]);
        float raw_grid_v = r1[11] * 0.1f;
        float grid_hz    = r1[15] * 0.01f;
        float inv_out_v  = r2[0]  * 0.1f;
        float out_a      = s16(r2[1])  * 0.1f;
        int   out_w      = s16(r2[2]);
        float out_hz     = r2[3]  * 0.01f;
        uint16_t op_st   = r2[4];
        float inv_t      = s16(r2[7])  * 0.1f;

        int is_bypassing = (op_st >> 10) & 1;
        int inv_on       = (op_st >>  8) & 1;
        int ac_chg       = (op_st >>  9) & 1;
        int fault        = (op_st >> 11) & 1;
        float out_v      = is_bypassing ? raw_grid_v : inv_out_v;

        if ((grid_hz > 0.0f && grid_hz < 44.0f) || grid_hz > 56.0f) {
            ESP_LOGW(TAG, "Bad grid_hz %.2f — discard", grid_hz);
            vTaskDelay(pdMS_TO_TICKS(3000)); continue;
        }
        if ((out_hz > 0.0f && out_hz < 44.0f) || out_hz > 56.0f) {
            ESP_LOGW(TAG, "Bad out_hz %.2f — discard", out_hz);
            vTaskDelay(pdMS_TO_TICKS(3000)); continue;
        }

        int batt_ok = (batt_v >= BATT_RATED_V * 0.60f &&
                       batt_v <= BATT_RATED_V * 1.20f &&
                       fabsf(batt_a) >= 0.5f);

        if (batt_ok) { valid_streak++; }
        else         { valid_streak = 0; smooth_pct = 0.0f; }

        if (batt_ok && valid_streak >= 3) {
            float raw_pct = (batt_v - 44.0f) / (57.6f - 44.0f) * 100.0f;
            if (raw_pct < 0.0f) raw_pct = 0.0f;
            if (raw_pct > 100.0f) raw_pct = 100.0f;
            if (smooth_pct < 0.0f || smooth_pct == 0.0f)
                smooth_pct = raw_pct;
            else if (fabsf(raw_pct - smooth_pct) < 25.0f)
                smooth_pct = BATT_EMA * raw_pct + (1.0f - BATT_EMA) * smooth_pct;
        }
        if (smooth_pct < 0.0f) smooth_pct = 0.0f;
        int pct = (int)smooth_pct;

        int backup_min = 0;
        if (out_w > 10 && pct > 0) {
            float remaining_wh = (pct / 100.0f) * BATT_AH * BATT_RATED_V;
            backup_min = (int)((remaining_wh / (float)out_w) * 60.0f);
            if (backup_min > 9999) backup_min = 9999;
        }

        float grid_v = raw_grid_v >= 80.0f ? raw_grid_v : 0.0f;

        /* ── Write to gd ────────────────────────────────────────── */
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
        gd.voltage   = (int)(batt_ok ? batt_v : 0.0f);
        gd.current   = batt_a;
        s_valid = true;
        lvgl_release();

        ESP_LOGI(TAG,
            "pv=%.1fV/%.1fA/%dW batt=%.1fV/%.1fA/%d%% "
            "out=%.1fV/%.2fHz/%dW grid=%.1fV/%.2fHz temp=%.1fC",
            pv_v, pv_a, pv_w, batt_v, batt_a, pct,
            out_v, out_hz, out_w, grid_v, grid_hz, inv_t);

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/* ── Init ────────────────────────────────────────────────────────────── */

void modbus_inverter_start(void)
{
    /* DE pin — output, start in RX mode */
    gpio_config_t de_cfg = {
        .pin_bit_mask = BIT64(MB_UART_DE),
        .mode         = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&de_cfg));
    gpio_set_level(MB_UART_DE, 0);

    /* UART driver */
    uart_config_t uart_cfg = {
        .baud_rate  = MB_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(MB_UART_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(MB_UART_NUM, MB_UART_TX, MB_UART_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(MB_UART_NUM, 256, 0, 0, NULL, 0));

    xTaskCreate(modbus_task, "modbus_inv", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "Modbus RTU ready (UART%d @ %d baud)", MB_UART_NUM, MB_BAUD);
}
