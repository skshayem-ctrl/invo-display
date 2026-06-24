#pragma once

/* Cross-platform LVGL mutex — call before/after touching LVGL from a
 * background thread. On Pi with single-threaded UI these are no-ops. */
void lvgl_acquire(void);
void lvgl_release(void);
