#include "invo_debug.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include <stdio.h>
#include <stdarg.h>

#define DBG_UART_NUM  UART_NUM_2
#define DBG_UART_TX   4
#define DBG_UART_RX   5
#define DBG_BAUD      115200
#define DBG_BUF       256

void invo_debug_init(void)
{
    uart_config_t cfg = {
        .baud_rate = DBG_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(DBG_UART_NUM, DBG_BUF, 0, 0, NULL, 0);
    uart_param_config(DBG_UART_NUM, &cfg);
    uart_set_pin(DBG_UART_NUM, DBG_UART_TX, DBG_UART_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void invo_debug_log(const char *fmt, ...)
{
    char buf[DBG_BUF];
    uint64_t ms = esp_timer_get_time() / 1000;

    int hdr = snprintf(buf, sizeof(buf), "%014llu,", ms);

    va_list args;
    va_start(args, fmt);
    int msg = vsnprintf(buf + hdr, sizeof(buf) - hdr - 2, fmt, args);
    va_end(args);

    int total = hdr + msg;
    buf[total]     = '\r';
    buf[total + 1] = '\n';

    uart_write_bytes(DBG_UART_NUM, buf, total + 2);
}
