#pragma once

/* Create the WS2812B RMT device. Call once at boot, after hw_config.h's
 * LED_STRIP_GPIO has been set to a real, confirmed-free GPIO. If it's still
 * the unconfigured placeholder (-1), this logs a warning and the effect
 * stays disabled rather than failing boot. */
void led_effect_init(void);

/* Start the render task that maps live LD2450 targets onto the strip.
 * No-op if led_effect_init() didn't successfully create the strip. */
void led_effect_start(void);
