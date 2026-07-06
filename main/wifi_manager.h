#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { WM_IDLE=0, WM_CONNECTING, WM_CONNECTED, WM_FAILED } wm_state_t;

void       wifi_manager_init(void);
bool       wifi_manager_connected(void);
bool       wifi_manager_time_synced(void);

void       wifi_manager_scan_start(void);
bool       wifi_manager_scan_done(void);
int        wifi_manager_ap_count(void);
bool       wifi_manager_ap_info(int idx, char ssid[33], int8_t *rssi, bool *secured);

void       wifi_manager_connect_to(const char *ssid, const char *pass);
wm_state_t wifi_manager_conn_state(void);
