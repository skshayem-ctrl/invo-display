#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* Shared RS485 bus mutex — take before any UART transaction, give after */
extern SemaphoreHandle_t rs485_mutex;
void rs485_bus_init(void);
