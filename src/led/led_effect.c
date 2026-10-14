/*
 * led_effect.c — status strip on a WS2812B strip, driven from the shared gd
 * data plus the FOTA state. The first matching row wins:
 *
 *   inverter fault        red, fast blink
 *   overload (> 3 kW)     red, breathing
 *   firmware update       purple bar = download %, purple scanner while waiting
 *   grid lost / restored  amber / green blink for 3 s (update failed: red, 3 s)
 *   no BMS data           purple scanner (covers boot, until the BMS first reports)
 *   critical low          solid red (latched, see CRITICAL_HYST_PCT)
 *   charging              green pulses moving toward higher pixel numbers
 *   discharging           blue pulses moving toward lower pixel numbers
 *   idle                  solid white
 *
 * Pulse speed follows |battery current|. An inverter in bypass puts amber caps
 * on both ends of the strip over any of the battery rows.
 *
 * Battery inputs match the UI's battery mode label (data_tick_cb in
 * ui_common.c), except that solar charging (batt_a > 0 with ac_chg == 0) counts
 * as charging here (the label reads "Idle" for it).
 *
 * gd keeps its last value when a poll fails, so a dead BMS or inverter link
 * leaves the strip on the last known state, same as the on-screen values.
 */
#include "led_effect.h"
#include "hw_config.h"
#include "ui_common.h"
#include "modbus_inverter.h"
#include "fota.h"
#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#define TAG "led_fx"
#define LED_TICK_MS 33 /* ~30 fps, fast enough for the animations */

/* The strip is lit across its whole length, so supply current matters: white
 * at 255 is ~60 mA per LED (~4.8 A for 80) and scales linearly with this.
 * Raise it only if the strip's supply can take it. */
#define LED_BRIGHTNESS 64

/* Same red band as the UI's SOC arc (ui_common.c: below 20% is red). */
#define CRITICAL_SOC_PCT 20
#define CRITICAL_HYST_PCT 1 /* stay red until SOC reaches CRITICAL_SOC_PCT + this */

/* Same +-0.5 A deadband the UI uses to tell discharging from idle. */
#define BATT_CURRENT_DEADBAND_A 0.5f

#define OVERLOAD_KW 3.0f     /* the 3 kW nominal the UI's load bar and alerts use */
#define GRID_PRESENT_V 50.0f /* same threshold as the UI's "AC input" label */
#define EVENT_MS 3000        /* how long a grid / update-failed blink lasts */
#define BYPASS_CAP_PX 4

#define FLOW_WAVELENGTH_PX 16.0f /* distance between pulse heads */
#define FLOW_MIN_PPS 3.0f        /* pulse speed at ~0 A, pixels per second */
#define FLOW_MAX_PPS 30.0f       /* ... and at FLOW_FULL_A or more */
#define FLOW_FULL_A 40.0f
#define FLOW_FLOOR 0.15f /* brightness between pulses, so the strip never goes dark */
#define SCAN_PERIOD_MS 1600
#define SCAN_TAIL_PX 4.0f

typedef enum
{
    BATT_NO_DATA,
    BATT_IDLE,
    BATT_CHARGING,
    BATT_DISCHARGING,
    BATT_CRITICAL,
} batt_state_t;

typedef struct
{
    uint8_t r, g, b;
} rgb_t;

static const rgb_t k_red = {255, 0, 0}, k_green = {0, 255, 0}, k_blue = {0, 0, 255},
                   k_white = {255, 255, 255}, k_amber = {255, 100, 0}, k_purple = {160, 0, 255};

static const char *const s_state_name[] = {
    [BATT_NO_DATA] = "waiting for BMS",
    [BATT_IDLE] = "idle",
    [BATT_CHARGING] = "charging",
    [BATT_DISCHARGING] = "discharging",
    [BATT_CRITICAL] = "critical low",
};

/* Everything render_frame() looks at, read once per tick. */
typedef struct
{
    int bms_valid, pct, ac_chg, fault, bypassing, inv_valid, fota_state, fota_pct;
    float batt_a, load_kw, grid_v;
} snap_t;

static led_strip_handle_t s_strip;
static bool s_ready;
static bool s_critical; /* latched critical-low flag, see classify() */
static float s_phase;   /* flow pulse head position, pixels, wraps at FLOW_WAVELENGTH_PX */
static struct
{
    rgb_t color;
    const char *name;
    uint32_t t0;
    bool on;
} s_event;

static float clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/* Triangle wave 0..1..0 over `period` ms. */
static float tri(uint32_t t, uint32_t period)
{
    float ph = (t % period) / (float)period;
    return ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f;
}

static float blink(uint32_t t, uint32_t period)
{
    return (t % period) < period / 2 ? 1.0f : 0.0f;
}

/* Set one pixel to colour c at level 0..1 of LED_BRIGHTNESS. */
static void put(int i, rgb_t c, float level)
{
    float s = level * LED_BRIGHTNESS / 255.0f;
    led_strip_set_pixel(s_strip, i, (uint32_t)(c.r * s + 0.5f), (uint32_t)(c.g * s + 0.5f),
                        (uint32_t)(c.b * s + 0.5f));
}

static void solid(rgb_t c, float level)
{
    for (int i = 0; i < LED_STRIP_NUM_LEDS; i++)
        put(i, c, level);
}

/* Sawtooth pulses with a bright head and a fading tail behind it. dir > 0
 * moves them toward higher pixel numbers, dir < 0 toward lower. */
static void flow(rgb_t c, int dir, float batt_a, float dt)
{
    float f = clamp01(fabsf(batt_a) / FLOW_FULL_A);
    float pps = FLOW_MIN_PPS + (FLOW_MAX_PPS - FLOW_MIN_PPS) * f;
    s_phase = fmodf(s_phase + pps * dt, FLOW_WAVELENGTH_PX);

    for (int i = 0; i < LED_STRIP_NUM_LEDS; i++)
    {
        float x = dir > 0 ? i : LED_STRIP_NUM_LEDS - 1 - i;
        float d = fmodf(s_phase - x, FLOW_WAVELENGTH_PX); /* distance behind the head */
        if (d < 0.0f)
            d += FLOW_WAVELENGTH_PX;
        float k = 1.0f - d / FLOW_WAVELENGTH_PX;
        put(i, c, FLOW_FLOOR + (1.0f - FLOW_FLOOR) * k * k);
    }
}

/* A short comet bouncing end to end. */
static void scanner(rgb_t c, uint32_t now)
{
    float pos = tri(now, SCAN_PERIOD_MS) * (LED_STRIP_NUM_LEDS - 1);
    for (int i = 0; i < LED_STRIP_NUM_LEDS; i++)
    {
        float k = clamp01(1.0f - fabsf(i - pos) / SCAN_TAIL_PX);
        put(i, c, k * k);
    }
}

/* Progress bar: full pixels, one partial pixel, the rest off. */
static void bar(rgb_t c, int pct)
{
    float lit = clamp01(pct / 100.0f) * LED_STRIP_NUM_LEDS;
    for (int i = 0; i < LED_STRIP_NUM_LEDS; i++)
        put(i, c, clamp01(lit - i));
}

/* Stateful (s_critical): call from the LED task only. */
static batt_state_t classify(int bms_valid, int pct, int ac_chg, float batt_a)
{
    if (!bms_valid)
        return BATT_NO_DATA;

    if (pct < CRITICAL_SOC_PCT)
        s_critical = true;
    else if (pct >= CRITICAL_SOC_PCT + CRITICAL_HYST_PCT)
        s_critical = false;
    if (s_critical)
        return BATT_CRITICAL;

    if (ac_chg || batt_a > BATT_CURRENT_DEADBAND_A)
        return BATT_CHARGING;
    if (batt_a < -BATT_CURRENT_DEADBAND_A)
        return BATT_DISCHARGING;
    return BATT_IDLE;
}

/* Plain reads, no lvgl_acquire(): each field is one aligned word, a snapshot
 * torn across fields is harmless here, and the LVGL lock is held while LVGL
 * renders or builds screens, which would freeze the animation. */
static void snapshot(snap_t *s)
{
    s->bms_valid = gd.bms_valid;
    s->pct = gd.batt_pct;
    s->ac_chg = gd.ac_chg;
    s->fault = gd.fault;
    s->bypassing = gd.bypassing;
    s->batt_a = gd.batt_a;
    s->load_kw = gd.load_kw;
    s->grid_v = gd.grid_v;
    s->inv_valid = modbus_inverter_valid();
    s->fota_state = fota_get_state();
    s->fota_pct = fota_get_progress();
}

static void trigger(rgb_t c, const char *name, uint32_t now)
{
    s_event.color = c;
    s_event.name = name;
    s_event.t0 = now;
    s_event.on = true;
}

/* Turn changes in grid presence and a failed update into timed blink events. */
static void track_events(const snap_t *s, uint32_t now)
{
    static bool grid_known, grid_prev;
    static int fota_prev = FOTA_IDLE;

    /* Only compare while the inverter link is good, so boot and link drops
     * don't look like the grid coming or going. */
    if (s->inv_valid)
    {
        bool grid = s->grid_v > GRID_PRESENT_V;
        if (grid_known && grid != grid_prev)
            trigger(grid ? k_green : k_amber, grid ? "grid restored" : "grid lost", now);
        grid_prev = grid;
        grid_known = true;
    }

    if (s->fota_state != fota_prev && (s->fota_state == FOTA_ERROR || s->fota_state == FOTA_NO_WIFI))
        trigger(k_red, "update failed", now);
    fota_prev = s->fota_state;
}

/* Draw one frame into the strip buffer; returns a label for the log. */
static const char *render_frame(const snap_t *s, uint32_t now, float dt)
{
    if (s->fault)
    {
        solid(k_red, blink(now, 250));
        return "inverter fault";
    }
    if (s->load_kw > OVERLOAD_KW)
    {
        float k = tri(now, 1000);
        solid(k_red, k * k);
        return "overload";
    }

    int fs = s->fota_state;
    if (fs == FOTA_CHECKING || fs == FOTA_DOWNLOADING || fs == FOTA_VERIFYING || fs == FOTA_DONE)
    {
        if (s->fota_pct > 0)
            bar(k_purple, s->fota_pct);
        else
            scanner(k_purple, now);
        return "firmware update";
    }

    if (s_event.on && now - s_event.t0 < EVENT_MS)
    {
        solid(s_event.color, blink(now - s_event.t0, 500));
        return s_event.name;
    }

    batt_state_t st = classify(s->bms_valid, s->pct, s->ac_chg, s->batt_a);
    switch (st)
    {
    case BATT_NO_DATA:
        scanner(k_purple, now);
        break;
    case BATT_CHARGING:
        flow(k_green, +1, s->batt_a, dt);
        break;
    case BATT_DISCHARGING:
        flow(k_blue, -1, s->batt_a, dt);
        break;
    case BATT_CRITICAL:
        solid(k_red, 1.0f);
        break;
    case BATT_IDLE:
        solid(k_white, 1.0f);
        break;
    }

    if (s->bypassing)
    {
        for (int i = 0; i < BYPASS_CAP_PX; i++)
        {
            put(i, k_amber, 1.0f);
            put(LED_STRIP_NUM_LEDS - 1 - i, k_amber, 1.0f);
        }
    }
    return s_state_name[st];
}

static void led_effect_task(void *arg)
{
    ESP_LOGI(TAG, "LED effect task started (%d LEDs, GPIO%d)",
             LED_STRIP_NUM_LEDS, LED_STRIP_GPIO);

    TickType_t wake = xTaskGetTickCount();
    uint32_t last = wake * portTICK_PERIOD_MS;
    const char *last_what = NULL;
    while (1)
    {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        float dt = (now - last) / 1000.0f;
        last = now;
        if (dt > 0.1f)
            dt = 0.1f;

        snap_t s;
        snapshot(&s);
        track_events(&s, now);
        const char *what = render_frame(&s, now, dt);
        if (what != last_what)
        {
            ESP_LOGI(TAG, "LED -> %s (soc=%d%% a=%.1fA load=%.1fkW grid=%.0fV fault=%d bypass=%d)",
                     what, s.pct, s.batt_a, s.load_kw, s.grid_v, s.fault, s.bypassing);
            last_what = what;
        }

        /* Re-sent every tick, so a glitched frame can't stick. */
        led_strip_refresh(s_strip);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(LED_TICK_MS));
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
