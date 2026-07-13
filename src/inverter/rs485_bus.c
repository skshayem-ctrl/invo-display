#include "rs485_bus.h"
#include "hw_config.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "rs485"

SemaphoreHandle_t rs485_mutex;

void rs485_bus_init(void)
{
    rs485_mutex = xSemaphoreCreateMutex();

    gpio_config_t de_cfg = {
        .pin_bit_mask = BIT64(MB_UART_DE),
        .mode         = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&de_cfg));
    gpio_set_level(MB_UART_DE, 0);

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
    ESP_ERROR_CHECK(uart_driver_install(MB_UART_NUM, 512, 0, 0, NULL, 0));

    ESP_LOGI(TAG, "RS485 bus ready (UART%d TX=%d RX=%d DE=%d @ %d baud)",
             MB_UART_NUM, MB_UART_TX, MB_UART_RX, MB_UART_DE, MB_BAUD);
}
