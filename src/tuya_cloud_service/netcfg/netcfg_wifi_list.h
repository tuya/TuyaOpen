/**
 * @file netcfg_wifi_list.h
 * @brief Shared WiFi AP scanning and JSON list serialization for AP and BLE netcfg.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __NETCFG_WIFI_LIST_H__
#define __NETCFG_WIFI_LIST_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Scan nearby WiFi APs, sort by descending RSSI, deduplicate by SSID,
 *        and serialize into {"wifi_list":[{"ssid":...,"rssi":...,"sec":...}]}.
 *
 * @param[out] json    Destination buffer for the NUL-terminated JSON string.
 * @param[in]  size    Size of @p json in bytes.
 * @param[in]  max_cnt Maximum number of AP entries to include.
 * @return Length of the serialized JSON string in bytes (excluding trailing NUL),
 *         or 0 if @p json is invalid / too small.
 */
uint32_t netcfg_wifi_list_build(char *json, uint32_t size, uint32_t max_cnt);

#ifdef __cplusplus
}
#endif

#endif /* __NETCFG_WIFI_LIST_H__ */
