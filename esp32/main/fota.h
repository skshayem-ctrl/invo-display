#pragma once

/* Version check: tiny JSON (<100 B), one TCP segment, no cascade risk.
 * CI pushes this to master after every release tag. */
#define FOTA_VER_URL \
    "https://raw.githubusercontent.com/skshayem-ctrl/invo-display/master/esp32/version.json"

/* Binary download — only fetched when version check confirms an update. */
#define FOTA_BIN_URL \
    "https://github.com/skshayem-ctrl/invo-display/releases/latest/download/invo-esp32.bin"

#include "../../src/fota.h"
