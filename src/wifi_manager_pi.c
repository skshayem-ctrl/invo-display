#include "wifi_manager.h"
#include <string.h>

/* Pi: WiFi managed by the OS — scanning/connecting not applicable.
 * Return scan_done=true immediately so the screen stops spinning. */

static bool s_scan_done = false;

void       wifi_manager_init(void)           {}
bool       wifi_manager_connected(void)      { return true; }
bool       wifi_manager_time_synced(void)    { return true; }
wm_state_t wifi_manager_conn_state(void)     { return WM_CONNECTED; }

void       wifi_manager_scan_start(void)     { s_scan_done = true; }
bool       wifi_manager_scan_done(void)      { return s_scan_done; }
int        wifi_manager_ap_count(void)       { return 0; }

bool wifi_manager_ap_info(int idx, char ssid[33], int8_t *rssi, bool *secured)
{
    (void)idx; (void)ssid; (void)rssi; (void)secured;
    return false;
}

void wifi_manager_connect_to(const char *ssid, const char *pass)
{
    (void)ssid; (void)pass;
}
