/*
 * ld2450.c — HLK-LD2450 24GHz mmWave human-tracking radar driver
 *
 * Plain UART (no RS485 DE), 256000 8N1 — UART2 on GPIO4/5, the pins freed by
 * this session's removal of the invo_debug module.
 *
 * Data output frame (streamed continuously, 10Hz, 30 bytes, little-endian):
 *   AA FF 03 00 | 3x(X:i16 Y:i16 Speed:i16 DistRes:u16) | 55 CC
 * X/Y/Speed use inverted sign-magnitude, NOT two's complement:
 *   magnitude = raw & 0x7FFF; sign = (raw & 0x8000) ? +1 : -1; value = sign*magnitude
 * (Config-mode commands, e.g. zone filtering, use plain two's-complement
 * int16 instead for their coordinate fields — a different encoding not used
 * by this driver. See "LD2450 serial port communication protocol V1.03.pdf"
 * if config commands are ever needed.)
 * A target slot with all-zero raw bytes means "not present."
 */
#include "ld2450.h"
#include "hw_config.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define TAG        "ld2450"
#define FRAME_LEN  30
#define RX_BUF     128

static ld2450_target_t s_targets[3];

/* LD2450's inverted sign-magnitude 16-bit encoding (data frames only). */
static int16_t decode_sm16(uint16_t raw)
{
    int16_t mag = (int16_t)(raw & 0x7FFF);
    return (raw & 0x8000) ? mag : (int16_t)(-mag);
}

static void parse_frame(const uint8_t *f)
{
    ld2450_target_t t[3];
    for (int i = 0; i < 3; i++) {
        const uint8_t *d = f + 4 + i * 8;
        bool zero = (d[0] | d[1] | d[2] | d[3] | d[4] | d[5] | d[6] | d[7]) == 0;
        if (zero) {
            t[i] = (ld2450_target_t){ .present = false };
            continue;
        }
        uint16_t x_raw = (uint16_t)d[0] | ((uint16_t)d[1] << 8);
        uint16_t y_raw = (uint16_t)d[2] | ((uint16_t)d[3] << 8);
        uint16_t s_raw = (uint16_t)d[4] | ((uint16_t)d[5] << 8);
        t[i].present   = true;
        t[i].x_mm      = decode_sm16(x_raw);
        t[i].y_mm      = decode_sm16(y_raw);
        t[i].speed_cms = decode_sm16(s_raw);
    }
    memcpy(s_targets, t, sizeof(t));
}

static void ld2450_rx_task(void *arg)
{
    static uint8_t buf[RX_BUF];
    int fill = 0;

    ESP_LOGI(TAG, "LD2450 RX task started (UART%d TX=%d RX=%d @ %d)",
             LD2450_UART_NUM, LD2450_UART_TX, LD2450_UART_RX, LD2450_BAUD);

    while (1) {
        int n = uart_read_bytes(LD2450_UART_NUM, buf + fill, sizeof(buf) - fill,
                                 pdMS_TO_TICKS(200));
        if (n > 0) fill += n;

        for (;;) {
            /* Resync: slide until the AA FF 03 00 magic sits at offset 0. */
            while (fill >= 4 &&
                   !(buf[0] == 0xAA && buf[1] == 0xFF && buf[2] == 0x03 && buf[3] == 0x00)) {
                memmove(buf, buf + 1, --fill);
            }
            if (fill < FRAME_LEN) break; /* need more bytes */

            if (buf[FRAME_LEN - 2] == 0x55 && buf[FRAME_LEN - 1] == 0xCC) {
                parse_frame(buf);
                memmove(buf, buf + FRAME_LEN, fill - FRAME_LEN);
                fill -= FRAME_LEN;
                continue; /* another full frame may already be buffered */
            }

            ESP_LOGW(TAG, "bad frame tail, resyncing");
            memmove(buf, buf + 1, --fill);
        }
    }
}

void ld2450_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = LD2450_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(LD2450_UART_NUM, 512, 0, 0, NULL, 0);
    uart_param_config(LD2450_UART_NUM, &cfg);
    uart_set_pin(LD2450_UART_NUM, LD2450_UART_TX, LD2450_UART_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void ld2450_start(void)
{
    xTaskCreate(ld2450_rx_task, "ld2450_rx", 3072, NULL, 4, NULL);
}

void ld2450_get_targets(ld2450_target_t out[3])
{
    memcpy(out, s_targets, sizeof(s_targets));
}
