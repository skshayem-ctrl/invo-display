#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl/lvgl.h"
#include "pi5/lib/driver_backends.h"
#include "pi5/lib/simulator_util.h"
#include "pi5/lib/simulator_settings.h"

#include "src/hal/hal.h"
#include "src/ui_common.h"
#include "src/wifi_manager.h"
#include "src/weather_service.h"
#include "src/uart_input.h"

static char *selected_backend;
extern simulator_settings_t settings;

static void configure_simulator(int argc, char **argv)
{
    selected_backend = NULL;
    driver_backends_register();

    const char *env_w = getenv("LV_SIM_WINDOW_WIDTH");
    const char *env_h = getenv("LV_SIM_WINDOW_HEIGHT");
    settings.window_width  = atoi(env_w ? env_w : "800");
    settings.window_height = atoi(env_h ? env_h : "800");

    int opt;
    while ((opt = getopt(argc, argv, "b:fmW:H:R:BVh")) != -1) {
        switch (opt) {
            case 'b': selected_backend = strdup(optarg); break;
            case 'f': settings.fullscreen = true; break;
            case 'm': settings.maximize   = true; break;
            case 'W': settings.window_width  = atoi(optarg); break;
            case 'H': settings.window_height = atoi(optarg); break;
            case 'V':
                fprintf(stdout, "%d.%d.%d-%s\n",
                        LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
                        LVGL_VERSION_PATCH, LVGL_VERSION_INFO);
                exit(EXIT_SUCCESS);
            case 'B': driver_backends_print_supported(); exit(EXIT_SUCCESS);
            default:  break;
        }
    }
}

int main(int argc, char **argv)
{
    configure_simulator(argc, argv);

    lv_init();

    /* Display + touch via HAL (wraps driver_backends / evdev) */
    hal_display_init();
    hal_touch_init();

    /* Platform services */
    wifi_manager_init();
    weather_service_start();
    uart_input_start();

    /* Build main screen */
    app.clk_h = 10; app.clk_m = 30; app.clk_s = 0;
    app.scr_main = screen_main_create();
    lv_screen_load(app.scr_main);
    lv_timer_create(clock_tick_cb, 1000, NULL);
    lv_timer_create(data_tick_cb,  2000, NULL);

    /* Pre-build all detail screens (Pi has plenty of RAM) */
    app.scr_batt     = screen_battery_create();
    app.scr_solar    = screen_solar_create();
    app.scr_wx       = screen_weather_create();
    app.scr_sleep    = screen_sleep_create();
    app.scr_load     = screen_load_create();
    app.scr_settings = screen_settings_create();
    app.scr_history  = screen_history_create();
    app.scr_alerts   = screen_alerts_create();

    driver_backends_run_loop();

    return 0;
}
