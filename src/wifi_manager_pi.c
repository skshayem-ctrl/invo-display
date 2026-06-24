#include "wifi_manager.h"
#include <string.h>

/* Pi: WiFi is managed by the OS. We report always-connected so the
 * clock uses system time (already NTP-synced by systemd-timesyncd)
 * and the WiFi icon shows green. */

void       wifi_manager_init(void)              {}
bool       wifi_manager_connected(void)         { return true; }
bool       wifi_manager_time_synced(void)        { return true; }

void       wifi_manager_scan_start(void)         {}
bool       wifi_manager_scan_done(void)          { return false; }
int        wifi_manager_ap_count(void)           { return 0; }
bool       wifi_manager_ap_info(int idx, char ssid[33], int8_t *rssi, bool *secured)
{
    (void)idx; (void)ssid; (void)rssi; (void)secured;
    return false;
}
void       wifi_manager_connect_to(const char *ssid, const char *pass)
{
    (void)ssid; (void)pass;
}
wm_state_t wifi_manager_conn_state(void) { return WM_CONNECTED; }
