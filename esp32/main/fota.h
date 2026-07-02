#pragma once

/* Version check: tiny JSON (<100 B), one TCP segment, no cascade risk.
 * CI pushes this to master after every release tag. */
#define FOTA_VER_URL \
    "https://raw.githubusercontent.com/skshayem-ctrl/invo-display/master/esp32/version.json"

/* Binary — same raw.githubusercontent.com CDN as version.json (Fastly, one TLS
 * handshake, no github.com redirect). CI pushes binary to the binaries branch. */
#define FOTA_BIN_URL \
    "https://raw.githubusercontent.com/skshayem-ctrl/invo-display/binaries/invo-esp32.bin"

#include "../../src/fota.h"
