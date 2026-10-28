/**
 * @file ble_netcfg.h
 * @brief Header file for BLE Network Configuration.
 * This file provides the interface for initializing the BLE network
 * configuration module. It includes the declaration of the function to register
 * the network configuration policy to the network configuration module.
 *
 * The BLE network configuration process is essential for setting up devices to
 * connect to the network via BLE. This setup includes registering the device
 * with the Tuya cloud services and configuring network parameters such as SSID
 * and password.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef _BLE_NETCFG_H_
#define _BLE_NETCFG_H_

#include "netcfg.h"
#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief register netcfg to netcfg module
 *
 * @param[in] netcfg_policy type
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int ble_netcfg_init(netcfg_args_t *netcfg_args);

/**
 * @brief Reply the network configuration stage to the app.
 */
void ble_netcfg_stat_query(void *data, uint32_t len, void *user_data);

#ifdef __cplusplus
}
#endif
#endif
