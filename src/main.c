#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_ota_ops.h"

#include "hal.h"
#include "hw_config.h"
#include "ui_common.h"
#include "lvgl_port.h"
#include "wifi_manager.h"
#include "weather_service.h"
#include "uart_input.h"
#include "daly_bms.h"

void app_main(void)
{
    /* Mark this OTA slot valid so the bootloader won't roll back */
    esp_ota_mark_app_valid_cancel_rollback();

    hal_display_init();   /* LDO + DSI + panel + LEDC backlight + LVGL init */
    hal_touch_init();     /* I2C + GT911 + LVGL indev */

    wifi_manager_init();  /* after display so the screen isn't blank during 15s WiFi wait */

    /* Build main screen and start UI timers */
    app.clk_h = 10;
    app.clk_m = 30;
    app.clk_s = 0;
    app.scr_main = screen_main_create();
    lv_screen_load(app.scr_main);
    lv_timer_create(clock_tick_cb, 1000, NULL);
    lv_timer_create(data_tick_cb,  2000, NULL);

    weather_service_start();
    uart_input_start();
    daly_bms_start();

    /* Detail screens are pre-built in background by lvgl_task */
    app.startup_phase = STARTUP_PHASES;
    xTaskCreate(lvgl_task, "lvgl_task", 16384, NULL, 4, NULL);
}
