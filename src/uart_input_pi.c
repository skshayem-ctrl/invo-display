#include "uart_input.h"

/* Pi: UART data comes via invo_bridge.py → shared memory, read by
 * hal_data_get() inside data_tick_cb. No separate UART task needed.
 * All validity flags return true so data_tick_cb skips simulation. */

void uart_input_start(void)      {}
bool uart_batt_valid(void)       { return true; }
bool uart_solar_valid(void)      { return true; }
bool uart_load_valid(void)       { return true; }
bool uart_voltage_valid(void)    { return true; }
bool uart_current_valid(void)    { return true; }
