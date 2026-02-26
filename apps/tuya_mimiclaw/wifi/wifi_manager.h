#pragma once

#include "mimi_base.h"

typedef void (*wifi_scan_result_cb_t)(uint32_t index, uint32_t total, const char *ssid, uint8_t channel, int rssi,
                                      uint8_t security, const char *bssid, void *user_data);

OPERATE_RET wifi_manager_init(void);
OPERATE_RET wifi_manager_start(void);
OPERATE_RET wifi_manager_wait_connected(uint32_t timeout_ms);
bool        wifi_manager_is_connected(void);
const char *wifi_manager_get_ip(void);
const char *wifi_manager_get_target_ssid(void);
OPERATE_RET wifi_manager_set_credentials(const char *ssid, const char *password);
void        wifi_manager_set_scan_result_cb(wifi_scan_result_cb_t cb, void *user_data);
OPERATE_RET wifi_manager_scan_and_print(void);
