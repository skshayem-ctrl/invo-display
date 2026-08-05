#include "renesas_ble.h"
#include "hw_config.h"
#include "driver/uart.h"
#include "esp_log.h"

#define TAG "ble_uart"

void renesas_ble_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = BLE_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(BLE_UART_NUM, &cfg));
    /* GPIO35 = TX (ESP32 → Renesas), GPIO36 = RX (Renesas → ESP32) */
    ESP_ERROR_CHECK(uart_set_pin(BLE_UART_NUM, BLE_UART_TX, BLE_UART_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(BLE_UART_NUM, 1024, 256, 0, NULL, 0));

    ESP_LOGI(TAG, "Renesas BLE UART ready (UART%d TX=%d RX=%d @ %d baud)",
             BLE_UART_NUM, BLE_UART_TX, BLE_UART_RX, BLE_BAUD);
}
