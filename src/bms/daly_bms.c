/*
 * daly_bms.c — DALY BMS RS485 driver (9600 8N1, address 0x40)
 *
 * Shared RS485 bus with inverter (UART1, GPIO37/38/22).
 * BMS A/B wires must be connected in parallel with inverter RS485 A/B.
 *
 * Protocol (half-duplex, big-endian):
 *   Request:  A5 40 {cmd} 08 00*8 {crc}   (13 bytes)
 *   Response: A5 01 {cmd} 08 {8 data} {crc} (13 bytes)
 *   CRC = sum of all preceding bytes & 0xFF
 */
#include "daly_bms.h"
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

#define TAG      "bms"
#define BMS_ADDR 0x40

/* ── RS485 direction helpers ──────────────────────────────────────── */

static void de_tx(void) { gpio_set_level(BMS_UART_DE, 1); }
static void de_rx(void)
{
    vTaskDelay(pdMS_TO_TICKS(15)); /* wait for last byte to finish clocking out */
    gpio_set_level(BMS_UART_DE, 0);
}

/* ── DALY protocol ────────────────────────────────────────────────── */

static uint8_t daly_crc(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) crc += data[i];
    return crc;
}

/* Send one command and receive one 13-byte response — mutex must already be held.
 * Returns true and fills resp_data[8] on success. */
static bool daly_cmd(uint8_t cmd, uint8_t *resp_data)
{
    uint8_t req[13] = {0xA5, BMS_ADDR, cmd, 0x08, 0,0,0,0,0,0,0,0, 0};
    req[12] = daly_crc(req, 12);

    uart_flush_input(BMS_UART_NUM);
    de_tx();
    vTaskDelay(pdMS_TO_TICKS(1));
    uart_write_bytes(BMS_UART_NUM, req, 13);
    de_rx();

    uint8_t resp[13];
    int got = uart_read_bytes(BMS_UART_NUM, resp, 13, pdMS_TO_TICKS(50));
    if (got < 13) {
        ESP_LOGI(TAG, "cmd 0x%02X timeout (got %d/13 bytes)", cmd, got);
        return false;
    }
    if (resp[0] != 0xA5 || resp[1] != 0x01 || resp[2] != cmd) {
        ESP_LOGW(TAG, "cmd 0x%02X bad header: %02X %02X %02X (expected A5 01 %02X)",
                 cmd, resp[0], resp[1], resp[2], cmd);
        return false;
    }
    if (daly_crc(resp, 12) != resp[12]) {
        ESP_LOGW(TAG, "cmd 0x%02X CRC fail (got 0x%02X want 0x%02X)",
                 cmd, resp[12], daly_crc(resp, 12));
        return false;
    }
    memcpy(resp_data, resp + 4, 8);
    return true;
}

/* ── BMS poll task ────────────────────────────────────────────────── */

static void bms_task(void *arg)
{
    ESP_LOGI(TAG, "DALY BMS task started (UART%d TX=%d RX=%d DE=%d)",
             BMS_UART_NUM, BMS_UART_TX, BMS_UART_RX, BMS_UART_DE);

    while (1) {
        uint8_t d[8];
        float soc = 0, pack_v = 0, pack_a = 0;
        float cell_max = 0, cell_min = 0, cell_diff = 0;
        int cycles = 0;
        float temp = 0;

        xSemaphoreTake(rs485_mutex, portMAX_DELAY);

        /* 0x90 — SOC, pack voltage, current (critical — bail early if it fails) */
        if (!daly_cmd(0x90, d)) {
            xSemaphoreGive(rs485_mutex);
            ESP_LOGI(TAG, "0x90 failed — keeping last display values");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        pack_v = (uint16_t)(d[0] << 8 | d[1]) * 0.1f;
        int16_t raw_a = (int16_t)((uint16_t)(d[4] << 8 | d[5]) - 30000);
        pack_a = raw_a * 0.1f;
        soc    = (uint16_t)(d[6] << 8 | d[7]) * 0.1f;
        vTaskDelay(pdMS_TO_TICKS(50));

        /* 0x91 — min/max cell voltage */
        vTaskDelay(pdMS_TO_TICKS(50));
        if (!daly_cmd(0x91, d)) {
            xSemaphoreGive(rs485_mutex);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        cell_max  = (uint16_t)(d[0] << 8 | d[1]) * 0.001f;
        cell_min  = (uint16_t)(d[3] << 8 | d[4]) * 0.001f;
        cell_diff = (cell_max - cell_min) * 1000.0f;

        /* 0x94 — cycle count */
        vTaskDelay(pdMS_TO_TICKS(50));
        if (!daly_cmd(0x94, d)) {
            xSemaphoreGive(rs485_mutex);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        cycles = (uint16_t)(d[5] << 8 | d[6]);

        /* 0x96 — temperatures (raw - 40 = °C, 0 = unused sensor) */
        vTaskDelay(pdMS_TO_TICKS(50));
        if (!daly_cmd(0x96, d)) {
            xSemaphoreGive(rs485_mutex);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        float max_t = -40.0f;
        for (int i = 1; i < 8; i++) {
            if (d[i] == 0 || d[i] == 0xFF) continue;
            float t = (float)d[i] - 40.0f;
            if (t > max_t) max_t = t;
        }
        temp = max_t > -40.0f ? max_t : 0.0f;

        /* 0x97 — remaining / rated capacity (raw bytes logged to find scaling) */
        float remain_ah = 0.0f;
        vTaskDelay(pdMS_TO_TICKS(50));
        if (daly_cmd(0x97, d)) {
            uint32_t raw_rem  = (uint32_t)(d[0] << 24 | d[1] << 16 | d[2] << 8 | d[3]);
            uint32_t raw_full = (uint32_t)(d[4] << 24 | d[5] << 16 | d[6] << 8 | d[7]);
            ESP_LOGI(TAG, "0x97 raw: rem=0x%08lX(%lu) full=0x%08lX(%lu)",
                     raw_rem, raw_rem, raw_full, raw_full);
            remain_ah = raw_rem * 0.001f; /* assuming mAh units — verify from log */
        }

        xSemaphoreGive(rs485_mutex);

        /* Backup time — only when discharging and 0x97 gave us remaining Ah */
        int bkp_h = 0, bkp_m = 0, bkp_valid = 0;
        if (remain_ah > 0.0f && pack_a < -0.5f) {
            float time_h = remain_ah / (-pack_a);
            bkp_h     = (int)time_h;
            bkp_m     = (int)((time_h - bkp_h) * 60.0f);
            bkp_valid = 1;
        }

        /* Update display — only reached when 0x90 succeeded */
        lvgl_acquire();
        gd.batt_pct      = (int)soc;
        gd.bms_cell_min  = cell_min;
        gd.bms_cell_max  = cell_max;
        gd.bms_cell_diff = cell_diff;
        gd.bms_temp      = temp;
        gd.bms_cycles    = cycles;
        gd.bms_valid     = 1;
        gd.backup_h      = bkp_h;
        gd.backup_m      = bkp_m;
        gd.backup_valid  = bkp_valid;
        lvgl_release();

        ESP_LOGI(TAG, "soc=%.1f%% v=%.1fV a=%.1fA cell=%.3f-%.3fV(%.0fmV) temp=%.1fC cyc=%d rem=%.2fAh",
                 soc, pack_v, pack_a, cell_min, cell_max, cell_diff, temp, cycles, remain_ah);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* ── Init ─────────────────────────────────────────────────────────── */

void daly_bms_start(void)
{
    xTaskCreate(bms_task, "bms", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "DALY BMS ready (shared RS485 bus, addr=0x%02X)", BMS_ADDR);
}
