/**
 * @file tuya_config.h
 * @brief Tuya IoT device configuration file for product and network settings.
 *
 * This file contains the configuration settings for Tuya IoT devices, including
 * product identification information, device authentication credentials, and
 * network provisioning settings. It provides the necessary configuration
 * parameters for device activation, cloud platform communication, and
 * network connectivity setup.
 *
 * The configuration includes product ID, device UUID, authentication key,
 * and optional PINCODE for AP provisioning. These parameters are essential
 * for device registration with the Tuya cloud platform and establishing
 * secure communication channels.
 *
 * Users must replace the default placeholder values with their actual
 * product information obtained from the Tuya IoT platform to ensure
 * proper device functionality and cloud connectivity.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef _TUYA_CONFIG_H_
#define _TUYA_CONFIG_H_

/**
 * @brief Product and device configuration settings
 *
 * This section defines the essential product and device identification
 * parameters required for Tuya IoT device operation. These values must
 * be obtained from the Tuya IoT platform and configured correctly
 * for proper device activation and cloud communication.
 *
 * TUYA_PRODUCT_ID: Product ID (PID) created on the Tuya IoT platform
 * TUYA_OPENSDK_UUID: Device UUID for SDK authentication
 * TUYA_OPENSDK_AUTHKEY: Authentication key for secure communication
 *
 * For detailed setup instructions, please refer to:
 * 1. Product creation and PID generation:
 *    https://developer.tuya.com/cn/docs/iot-device-dev/application-creation?id=Kbxw7ket3aujc
 * 2. SDK license acquisition:
 *    https://platform.tuya.com/purchase/index?type=6
 *
 * WARNING: Replace these placeholder values with your actual product
 * information to ensure proper device functionality.
 *
 */
// clang-format off
#define TUYA_PRODUCT_ID         "wtewb6gaxuowjzhb"                        // Please change your product id
#define TUYA_OPENSDK_UUID       "uuidxxxxxxxxxxxxxxxx"                    // Please change the correct uuid
#define TUYA_OPENSDK_AUTHKEY    "keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"        // Please change the correct authkey

/**
 * @brief Network provisioning configuration
 *
 * This section defines the PINCODE configuration for AP (Access Point)
 * provisioning mode. The PINCODE is a randomly generated code provided
 * by the Tuya PMS (Product Management System) for secure device
 * network configuration.
 *
 * TUYA_NETCFG_PINCODE: Random PINCODE for AP provisioning mode
 *
 * WARNING: PINCODE is mandatory for AP provisioning mode and must
 * be obtained from the Tuya PMS system for proper network setup.
 *
 */
// #define TUYA_NETCFG_PINCODE   "69832860"

// clang-format on

#endif //!_TUYA_CONFIG_H_
