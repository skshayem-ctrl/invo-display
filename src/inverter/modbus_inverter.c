/*
 * modbus_inverter.c — Megmeet inverter Modbus RTU via hardware UART + RS485 module
 *
 * ESP32 → RS485 module (TTL side):
 *   GPIO37 (MB_UART_TX) → Module RXD   (J8 TXD pin)
 *   GPIO38 (MB_UART_RX) ← Module TXD   (J8 RXD pin)
 *   GPIO22 (MB_UART_DE) → Module EN   HIGH=transmit, LOW=receive
 *   5V                  → Module VCC
 *   GND                 → Module GND
 *
 * RS485 module → Inverter CN10:
 *   Module A → Pin 3 (RX_485A)
 *   Module B → Pin 2 (TX_485B)
 *   GND      → Pin 1 (TTL_DGND)  ← isolated RS485 ground, must be connected
 *   3.3V     → Pin 4 (PCS_EN)    ← enables inverter RS485 output driver
 *   Pin 5 (TTL_12V) — do NOT connect
 *
 * If no response: swap A↔B at the inverter end and retry.
 */
#include "modbus_inverter.h"
#include "rs485_bus.h"
#include "hw_config.h"
#include "ui_common.h"
#include "lvgl_port.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

#define TAG "modbus"

#define MB_SLAVE 1
#define REG_B1_START 4017
#define REG_B1_COUNT 17 /* 4017-4033 */
#define REG_B2_START 4036
#define REG_B2_COUNT 8 /* 4036-4043 */
#define REG_OUT_CTRL 4049
#define REG_CHG_CTRL 4054
#define REG_CHG_VOLT 4056 /* Battery charge voltage setpoint ×0.1V */
#define REG_B3_START 4049
#define REG_B3_COUNT 8 /* 4049–4056 */

static volatile bool s_valid = false;
static volatile int s_pending_cmd = -1;
static volatile int s_pending_chg_w = -1;
static volatile int s_pending_chg_v = -1;
static TaskHandle_t s_task_handle = NULL;

bool modbus_inverter_valid(void) { return s_valid; }

void modbus_inverter_request_output(int on)
{
    s_pending_cmd = on;
    if (s_task_handle) xTaskNotifyGive(s_task_handle);
}

void modbus_inverter_request_chg_w(int watts) { s_pending_chg_w = watts; }
void modbus_inverter_request_chg_v(int tenths_v) { s_pending_chg_v = tenths_v; }

/* ── CRC16 ───────────────────────────────────────────────────────────── */

static uint16_t crc16(const uint8_t *data, int len)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

static inline int16_t s16(uint16_t v) { return (int16_t)v; }

/* ── RS485 direction helpers — software GPIO ─────────────────────────── */

static void de_tx(void) { gpio_set_level(MB_UART_DE, 1); }
static void de_rx(void)
{
    /* Hard wait: 8 bytes @ 9600 baud = 8.33ms, so 15ms guarantees TX is done
     * even if uart_wait_tx_done returns prematurely on ESP32-P4 */
    vTaskDelay(pdMS_TO_TICKS(15));
    gpio_set_level(MB_UART_DE, 0);
}

/* ── Modbus transport ────────────────────────────────────────────────── */

static int mb_read_regs(uint16_t start, uint8_t count, uint16_t *out)
{
    uint8_t req[8];
    req[0] = MB_SLAVE;
    req[1] = 0x03;
    req[2] = start >> 8;
    req[3] = start & 0xFF;
    req[4] = 0;
    req[5] = count;
    uint16_t c = crc16(req, 6);
    req[6] = c & 0xFF;
    req[7] = c >> 8;

    uart_flush_input(MB_UART_NUM);
    de_tx();
    vTaskDelay(pdMS_TO_TICKS(1));
    uart_write_bytes(MB_UART_NUM, req, 8);
    de_rx();

    int expected = 5 + count * 2;
    uint8_t resp[64];
    int got = uart_read_bytes(MB_UART_NUM, resp, expected, pdMS_TO_TICKS(200));
    if (got < expected)
    {
        ESP_LOGW(TAG, "RX timeout: got %d/%d (reg %u)", got, expected, start);
        return -1;
    }

    if (resp[1] & 0x80)
    {
        ESP_LOGW(TAG, "Modbus exc 0x%02X", resp[2]);
        return -2;
    }
    if (resp[2] != count * 2)
        return -3;
    uint16_t resp_crc = (uint16_t)resp[expected - 2] | ((uint16_t)resp[expected - 1] << 8);
    if (resp_crc != crc16(resp, expected - 2))
    {
        ESP_LOGW(TAG, "CRC fail");
        return -4;
    }

    for (int i = 0; i < count; i++)
        out[i] = ((uint16_t)resp[3 + i * 2] << 8) | resp[4 + i * 2];
    return count;
}

static bool mb_write_reg(uint16_t addr, uint16_t value)
{
    uint8_t req[8];
    req[0] = MB_SLAVE;
    req[1] = 0x06;
    req[2] = addr >> 8;
    req[3] = addr & 0xFF;
    req[4] = value >> 8;
    req[5] = value & 0xFF;
    uint16_t c = crc16(req, 6);
    req[6] = c & 0xFF;
    req[7] = c >> 8;

    uart_flush_input(MB_UART_NUM);
    de_tx();
    vTaskDelay(pdMS_TO_TICKS(1));
    uart_write_bytes(MB_UART_NUM, req, 8);
    de_rx();
    uint8_t resp[8];
    int got = uart_read_bytes(MB_UART_NUM, resp, 8, pdMS_TO_TICKS(500));
    if (got == 8) {
        ESP_LOGI(TAG, "Write ACK %04X=%u", addr, value);
        return true;
    }
    ESP_LOGW(TAG, "Write NACK addr=%04X (got %d/8)", addr, got);
    return false;
}

/* ── Main poll task ──────────────────────────────────────────────────── */

static void modbus_task(void *arg)
{
    s_task_handle = xTaskGetCurrentTaskHandle();
    ESP_LOGI(TAG, "Modbus RTU polling started (UART%d TX=%d RX=%d DE=%d)",
             MB_UART_NUM, MB_UART_TX, MB_UART_RX, MB_UART_DE);

    while (1)
    {
        /* Pending output ON/OFF command */
        int cmd = s_pending_cmd;
        if (cmd >= 0)
        {
            s_pending_cmd = -1;
            xSemaphoreTake(rs485_mutex, portMAX_DELAY);
            ESP_LOGI(TAG, "WRITE  output %s sending  t=%lldms", cmd ? "ON" : "OFF",
                     esp_timer_get_time() / 1000);
            bool acked = mb_write_reg(REG_OUT_CTRL, cmd ? 1 : 0);
            xSemaphoreGive(rs485_mutex);
            ESP_LOGI(TAG, "Output %s", cmd ? "ON" : "OFF");
            if (acked) {
                lvgl_acquire();
                gd.out_switch = cmd;
                screen_battery_set_output_state(gd.inv_on, cmd);
                lvgl_release();
            }
        }

        /* Pending charge power setpoint */
        int chg = s_pending_chg_w;
        if (chg >= 0)
        {
            s_pending_chg_w = -1;
            xSemaphoreTake(rs485_mutex, portMAX_DELAY);
            mb_write_reg(REG_CHG_CTRL, (uint16_t)chg);
            xSemaphoreGive(rs485_mutex);
            ESP_LOGI(TAG, "Charge W set → %d W", chg);
        }

        /* Pending charge voltage setpoint */
        int chgv = s_pending_chg_v;
        if (chgv >= 0)
        {
            s_pending_chg_v = -1;
            xSemaphoreTake(rs485_mutex, portMAX_DELAY);
            mb_write_reg(REG_CHG_VOLT, (uint16_t)chgv);
            xSemaphoreGive(rs485_mutex);
            ESP_LOGI(TAG, "Charge V set → %.1f V", chgv * 0.1f);
        }

        /* ── Read all register blocks under one mutex hold ────── */
        uint16_t r1[REG_B1_COUNT], r2[REG_B2_COUNT], r3[REG_B3_COUNT];
        xSemaphoreTake(rs485_mutex, portMAX_DELAY);
        int rc1 = mb_read_regs(REG_B1_START, REG_B1_COUNT, r1);
        vTaskDelay(pdMS_TO_TICKS(50));
        int rc2 = mb_read_regs(REG_B2_START, REG_B2_COUNT, r2);
        vTaskDelay(pdMS_TO_TICKS(50));
        int rc3 = mb_read_regs(REG_B3_START, REG_B3_COUNT, r3);
        xSemaphoreGive(rs485_mutex);

        if (rc1 < 0 || rc2 < 0)
        {
            ESP_LOGW(TAG, "Poll failed (rc1=%d rc2=%d)", rc1, rc2);
            s_valid = false;
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
            continue;
        }

        /* ── Decode ───────────────────────────────────────────── */
        float pv_v = r1[0] * 0.1f;
        float pv_a = s16(r1[1]) * 0.01f;
        int pv_w = s16(r1[2]);
        float batt_v = r1[7] * 0.1f;
        float batt_a = s16(r1[8]) * 0.01f;
        int batt_w = s16(r1[9]);
        float raw_grid_v = r1[11] * 0.1f;
        float grid_a = s16(r1[12]) * 0.01f; /* 4029 grid current ×0.1A */
        int grid_chg_w = s16(r1[14]);       /* 4031 mains charge power W */
        float grid_hz = r1[15] * 0.01f;
        float inv_out_v = r2[0] * 0.1f;
        float out_a = s16(r2[1]) * 0.01f;
        int out_w = s16(r2[2]);
        int out_switch = (rc3 >= 0) ? (int)r3[0] : 0; /* 4049 output switch */
        int chg_set_w = (rc3 >= 0) ? (int)r3[5] : 0;  /* 4054 charge power W */
        int chgv_set_v = (rc3 >= 0) ? (int)r3[7] : 0; /* 4056 charge voltage ×0.1V */

        float out_hz = r2[3] * 0.01f;
        uint16_t op_st = r2[4];
        float inv_t = s16(r2[7]) * 0.1f;

        int is_bypassing = (op_st >> 10) & 1;
        int inv_on = (op_st >> 8) & 1;
        int ac_chg = (op_st >> 9) & 1;
        int fault = (op_st >> 11) & 1;
        float out_v = is_bypassing ? raw_grid_v : inv_out_v;

        if ((out_hz > 0.0f && out_hz < 44.0f) || out_hz > 56.0f)
        {
            ESP_LOGW(TAG, "Bad out_hz %.2f — discard", out_hz);
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
            continue;
        }

        /* Bad grid_hz means AC is gone — zero grid data, keep rest */
        if (grid_hz < 44.0f || grid_hz > 56.0f)
        {
            ESP_LOGW(TAG, "Bad grid_hz %.2f — grid data zeroed", grid_hz);
            grid_hz = 0.0f;
            raw_grid_v = 0.0f;
            grid_a = 0.0f;
        }

        int batt_ok = (batt_v >= 28.8f && batt_v <= 57.6f);

        float grid_v = raw_grid_v >= 80.0f ? raw_grid_v : 0.0f;

        /* ── Write to gd ────────────────────────────────────────── */
        lvgl_acquire();
        gd.solar_kw = pv_w > 0 ? (float)pv_w / 1000.0f : 0.0f;
        gd.pv_v = pv_v > 0.0f ? pv_v : 0.0f;
        gd.pv_a = pv_a > 0.0f ? pv_a : 0.0f;
        gd.load_kw = out_w > 20 ? (float)out_w / 1000.0f : 0.0f;
        /* gd.batt_pct owned by BMS task */
        gd.batt_v = batt_ok ? batt_v : 0.0f;
        gd.batt_a = batt_ok ? batt_a : 0.0f;
        gd.out_switch = out_switch;
        gd.chg_kw = (float)batt_w / 1000.0f;
        gd.chg_set_w = chg_set_w;
        gd.chgv_set_v = chgv_set_v;
        gd.batt_temp = (-10.0f <= inv_t && inv_t <= 120.0f) ? inv_t : 0.0f;
        gd.backup_h = 0;
        gd.backup_m = 0;
        gd.grid_v = grid_v;
        gd.grid_hz = grid_hz;
        gd.grid_a = grid_a;
        gd.grid_chg_w = grid_chg_w;
        gd.out_v = out_v;
        gd.out_hz = out_hz;
        gd.out_a = out_a;
        gd.inv_on = inv_on;
        gd.ac_chg = ac_chg;
        gd.bypassing = is_bypassing;
        gd.fault = fault;
        gd.voltage = (int)(batt_ok ? batt_v : 0.0f);
        gd.current = batt_a;
        s_valid = true;
        lvgl_release();

        ESP_LOGI(TAG,
                 "pv=%.1fV/%.1fA/%dW batt=%.1fV/%.1fA "
                 "out=%.1fV/%.2fHz/%dW/%.1fA grid=%.1fV/%.1fA/%.2fHz temp=%.1fC "
                 "inv=%d bypass=%d ac_chg=%d fault=%d op_st=0x%04X",
                 pv_v, pv_a, pv_w, batt_v, batt_a,
                 out_v, out_hz, out_w, out_a, grid_v, grid_a, grid_hz, inv_t,
                 inv_on, is_bypassing, ac_chg, fault, op_st);

        /* Sleep 1s — but wake immediately if output command arrives */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
    }
}

/* ── Init ────────────────────────────────────────────────────────────── */

void modbus_inverter_start(void)
{
    rs485_bus_init();
    xTaskCreate(modbus_task, "modbus_inv", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "Modbus RTU ready (UART%d @ %d baud)", MB_UART_NUM, MB_BAUD);
}
