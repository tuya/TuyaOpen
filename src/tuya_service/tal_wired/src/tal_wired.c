/**
 * @file tal_wired.c
 * @brief Provides the implementation for managing wired network connections in
 * Tuya IoT applications, including status checks, IP and MAC address
 * management, and event callbacks.
 *
 * This source file implements the functions declared in tal_wired.h for
 * interacting with wired network interfaces. It includes operations such as
 * checking the connection status, retrieving and setting IP and MAC addresses,
 * and registering callbacks for status changes. These functionalities are
 * essential for managing wired network connections in IoT devices, ensuring
 * reliable communication and network integration.
 *
 * The implementation abstracts the underlying hardware specifics, offering a
 * unified API for wired network operations across different platforms supported
 * by Tuya's IoT SDK. This facilitates the development of cross-platform IoT
 * applications with wired network capabilities.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_wired.h"
#include "tkl_init_wired.h"

// the tkl description, must be exist
extern TKL_WIRED_DESC_T c_wired_desc;

/**
 * @brief  get the link status of wired link
 *
 * @param[out]  is_up: the wired link status is up or not
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wired_get_status(WIRED_STAT_E *stat)
{
    return tkl_wired_get_status(stat);
}

/**
 * @brief  set the status change callback
 *
 * @param[in]   cb: the callback when link status changed
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wired_set_status_cb(TAL_WIRED_STATUS_CHANGE_CB cb)
{
    return tkl_wired_set_status_cb(cb);
}

/**
 * @brief  set the ip address of the wired link
 *
 * @param[in]   ip: the ip address
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wired_set_ip(NW_IP_S *ip)
{
    return tkl_wired_set_ip(ip);
}

/**
 * @brief  get the ip address of the wired link
 *
 * @param[in]   ip: the ip address
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wired_get_ip(NW_IP_S *ip)
{
    return tkl_wired_get_ip(ip);
}

/**
 * @brief  get the mac address of the wired link
 *
 * @param[in]   mac: the mac address
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wired_get_mac(NW_MAC_S *mac)
{
    return tkl_wired_get_mac(mac);
}

/**
 * @brief  set the mac address of the wired link
 *
 * @param[in]   mac: the mac address
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wired_set_mac(const NW_MAC_S *mac)
{
    return tkl_wired_set_mac(mac);
}
