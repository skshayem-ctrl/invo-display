/*
 * uart_input.c — compatibility shim
 * All real work is in modbus_inverter.c; these functions just forward
 * so that ui_common.c's data_tick_cb() doesn't need #ifdef changes.
 */
#include "uart_input.h"
#include "modbus_inverter.h"

bool uart_batt_valid(void)    { return modbus_inverter_valid(); }
bool uart_solar_valid(void)   { return modbus_inverter_valid(); }
bool uart_load_valid(void)    { return modbus_inverter_valid(); }
bool uart_voltage_valid(void) { return modbus_inverter_valid(); }
bool uart_current_valid(void) { return modbus_inverter_valid(); }

void uart_input_start(void)   { modbus_inverter_start(); }
