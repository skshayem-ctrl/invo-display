#pragma once
#include <stdbool.h>
#include "lvgl.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_timer.h"

void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
bool notify_lvgl_flush_ready(esp_lcd_panel_handle_t panel,
                             esp_lcd_dpi_panel_event_data_t *edata,
                             void *user_ctx);
void lvgl_tick_cb(void *arg);
void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data);
void lvgl_task(void *arg);

void lvgl_acquire(void);
void lvgl_release(void);
