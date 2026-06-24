#include "fota.h"
#include "hal/hal.h"
#include <stdio.h>
#include <string.h>

static fota_state_t s_state = FOTA_IDLE;

void fota_start(fota_cb_t cb)
{
    s_state = FOTA_CHECKING;
    hal_fota_trigger();
    s_state = FOTA_DONE;
    if (cb) cb(FOTA_DONE, 100, "Update triggered");
}

fota_state_t fota_get_state(void)
{
    return s_state;
}

const char *fota_current_version(void)
{
    static char ver[32] = "";
    if (ver[0]) return ver;
    FILE *f = fopen("/etc/invo/version", "r");
    if (f) {
        if (fgets(ver, sizeof(ver), f)) {
            int len = (int)strlen(ver);
            if (len > 0 && ver[len - 1] == '\n') ver[len - 1] = '\0';
        }
        fclose(f);
    }
    if (!ver[0]) strncpy(ver, "unknown", sizeof(ver) - 1);
    return ver;
}
