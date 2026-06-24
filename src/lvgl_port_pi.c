#include "lvgl_port.h"

/* On Pi the LVGL loop runs on the main thread and weather/uart
 * background tasks are stubbed, so no locking is needed. */
void lvgl_acquire(void) {}
void lvgl_release(void) {}
