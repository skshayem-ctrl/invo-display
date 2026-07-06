#pragma once
#include <stdbool.h>

void modbus_inverter_start(void);
bool modbus_inverter_valid(void);
void modbus_inverter_request_output(int on);    /* 1=ON, 0=OFF — posted async */
void modbus_inverter_request_chg_w(int watts);     /* charge power setpoint → reg 4054 */
void modbus_inverter_request_chg_v(int tenths_v);  /* charge voltage setpoint ×0.1V → reg 4056 */
