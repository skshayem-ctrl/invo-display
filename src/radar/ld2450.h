#pragma once
#include <stdint.h>
#include <stdbool.h>

/* One tracked person as reported by the LD2450 (3 target slots). */
typedef struct {
    bool    present;
    int16_t x_mm;      /* + = right, - = left, sensor-relative */
    int16_t y_mm;      /* forward distance, mm */
    int16_t speed_cms; /* + = moving away from sensor, - = toward it */
} ld2450_target_t;

/* Install the UART driver for the radar link. Call once at boot. */
void ld2450_init(void);

/* Start the background task that reads and parses radar frames. */
void ld2450_start(void);

/* Copy out the latest known state of all 3 target slots. */
void ld2450_get_targets(ld2450_target_t out[3]);
