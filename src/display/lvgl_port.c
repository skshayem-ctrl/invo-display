#include <sys/lock.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_touch_gt911.h"

#include "lvgl.h"
#include "ui_common.h"
#include "hw_config.h"
#include "lvgl_port.h"

static _lock_t lvgl_api_lock;

void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1, px_map);
}

bool notify_lvgl_flush_ready(esp_lcd_panel_handle_t panel,
                             esp_lcd_dpi_panel_event_data_t *edata,
                             void *user_ctx)
{
    lv_display_flush_ready((lv_display_t *)user_ctx);
    return false;
}

void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t touch = lv_indev_get_user_data(indev);
    esp_lcd_touch_point_data_t point;
    uint8_t count = 0;
    esp_lcd_touch_read_data(touch);
    if (esp_lcd_touch_get_data(touch, &point, &count, 1) == ESP_OK && count > 0) {
        data->point.x = point.x;
        data->point.y = point.y;
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void lvgl_task(void *arg)
{
    while (1) {
        _lock_acquire(&lvgl_api_lock);
        uint32_t delay_ms = lv_timer_handler();
        _lock_release(&lvgl_api_lock);

        /* Guard: highest case number must equal STARTUP_PHASES in hw_config.h */
        _Static_assert(STARTUP_PHASES == 8,
            "STARTUP_PHASES in hw_config.h must match the highest case in this switch");

        if (app.startup_phase > 0) {
            _lock_acquire(&lvgl_api_lock);
            switch (app.startup_phase) {
            case 8: app.scr_settings = screen_settings_create(); break;
            case 7: app.scr_history  = screen_history_create();  break;
            case 6: app.scr_alerts   = screen_alerts_create();   break;
            case 5: app.scr_batt     = screen_battery_create();  break;
            case 4: app.scr_solar = screen_solar_create();   break;
            case 3: app.scr_load  = screen_load_create();    break;
            case 2: app.scr_wx    = screen_weather_create(); break;
            case 1: app.scr_sleep = screen_sleep_create();   break;
            }
            app.startup_phase--;
            _lock_release(&lvgl_api_lock);
        }

        TickType_t ticks = pdMS_TO_TICKS(delay_ms);
        vTaskDelay(ticks > 0 ? ticks : 1);
    }
}

void lvgl_acquire(void) { _lock_acquire(&lvgl_api_lock); }
void lvgl_release(void) { _lock_release(&lvgl_api_lock); }
