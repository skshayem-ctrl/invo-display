#include "hal.h"
#include "hw_config.h"
#include "lvgl_port.h"
#include "jd9365_init_cmds.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_touch_gt911.h"

static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch;

/* ── Display ──────────────────────────────────────────────────────────────── */

void hal_display_init(void)
{
    /* Power on DSI PHY LDO */
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id    = DSI_PHY_LDO_CHAN,
        .voltage_mv = DSI_PHY_LDO_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &ldo_mipi_phy));

    /* DSI bus */
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
    esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id             = 0,
        .num_data_lanes     = MIPI_DSI_LANE_NUM,
        .phy_clk_src        = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = MIPI_DSI_LANE_MBPS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_cfg, &mipi_dsi_bus));

    /* DBI command channel */
    esp_lcd_panel_io_handle_t mipi_dbi_io;
    esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel = 0,
        .lcd_cmd_bits    = 8,
        .lcd_param_bits  = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_cfg, &mipi_dbi_io));

    /* JD9365 panel */
    esp_lcd_dpi_panel_config_t dpi_cfg = {
        .virtual_channel      = 0,
        .dpi_clk_src          = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz   = MIPI_DSI_DPI_CLK_MHZ,
        .in_color_format      = LCD_COLOR_FMT_RGB565,
        .num_fbs              = 1,
        .video_timing = {
            .h_size            = LCD_H_RES,
            .v_size            = LCD_V_RES,
            .hsync_back_porch  = LCD_HBP,
            .hsync_pulse_width = LCD_HSYNC,
            .hsync_front_porch = LCD_HFP,
            .vsync_back_porch  = LCD_VBP,
            .vsync_pulse_width = LCD_VSYNC,
            .vsync_front_porch = LCD_VFP,
        },
    };
    jd9365_vendor_config_t vendor_cfg = {
        .init_cmds      = jd9365_init_cmds,
        .init_cmds_size = sizeof(jd9365_init_cmds) / sizeof(jd9365_init_cmds[0]),
        .mipi_config = {
            .dsi_bus    = mipi_dsi_bus,
            .dpi_config = &dpi_cfg,
            .lane_num   = MIPI_DSI_LANE_NUM,
        },
    };
    esp_lcd_panel_dev_config_t panel_dev_cfg = {
        .reset_gpio_num  = LCD_RST_GPIO,
        .rgb_ele_order   = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel  = 16,
        .vendor_config   = &vendor_cfg,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9365(mipi_dbi_io, &panel_dev_cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));

    /* Backlight via LEDC (inverted output — 0 duty = full brightness) */
    ledc_timer_config_t bl_timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num       = LEDC_TIMER_1,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&bl_timer));
    ledc_channel_config_t bl_channel = {
        .gpio_num            = LCD_BK_LIGHT_GPIO,
        .speed_mode          = LEDC_LOW_SPEED_MODE,
        .channel             = LEDC_CHANNEL_0,
        .intr_type           = LEDC_INTR_DISABLE,
        .timer_sel           = LEDC_TIMER_1,
        .duty                = 1023,
        .hpoint              = 0,
        .flags.output_invert = 1,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&bl_channel));

    /* LVGL init + PSRAM pool */
    lv_init();
    void *psram_pool = heap_caps_malloc(512 * 1024, MALLOC_CAP_SPIRAM);
    if (psram_pool)
        lv_mem_add_pool(psram_pool, 512 * 1024);

    lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_user_data(display, s_panel);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    size_t draw_buf_sz = LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color_t);
    void  *buf1 = heap_caps_aligned_calloc(4, 1, draw_buf_sz, MALLOC_CAP_SPIRAM);
    void  *buf2 = heap_caps_aligned_calloc(4, 1, draw_buf_sz, MALLOC_CAP_SPIRAM);
    assert(buf1 && buf2);
    lv_display_set_buffers(display, buf1, buf2, draw_buf_sz,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, lvgl_flush_cb);

    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(s_panel, &cbs, display));

    esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb,
        .name     = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000));
}

/* ── Touch ────────────────────────────────────────────────────────────────── */

void hal_touch_init(void)
{
    i2c_master_bus_config_t i2c_cfg = {
        .clk_source               = I2C_CLK_SRC_DEFAULT,
        .i2c_port                 = I2C_NUM_0,
        .scl_io_num               = TOUCH_I2C_SCL,
        .sda_io_num               = TOUCH_I2C_SDA,
        .glitch_ignore_cnt        = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &i2c_bus));

    gpio_config_t rst_gpio_cfg = {
        .pin_bit_mask = BIT64(TOUCH_RST_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&rst_gpio_cfg));

    /* After a flash reset (no power cycle) two things can go wrong:
     *  1. GT911 was mid I2C transaction → SDA stuck low
     *  2. INT pin floats HIGH → GT911 latches address 0x14 instead of 0x5D
     * Fix: RST reset + i2c_master_bus_reset() (9 SCL pulses unstick SDA),
     * then try both addresses. */
    const uint8_t addrs[2] = {
        ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,         /* 0x5D — INT low at reset */
        ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP,  /* 0x14 — INT high at reset */
    };
    for (int attempt = 0; attempt < 2; attempt++) {
        gpio_set_level(TOUCH_RST_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(TOUCH_RST_GPIO, 1);
        i2c_master_bus_reset(i2c_bus);   /* 9 SCL pulses — clears stuck SDA */
        vTaskDelay(pdMS_TO_TICKS(200));

        esp_lcd_panel_io_handle_t tp_io;
        esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        tp_io_cfg.dev_addr = addrs[attempt];
        if (esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_cfg, &tp_io) != ESP_OK) continue;

        esp_lcd_touch_config_t tp_cfg = {
            .x_max        = LCD_H_RES,
            .y_max        = LCD_V_RES,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = TOUCH_INT_GPIO,
            .levels.reset = 0,
        };
        esp_err_t err = esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &s_touch);
        if (err == ESP_OK) {
            ESP_LOGI("hal", "GT911 OK at addr 0x%02X", addrs[attempt]);
            break;
        }
        ESP_LOGW("hal", "GT911 addr 0x%02X failed (%s)", addrs[attempt], esp_err_to_name(err));
        esp_lcd_panel_io_del(tp_io);
        s_touch = NULL;
    }

    if (s_touch == NULL) {
        ESP_LOGW("hal", "GT911 init failed — touch disabled");
        return;
    }

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    lv_indev_set_user_data(indev, s_touch);
}

/* ── Backlight ────────────────────────────────────────────────────────────── */

void hal_brightness_set(int percent)
{
    /* output_invert=1 means duty=1023 → backlight OFF, duty=0 → full on  */
    uint32_t duty = ((100 - percent) * 1023) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

