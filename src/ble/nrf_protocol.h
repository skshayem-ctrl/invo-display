#pragma once
#include <stdint.h>

void nrf_init(void);
void nrf_rx_task_start(void);

/* Beacon type codes (Beacon 300/301) */
#define BEACON_300_ESP_TO_NRF   0x012C  /* ESP32-P4 → nRF54 */
#define BEACON_301_NRF_TO_ESP   0x012D  /* nRF54 → ESP32-P4 */

/* Module code for smart plug control */
#define MOD_SMART_PLUG          1

/* Opcodes for Module Code 1 */
#define OP_SMART_PLUG_CTRL      1

/* Send a smart-plug control command over Beacon 300.
 * plug_num : plug index (1-based)
 * state    : 0 = OFF, 1 = ON
 * ack_req  : 0 = no ACK, 1 = request ACK from nRF
 */
void nrf_send_plug_ctrl(uint8_t plug_num, uint8_t state, uint8_t ack_req);

/* Start the background RX task that parses incoming Beacon 301 frames. */
void nrf_rx_task_start(void);
