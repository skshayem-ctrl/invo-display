#pragma once
#include <stdbool.h>

void uart_input_start(void);

/* Validity flags — false until a valid value has been received via UART.
 * Protocol:  75        → battery %
 *            S:2.4     → solar kW
 *            L:1.6     → load kW
 *            V:380     → voltage (integer)
 *            I:6.3     → current (amps)
 */
bool uart_batt_valid(void);
bool uart_solar_valid(void);
bool uart_load_valid(void);
bool uart_voltage_valid(void);
bool uart_current_valid(void);
