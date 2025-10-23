/**
 * @file peripherals_scan.h
 *
 * @brief Declares peripheral scanning functionality for IoT device
 *
 * This header file declares the peripheral scanning functionalities required for an IoT device.
 * It includes declarations for scanning and identifying connected peripherals, managing device
 * connections, and handling communication protocols. The declarations support various peripheral
 * types and ensure seamless integration with the Tuya IoT platform.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __PERIPHERALS_SCAN_H__
#define __PERIPHERALS_SCAN_H__

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************************
 *                             PUBLIC FUNCTIONS                                  *
 *********************************************************************************/

/**
 * @brief Show I2C device scan results
 */
void i2c_scan_show(uint8_t i2c_port);

/**
 * @brief Hide I2C device scan results
 */
void i2c_scan_hidden(void);

bool i2c_scan_is_active(void);

void i2c_scan_handle_input(uint32_t key);

void wifi_scan_show(void);
void wifi_scan_hidden(void);
bool wifi_scan_is_active(void);
void wifi_scan_handle_input(uint32_t key);
#ifdef __cplusplus
}
#endif

#endif /* __PERIPHERALS_SCAN_H__ */