# INVO Display — LVGL Embedded UI

## Project
LVGL native UI running on Raspberry Pi 5 + Waveshare 3.4" DSI round display (800x800).
Replaces React/Chromium stack for production-grade embedded product.

## Hardware
- Raspberry Pi 5, Raspberry Pi OS Trixie
- Display: /dev/fb0 (drm-rp1-dsidrmf), 800x800
- Touch: /dev/input/event5 (Goodix Capacitive TouchScreen)
- SSH: intelli@192.168.77.120  (password: 1)

## Build & Run
```bash
cd ~/lv_port_linux/build && make -j4
sudo systemctl restart invo-lvgl
```

## Files
- src/main.c — entry point, calls invo_screens_init()
- src/invo/invo_home.c — home screen with clock, battery arc, solar/load, weather
- src/invo/invo_home.h — declares invo_home_screen_create(lv_obj_t * scr)
- src/invo/invo_screens.c — screen manager, detail screens, navigation, back button
- src/invo/invo_screens.h — shared colors (C_BG, C_GREEN etc), nav function declarations
- /etc/systemd/system/invo-lvgl.service — auto-starts on boot, no display manager

## Current Status
- Home screen working: clock, battery arc (78%), solar/load labels, weather/AQI
- Battery, Solar, Weather detail screens built and navigable via touch
- Back button broken — renders outside the 800x800 circle boundary
  - Current (wrong): LV_ALIGN_BOTTOM_LEFT, 60, -60  (puts it at x=100,y=700 = 424px from center, outside circle)
  - Fix needed: LV_ALIGN_BOTTOM_MID, -200, -90  (puts it at x=200,y=670 = 336px from center, inside circle)
  - File to fix: src/invo/invo_screens.c, function make_back_button()

## Next Steps
1. Fix back button position (CURRENT BLOCKER)
2. Real data via /dev/serial0 in C
3. Auto-sleep on inactivity
4. Production hardening
