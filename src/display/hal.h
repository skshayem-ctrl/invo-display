#ifndef INVO_HAL_H
#define INVO_HAL_H

#include "lvgl.h"

void hal_display_init(void);
void hal_touch_init(void);

void hal_brightness_set(int percent);

#endif /* INVO_HAL_H */
