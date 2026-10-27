/**
 * @file ap_netcfg.h
 * @brief Header file for AP (Access Point) network configuration
 * functionalities.
 *
 * This header file provides the declarations for the AP network configuration
 * process, including the initialization function that registers the network
 * configuration policy to the network configuration module. It is designed to
 * facilitate the setup of devices connecting to a local network via an Access
 * Point, ensuring a seamless and secure network configuration process.
 *
 * The file defines the interface for initializing the AP network configuration,
 * which is a crucial part of the device setup process in the Tuya IoT SDK
 * ecosystem.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef _AP_NETCFG_H_
#define _AP_NETCFG_H_

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
int ap_netcfg_init(netcfg_args_t *netcfg);

/**
 * @brief get the SoftAP hotspot info used while AP netcfg is running
 *
 * @param[in/out] ssid buffer receiving the hotspot ssid ("SmartLife-XXXX")
 * @param[in] ssid_len size of the ssid buffer
 * @param[in/out] ip buffer receiving the hotspot gateway address
 * @param[in] ip_len size of the ip buffer
 *
 * @return OPRT_OK when the hotspot is up and the info was copied. Others when
 * AP netcfg has not started yet (buffers left as empty strings).
 */
OPERATE_RET ap_netcfg_get_hotspot_info(char *ssid, int ssid_len, char *ip, int ip_len);

#ifdef __cplusplus
}
#endif
#endif
