idf_component_register(
    SRCS
    "main.c"
    # Display / LVGL
    "${CMAKE_SOURCE_DIR}/src/display/hal_esp32.c"
    "${CMAKE_SOURCE_DIR}/src/display/lvgl_port.c"
    # UI screens
    "${CMAKE_SOURCE_DIR}/src/ui/ui_common.c"
    "${CMAKE_SOURCE_DIR}/src/ui/weather_icon.c"
    "${CMAKE_SOURCE_DIR}/src/ui/screen_main.c"
    "${CMAKE_SOURCE_DIR}/src/ui/screen_battery.c"
    "${CMAKE_SOURCE_DIR}/src/ui/screen_battery_settings.c"
    "${CMAKE_SOURCE_DIR}/src/ui/screen_solar.c"
    "${CMAKE_SOURCE_DIR}/src/ui/screen_weather.c"
    "${CMAKE_SOURCE_DIR}/src/ui/screen_sleep.c"
    "${CMAKE_SOURCE_DIR}/src/ui/screen_wifi.c"
    "${CMAKE_SOURCE_DIR}/src/ui/screen_settings_general.c"
    "${CMAKE_SOURCE_DIR}/src/ui/screen_settings.c"
    "${CMAKE_SOURCE_DIR}/src/ui/screen_history.c"
    "${CMAKE_SOURCE_DIR}/src/ui/screen_alerts.c"
    "${CMAKE_SOURCE_DIR}/src/ui/screen_load.c"
    # WiFi
    "${CMAKE_SOURCE_DIR}/src/wifi/wifi_manager.c"
    # Inverter / UART
    "${CMAKE_SOURCE_DIR}/src/inverter/modbus_inverter.c"
    "${CMAKE_SOURCE_DIR}/src/inverter/uart_input.c"
    # FOTA
    "${CMAKE_SOURCE_DIR}/src/fota/fota.c"
    # Weather
    "${CMAKE_SOURCE_DIR}/src/weather/weather_service.c"
    INCLUDE_DIRS
    "."
    "${CMAKE_SOURCE_DIR}/src/display"
    "${CMAKE_SOURCE_DIR}/src/ui"
    "${CMAKE_SOURCE_DIR}/src/wifi"
    "${CMAKE_SOURCE_DIR}/src/inverter"
    "${CMAKE_SOURCE_DIR}/src/fota"
    "${CMAKE_SOURCE_DIR}/src/weather"
    REQUIRES
    esp_timer
    esp_driver_gpio
    esp_driver_ledc
    esp_driver_i2c
    esp_lcd
    esp_hw_support
    esp_wifi
    nvs_flash
    esp_event
    esp_netif
    esp_http_client
    mbedtls
    esp_driver_uart
    json
    app_update
    esp_https_ota
)
