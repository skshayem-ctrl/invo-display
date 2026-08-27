/*
 * led_effect.c — renders HLK-LD2450 target tracking onto a WS2812B
 * strip: each of the sensor's 3 target slots gets a fixed color, placed
 * along the strip by raw left/right offset (x_mm) and dimmed by distance.
 *
 * Position uses x_mm directly (not angle) clamped to LED_POS_MAX_X_MM —
 * the largest lateral offset physically reachable within the sensor's
 * rated FOV+range envelope (LD2450_MAX_RANGE_MM * sin(LD2450_FOV_DEG)).
 * x=0 maps to the strip's center (pixel (N-1)/2).
 *
 * Brightness is normalized against LED_BRIGHTNESS_RANGE_MM, a *practical*
 * range distinct from the sensor's rated LD2450_MAX_RANGE_MM (6m) — chosen
 * so that normal indoor test/use distances produce a visible swing instead
 * of sitting in the flat part of the curve. See conversation history for
 * the CIE-L*-based derivation of this value.
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

#define TAG "led_fx"
#define RENDER_HZ 30
#define BLOB_HALF_W 1.2f      /* pixels either side of center that get some brightness */
#define MIN_BRIGHTNESS 8      /* floor so a target at max range isn't fully invisible */
#define BRIGHTNESS_GAMMA 2.4f /* perceptual correction — see render_tick() comment */

/* Max lateral offset (mm) physically reachable within the sensor's rated
 * envelope: LD2450_MAX_RANGE_MM * sin(LD2450_FOV_DEG) = 6000 * sin(60°). */
#define LED_POS_MAX_X_MM 5196.0f

/* Practical brightness-normalization range (mm) — deliberately smaller than
 * LD2450_MAX_RANGE_MM (6000mm) so normal indoor test/use distances land in
 * the part of the curve that actually shows a visible swing. */
#define LED_BRIGHTNESS_RANGE_MM 4500.0f

typedef struct
{
    uint8_t r, g, b;
} rgb_t;

static const rgb_t s_slot_color[3] = {
    {255, 50, 50}, /* slot 0 — red */
    {50, 255, 50}, /* slot 1 — green */
    {50, 50, 255}, /* slot 2 — blue */
};

static led_strip_handle_t s_strip;
static bool s_ready;

/* Per-slot EMA so noisy frame-to-frame radar jitter doesn't flicker the
 * strip; x-offset/distance are smoothed independently per slot. */
static float s_x_ema[3];
static float s_dist_ema[3];
static bool s_ema_init[3];

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

    for (int i = 0; i < 3; i++)
    {
        if (!t[i].present)
        {
            s_ema_init[i] = false;
            continue;
        }

        float xf = (float)t[i].x_mm;
        float dist = sqrtf((float)t[i].x_mm * t[i].x_mm + (float)t[i].y_mm * t[i].y_mm);

        if (!s_ema_init[i])
        {
            s_x_ema[i] = xf;
            s_dist_ema[i] = dist;
            s_ema_init[i] = true;
        }
        else
        {
            s_x_ema[i] = s_x_ema[i] * 0.7f + xf * 0.3f;
            s_dist_ema[i] = s_dist_ema[i] * 0.7f + dist * 0.3f;
        }

        /* x=0 -> strip center; x=+-LED_POS_MAX_X_MM -> the two strip ends. */
        float x = clampf(s_x_ema[i], -LED_POS_MAX_X_MM, LED_POS_MAX_X_MM);
        float pos = (x + LED_POS_MAX_X_MM) / (2.0f * LED_POS_MAX_X_MM) * (LED_STRIP_NUM_LEDS - 1);

        /* WS2812 PWM duty isn't perceived linearly by the eye (Stevens' power
         * law) — most values above ~80/255 all look "full brightness," so a
         * linear distance->PWM map only looks dimmer right at the sensor's
         * rated max range. Gamma-correct so the falloff is visible across a
         * normal room-scale walk instead of only at the very edge. */
        float d = clampf(s_dist_ema[i], 0.0f, LED_BRIGHTNESS_RANGE_MM);
        float brightness = powf(1.0f - (d / LED_BRIGHTNESS_RANGE_MM), BRIGHTNESS_GAMMA);
        uint8_t level = (uint8_t)clampf(brightness * 255.0f, MIN_BRIGHTNESS, 255.0f);

        if (dbg_log)
        {
            ESP_LOGI(TAG, "slot%d raw(x=%d,y=%d) dist_raw=%.0fmm dist_ema=%.0fmm brightness=%.3f level=%u",
                     i, t[i].x_mm, t[i].y_mm, dist, s_dist_ema[i], brightness, level);
        }

        int lo = (int)floorf(pos - BLOB_HALF_W);
        int hi = (int)ceilf(pos + BLOB_HALF_W);
        if (lo < 0)
            lo = 0;
        if (hi > LED_STRIP_NUM_LEDS - 1)
            hi = LED_STRIP_NUM_LEDS - 1;

        for (int p = lo; p <= hi; p++)
        {
            float falloff = clampf(1.0f - fabsf((float)p - pos) / BLOB_HALF_W, 0.0f, 1.0f);
            uint8_t s = (uint8_t)(level * falloff);
            fb[p][0] += (uint32_t)s_slot_color[i].r * s / 255;
            fb[p][1] += (uint32_t)s_slot_color[i].g * s / 255;
            fb[p][2] += (uint32_t)s_slot_color[i].b * s / 255;
        }
    }

    for (int p = 0; p < LED_STRIP_NUM_LEDS; p++)
    {
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
    while (1)
    {
        render_tick();
        vTaskDelay(pdMS_TO_TICKS(1000 / RENDER_HZ));
    }
}

void led_effect_init(void)
{
    if (LED_STRIP_GPIO < 0)
    {
        ESP_LOGW(TAG, "LED_STRIP_GPIO not configured — LED effect disabled");
        return;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_NUM_LEDS,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {.invert_out = false},
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = {.with_dma = false},
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
        return;
    }
    led_strip_clear(s_strip);
    s_ready = true;
}

void led_effect_start(void)
{
    if (!s_ready)
        return;
    xTaskCreate(led_effect_task, "led_fx", 4096, NULL, 3, NULL);
}
