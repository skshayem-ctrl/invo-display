/*
 * led_effect.c — renders HLK-LD2450 target tracking onto a WS2812B
 * strip: each of the sensor's 3 target slots gets a fixed color, placed
 * along the strip by left/right angle and dimmed by distance.
 *
 * Angle/range mapping uses the sensor's rated envelope (LD2450_FOV_DEG,
 * LD2450_MAX_RANGE_MM in hw_config.h) — not arbitrary constants.
 *
 * Known limitation: the LD2450 gives 3 *anonymous* target slots — it does
 * not guarantee a slot stays assigned to the same physical person as people
 * move or cross paths, so these fixed per-slot colors can visibly swap
 * between people. That's a sensor-protocol limitation, not fixable here.
 */
#include "led_effect.h"
#include "ld2450.h"
#include "hw_config.h"
#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

#define TAG            "led_fx"
#define RENDER_HZ      30
#define BLOB_HALF_W    1.2f    /* pixels either side of center that get some brightness */
#define MIN_BRIGHTNESS 8       /* floor so a target at max range isn't fully invisible */
#define RAD_TO_DEG     57.29577951308232f
#define BRIGHTNESS_GAMMA 2.4f  /* perceptual correction — see render_tick() comment */

typedef struct { uint8_t r, g, b; } rgb_t;

static const rgb_t s_slot_color[3] = {
    /* TEMPORARY DIAGNOSTIC — slot 0 swapped red->green to test whether the
     * "barely visible" dimming is a perceptual/color-channel issue (human
     * scotopic vision is nearly blind to red) rather than the brightness
     * curve. Revert to {255,40,40} once this is confirmed either way. */
    { 255,  40,  40 },  /* slot 0 — red */
    {  40, 255,  60 },  /* slot 1 — green */
    {  60, 100, 255 },  /* slot 2 — blue */
};

static led_strip_handle_t s_strip;
static bool s_ready;

/* Per-slot EMA so noisy frame-to-frame radar jitter doesn't flicker the
 * strip; angle/distance are smoothed independently per slot. */
static float s_angle_ema[3];
static float s_dist_ema[3];
static bool  s_ema_init[3];

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void render_tick(void)
{
    ld2450_target_t t[3];
    ld2450_get_targets(t);

    static uint32_t fb[LED_STRIP_NUM_LEDS][3]; /* r,g,b accumulator, additive */
    memset(fb, 0, sizeof(fb));

    /* TEMPORARY — remove once brightness-vs-distance is confirmed on hardware. */
    static int s_dbg_ctr = 0;
    bool dbg_log = (++s_dbg_ctr % 15) == 0; /* ~2x/sec at RENDER_HZ=30 */

    for (int i = 0; i < 3; i++) {
        if (!t[i].present) {
            s_ema_init[i] = false;
            continue;
        }

        float angle = atan2f((float)t[i].x_mm, (float)t[i].y_mm) * RAD_TO_DEG;
        float dist  = sqrtf((float)t[i].x_mm * t[i].x_mm + (float)t[i].y_mm * t[i].y_mm);

        if (!s_ema_init[i]) {
            s_angle_ema[i] = angle;
            s_dist_ema[i]  = dist;
            s_ema_init[i]  = true;
        } else {
            s_angle_ema[i] = s_angle_ema[i] * 0.7f + angle * 0.3f;
            s_dist_ema[i]  = s_dist_ema[i]  * 0.7f + dist  * 0.3f;
        }

        float a   = clampf(s_angle_ema[i], -LD2450_FOV_DEG, LD2450_FOV_DEG);
        float pos = (a + LD2450_FOV_DEG) / (2.0f * LD2450_FOV_DEG) * (LED_STRIP_NUM_LEDS - 1);

        /* WS2812 PWM duty isn't perceived linearly by the eye (Stevens' power
         * law) — most values above ~80/255 all look "full brightness," so a
         * linear distance->PWM map only looks dimmer right at the sensor's
         * rated max range. Gamma-correct so the falloff is visible across a
         * normal room-scale walk instead of only at the very edge. */
        float d          = clampf(s_dist_ema[i], 0.0f, LD2450_MAX_RANGE_MM);
        float brightness = powf(1.0f - (d / LD2450_MAX_RANGE_MM), BRIGHTNESS_GAMMA);
        uint8_t level    = (uint8_t)clampf(brightness * 255.0f, MIN_BRIGHTNESS, 255.0f);

        if (dbg_log) {
            ESP_LOGI(TAG, "slot%d raw(x=%d,y=%d) dist_raw=%.0fmm dist_ema=%.0fmm brightness=%.3f level=%u",
                     i, t[i].x_mm, t[i].y_mm, dist, s_dist_ema[i], brightness, level);
        }

        int lo = (int)floorf(pos - BLOB_HALF_W);
        int hi = (int)ceilf(pos + BLOB_HALF_W);
        if (lo < 0) lo = 0;
        if (hi > LED_STRIP_NUM_LEDS - 1) hi = LED_STRIP_NUM_LEDS - 1;

        for (int p = lo; p <= hi; p++) {
            float   falloff = clampf(1.0f - fabsf((float)p - pos) / BLOB_HALF_W, 0.0f, 1.0f);
            uint8_t s        = (uint8_t)(level * falloff);
            fb[p][0] += (uint32_t)s_slot_color[i].r * s / 255;
            fb[p][1] += (uint32_t)s_slot_color[i].g * s / 255;
            fb[p][2] += (uint32_t)s_slot_color[i].b * s / 255;
        }
    }

    for (int p = 0; p < LED_STRIP_NUM_LEDS; p++) {
        uint32_t r = fb[p][0] > 255 ? 255 : fb[p][0];
        uint32_t g = fb[p][1] > 255 ? 255 : fb[p][1];
        uint32_t b = fb[p][2] > 255 ? 255 : fb[p][2];
        led_strip_set_pixel(s_strip, p, r, g, b);
    }
    led_strip_refresh(s_strip);
}

static void led_effect_task(void *arg)
{
    ESP_LOGI(TAG, "LED effect task started (%d LEDs, GPIO%d)",
             LED_STRIP_NUM_LEDS, LED_STRIP_GPIO);
    while (1) {
        render_tick();
        vTaskDelay(pdMS_TO_TICKS(1000 / RENDER_HZ));
    }
}

void led_effect_init(void)
{
    if (LED_STRIP_GPIO < 0) {
        ESP_LOGW(TAG, "LED_STRIP_GPIO not configured — LED effect disabled");
        return;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num        = LED_STRIP_GPIO,
        .max_leds              = LED_STRIP_NUM_LEDS,
        .led_model             = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = { .invert_out = false },
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = { .with_dma = false },
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
        return;
    }
    led_strip_clear(s_strip);
    s_ready = true;
}

void led_effect_start(void)
{
    if (!s_ready) return;
    xTaskCreate(led_effect_task, "led_fx", 4096, NULL, 3, NULL);
}
