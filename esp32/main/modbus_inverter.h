#pragma once
#include <stdbool.h>

void modbus_inverter_start(void);
bool modbus_inverter_valid(void);
void modbus_inverter_request_output(int on);  /* 1=ON, 0=OFF — posted async */
