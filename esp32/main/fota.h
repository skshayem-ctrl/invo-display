#pragma once

/* Fixed-name asset uploaded to every GitHub release.
 * /releases/latest/download/ always resolves to the newest release. */
#define FOTA_BIN_URL \
    "https://github.com/skshayem-ctrl/invo-display/releases/latest/download/invo-esp32.bin"

#include "../../src/fota.h"
