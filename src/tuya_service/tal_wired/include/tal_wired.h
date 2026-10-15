/**
 * @file tal_wired.h
 * @brief Provides the interface for managing wired network connections,
 * including status checks and event callbacks.
 *
 * This header file defines the API for interacting with wired network
 * interfaces in Tuya IoT applications. It includes functions for checking the
 * current status of a wired connection and setting callbacks for status
 * changes. The API abstracts the underlying hardware specifics, offering a
 * unified interface for wired network operations across different platforms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_WIRED_H__
#define __TAL_WIRED_H__

#include "tuya_cloud_types.h"
#include "tkl_wired.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIRED_STAT_E               TKL_WIRED_STAT_E
#define TAL_WIRED_STATUS_CHANGE_CB TKL_WIRED_STATUS_CHANGE_CB

/**
 * @brief  get the link status of wired link
 *
 * @param[out]  is_up: the wired link status is up or not
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wired_get_status(WIRED_STAT_E *stat);

/**
 * @brief  set the status change callback
 *
 * @param[in]   cb: the callback when link status changed
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wired_set_status_cb(TAL_WIRED_STATUS_CHANGE_CB cb);

/**
 * @brief  set the ip address of the wired link
 *
 * @param[in]   ip: the ip address
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wired_set_ip(NW_IP_S *ip);

/**
 * @brief  get the ip address of the wired link
 *
 * @param[in]   ip: the ip address
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wired_get_ip(NW_IP_S *ip);

/**
 * @brief  get the mac address of the wired link
 *
 * @param[in]   mac: the mac address
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wired_get_mac(NW_MAC_S *mac);

/**
 * @brief  set the mac address of the wired link
 *
 * @param[in]   mac: the mac address
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wired_set_mac(const NW_MAC_S *mac);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TAL_WIRED_H__
