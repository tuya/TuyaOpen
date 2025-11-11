/**
 * @file rfid_scan.h
 * @brief Implements RFID scanning functionality for IoT devices
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __RFID_SCAN_H__
#define __RFID_SCAN_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include "tuya_cloud_types.h"

/**
 * @brief Initialize RFID scanning functionality
 */
OPERATE_RET rfid_scan_init(void);

/**
 * @brief Start log scanning thread
 * This function starts the UART log scanning thread for AI log screen.
 */
OPERATE_RET rfid_log_scan_start(void);

/**
 * @brief Stop log scanning thread
 * This function stops the UART log scanning thread.
 */
OPERATE_RET rfid_log_scan_stop(void);

#if defined(__cplusplus)
}
#endif

#endif // __RFID_SCAN_H__