#pragma once
#include <stdbool.h>

/* FOTA endpoints — CI writes version.json to both current and legacy esp32/ path */
#define FOTA_VER_URL \
    "https://raw.githubusercontent.com/skshayem-ctrl/invo-display/master/version.json"
#define FOTA_BIN_URL \
    "https://raw.githubusercontent.com/skshayem-ctrl/invo-display/binaries/invo-esp32.bin"

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

typedef void (*fota_cb_t)(fota_state_t state, int progress_pct, const char *msg);

void         fota_start(fota_cb_t cb);
void         fota_cancel(void);
fota_state_t fota_get_state(void);
const char  *fota_current_version(void);
