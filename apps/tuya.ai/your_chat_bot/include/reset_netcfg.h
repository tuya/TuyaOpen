/**
 * @file reset_netcfg.c
 * @brief Implements reset network configuration functionality for IoT devices
 *
 * This source file provides the implementation of the reset network configuration
 * functionality required for IoT devices. It includes functionality for managing
 * reset counters, handling reset events, and clearing network configurations.
 * The implementation supports integration with the Tuya IoT platform and ensures
 * proper handling of reset-related operations. This file is essential for developers
 * working on IoT applications that require robust network configuration reset mechanisms.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __RESET_NETCFG_H__
#define __RESET_NETCFG_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Starts the network configuration reset process.
 *
 * This function initiates the process to reset the network configuration
 * of the device. It is typically used to clear existing network settings
 * and prepare the device for reconfiguration.
 *
 * @return int Returns 0 on success, or a negative value on failure.
 */
int reset_netconfig_start(void);

/**
 * @brief Checks the status of the network configuration reset process.
 *
 * This function verifies whether the network configuration reset process
 * has been completed successfully or is still in progress.
 *
 * @return int Returns 0 on success, or a negative value on failure.
 */
int reset_netconfig_check(void);

#ifdef __cplusplus
}
#endif

#endif /* __RESET_NETCFG_H__ */
