#pragma once
#include <stdbool.h>

/*
 * FOTA — firmware-over-the-air update via HTTP(S).
 *
 * Server setup (host these two files on any HTTP/HTTPS server):
 *
 *   version.json
 *   ------------
 *   {
 *     "version": "1.0.1",
 *     "url":     "http://your-server.com/firmware/Test.bin"
 *   }
 *
 *   Test.bin  (copy from build/Test.bin after idf.py build)
 *
 * Then set FOTA_VERSION_URL below to the URL of version.json.
 */
#define FOTA_VERSION_URL  "https://raw.githubusercontent.com/akshathaaa-ctrl/intellicar-firmware/main/version.json"

typedef enum {
    FOTA_IDLE = 0,
    FOTA_CHECKING,
    FOTA_DOWNLOADING,
    FOTA_VERIFYING,
    FOTA_DONE,
    FOTA_UP_TO_DATE,
    FOTA_NO_WIFI,
    FOTA_ERROR,
} fota_state_t;

/* cb is called from the FOTA task — acquire LVGL lock before touching widgets */
typedef void (*fota_cb_t)(fota_state_t state, int progress_pct, const char *msg);

/* Start background check + update task. No-op if already running. */
void fota_start(fota_cb_t cb);

fota_state_t fota_get_state(void);
const char  *fota_current_version(void);
