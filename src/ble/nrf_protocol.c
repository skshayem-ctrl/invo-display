#include "nrf_protocol.h"
#include "hw_config.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

#define TAG "nrf_proto"

/* ── UART init ────────────────────────────────────────────────────── */
void nrf_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = BLE_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(BLE_UART_NUM, &cfg));
    /* GPIO35 = TX (ESP32-P4 → nRF), GPIO36 = RX (nRF → ESP32-P4) */
    ESP_ERROR_CHECK(uart_set_pin(BLE_UART_NUM, BLE_UART_TX, BLE_UART_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(BLE_UART_NUM, 1024, 256, 0, NULL, 0));
    ESP_LOGI(TAG, "nRF UART ready (UART%d TX=%d RX=%d @ %d baud)",
             BLE_UART_NUM, BLE_UART_TX, BLE_UART_RX, BLE_BAUD);
}

/* ── CRC-16/CCITT-FALSE (matches server_connect.c) ───────────────── */
static uint16_t crc16(const uint8_t *d, size_t n)
{
    uint16_t c = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        c ^= (uint16_t)d[i] << 8;
        for (int b = 0; b < 8; b++)
            c = (c & 0x8000) ? ((c << 1) ^ 0x1021) : (c << 1);
    }
    return c;
}

/* ── Sequence counter ─────────────────────────────────────────────── */
static uint16_t s_seq = 0;

/* ── LA5 frame builder ────────────────────────────────────────────── */
/*
 * Wire layout (all multi-byte fields are little-endian):
 *   LA5(3) | BeaconType(2) | SeqID(2) | PayloadLen(2) | Payload(N) | CRC(2)
 *
 * CRC-16/CCITT-FALSE computed over all bytes before the CRC field.
 * Returns total frame length, or 0 on buffer overflow.
 */
static size_t la5_build(uint8_t *buf, size_t buf_sz,
                         uint16_t beacon_type,
                         const uint8_t *payload, uint16_t payload_len)
{
    size_t frame_len = 9 + payload_len + 2;   /* header(9) + payload + CRC(2) */
    if (frame_len > buf_sz) return 0;

    uint8_t *p = buf;
    *p++ = 'L'; *p++ = 'A'; *p++ = '5';
    *p++ = beacon_type & 0xFF; *p++ = beacon_type >> 8;
    uint16_t seq = s_seq++;
    *p++ = seq & 0xFF; *p++ = seq >> 8;
    *p++ = payload_len & 0xFF; *p++ = payload_len >> 8;
    memcpy(p, payload, payload_len);
    p += payload_len;
    uint16_t crc = crc16(buf, frame_len - 2);
    *p++ = crc & 0xFF; *p = crc >> 8;

    return frame_len;
}

/* ── Beacon 300: ESP32-P4 → nRF54 ─────────────────────────────────── */
/*
 * Payload layout:
 *   ModuleCount(1) | ModuleCode(2 LE) | ModuleLen(2 LE) | ModulePayload
 *
 * ModulePayload for Module Code 1:
 *   Opcode(2 LE) | OpcodeLen(2 LE) | OpcodeData(N)
 *
 * OpcodeData for OP_SMART_PLUG_CTRL:
 *   PlugNum(1) | State(1: 0=OFF, 1=ON) | AckReq(1: 0=no ack, 1=ack)
 */
void nrf_send_plug_ctrl(uint8_t plug_num, uint8_t state, uint8_t ack_req)
{
    const uint16_t mod_code    = MOD_SMART_PLUG;
    const uint16_t opcode      = OP_SMART_PLUG_CTRL;
    const uint16_t opcode_len  = 2;                       /* (AckReq<<7 | PlugNum) + State */
    const uint16_t mod_len     = 2 + 2 + opcode_len;     /* Opcode(2) + OpcodeLen(2) + data = 6 */
    const uint16_t payload_len = 1 + 2 + 2 + mod_len;    /* ModuleCount(1) + Code(2) + Len(2) + payload = 11 */

    uint8_t payload[32];
    uint8_t *p = payload;
    *p++ = 1;                              /* ModuleCount */
    *p++ = mod_code & 0xFF; *p++ = mod_code >> 8;
    *p++ = mod_len & 0xFF;  *p++ = mod_len >> 8;
    *p++ = opcode & 0xFF;   *p++ = opcode >> 8;
    *p++ = opcode_len & 0xFF; *p++ = opcode_len >> 8;
    *p++ = (uint8_t)((ack_req << 7) | (plug_num & 0x7F));  /* bit7=AckReq, bits6:0=PlugNum */
    *p++ = state;

    uint8_t frame[64];
    size_t frame_len = la5_build(frame, sizeof(frame),
                                  BEACON_300_ESP_TO_NRF, payload, payload_len);
    if (!frame_len) {
        ESP_LOGE(TAG, "B300 frame overflow");
        return;
    }

    uart_write_bytes(BLE_UART_NUM, frame, frame_len);
    ESP_LOGD(TAG, "B300 TX plug=%u state=%u ack=%u (%u B)",
             plug_num, state, ack_req, (unsigned)frame_len);
}

/* ── Beacon 301 RX: nRF54 → ESP32-P4 ─────────────────────────────── */
#define RX_BUF    256
#define FRAME_HDR 9    /* LA5(3) + type(2) + seq(2) + plen(2) */

static void nrf_rx_task(void *arg)
{
    static uint8_t buf[RX_BUF];
    size_t fill = 0;

    for (;;) {
        int n = uart_read_bytes(BLE_UART_NUM, buf + fill,
                                sizeof(buf) - fill, pdMS_TO_TICKS(50));
        if (n > 0) fill += (size_t)n;
        if (fill < 3) continue;

        /* Re-sync: slide until "LA5" is at index 0 */
        while (fill >= 3 && !(buf[0] == 'L' && buf[1] == 'A' && buf[2] == '5'))
            memmove(buf, buf + 1, --fill);

        if (fill < FRAME_HDR + 2) continue;  /* need header + CRC minimum */

        uint16_t plen    = (uint16_t)buf[7] | ((uint16_t)buf[8] << 8);
        size_t   expected = FRAME_HDR + plen + 2;

        if (expected > sizeof(buf)) {   /* malformed length — discard sync byte */
            memmove(buf, buf + 1, --fill);
            continue;
        }
        if (fill < expected) continue;  /* wait for more bytes */

        /* Verify CRC */
        uint16_t rx_crc   = (uint16_t)buf[expected - 2] | ((uint16_t)buf[expected - 1] << 8);
        uint16_t calc_crc = crc16(buf, expected - 2);
        if (rx_crc != calc_crc) {
            ESP_LOGW(TAG, "B301 CRC fail — resyncing");
            memmove(buf, buf + 1, --fill);
            continue;
        }

        uint16_t btype = (uint16_t)buf[3] | ((uint16_t)buf[4] << 8);
        uint16_t seq   = (uint16_t)buf[5] | ((uint16_t)buf[6] << 8);
        ESP_LOGI(TAG, "B301 RX type=0x%04X seq=%u plen=%u", btype, seq, plen);

        /* TODO: parse module/opcode payload from Beacon 301 */

        /* Consume the frame */
        memmove(buf, buf + expected, fill - expected);
        fill -= expected;
    }
}

void nrf_rx_task_start(void)
{
    xTaskCreate(nrf_rx_task, "nrf_rx", 3072, NULL, 5, NULL);
}
