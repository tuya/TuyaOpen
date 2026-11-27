/**
 * @file ble_netcfg.h
 * @brief BLE network configuration transport adapter (0x0001 netcfg, 0x0003 WiFi list, 0x0004 netcfg status).
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __BLE_NETCFG_H__
#define __BLE_NETCFG_H__

#include "netcfg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_WIFI_LIST_BUF_SIZE_MIN 512
#define BLE_WIFI_LIST_BUF_SIZE_MAX 4096

/**
 * @brief Register the BLE netcfg provider with the netcfg framework.
 */
int ble_netcfg_init(netcfg_args_t *netcfg_args);

/**
 * @brief Register BLE combos provisioning channels (0x0003 WiFi list, 0x0004 netcfg status)
 *        and hook proactive status reporting.
 */
void ble_netcfg_combos_register(void);

/**
 * @brief Unregister BLE combos provisioning channels and proactive status reporting.
 */
void ble_netcfg_combos_unregister(void);

/**
 * @brief Enable or disable the BLE WiFi hotspot list query capability at runtime.
 */
void ble_wifi_list_enable(bool enable);

/**
 * @brief Check whether the BLE WiFi hotspot list query capability is enabled.
 */
bool ble_wifi_list_is_enable(void);

/**
 * @brief Configure the maximum JSON buffer size for the BLE WiFi hotspot list reply.
 *
 * @param[in] size Buffer size in bytes, within [BLE_WIFI_LIST_BUF_SIZE_MIN, BLE_WIFI_LIST_BUF_SIZE_MAX].
 * @return OPRT_OK on success, OPRT_INVALID_PARM if out of range.
 */
OPERATE_RET ble_wifi_list_buf_size_set(uint32_t size);

/**
 * @brief Get the current maximum JSON buffer size for the BLE WiFi hotspot list reply.
 */
uint32_t ble_wifi_list_buf_size_get(void);

/**
 * @brief Report the current provisioning stage/status over BLE (subcmd 0x0004).
 *
 * @param[in] type 0 for proactive report, 1 for query response.
 * @return OPRT_OK on success.
 */
OPERATE_RET ble_netcfg_stat_report(uint32_t type);

#ifdef __cplusplus
}
#endif

#endif /* __BLE_NETCFG_H__ */
