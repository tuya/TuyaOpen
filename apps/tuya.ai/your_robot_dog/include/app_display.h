/**
 * @file app_display.h
 * @brief Header file for Tuya Display System
 *
 * This header file provides the declarations for initializing the display system
 * and sending messages to the display. It includes the necessary data types and
 * function prototypes for interacting with the display functionality.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __APP_DISPLAY_YOUR_ROBOT_DOG_H__
#define __APP_DISPLAY_YOUR_ROBOT_DOG_H__

#include <stdbool.h>

#include "tuya_cloud_types.h"

#include "lang_config.h"

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
 * @brief Initialize the display system
 *
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */

/**
 * @brief Send display message to the display system
 *
 * @param tp Type of the display message
 * @param data Pointer to the message data
 * @param len Length of the message data
 * @return OPERATE_RET Result of sending the message, OPRT_OK indicates success
 */

/* Direct UI update APIs (no message sending) */
void robot_ui_battery_update(bool is_charging, uint8_t percentage);
void robot_ui_enter_netcfg(void);
void robot_ui_wifi_update(uint8_t wifi_status);

#ifdef __cplusplus
}
#endif

#endif /* __APP_DISPLAY_YOUR_ROBOT_DOG_H__ */
