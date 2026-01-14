/**
 * @file app_system_info.h
 * @brief app_system_info module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_SYSTEM_INFO_H__
#define __APP_SYSTEM_INFO_H__

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
 * @brief Initialize the battery management system
 *
 * This function sets up hardware and starts timers for battery status and charging detection.
 * Must be called once at startup before using other battery APIs.
 *
 * @return OPERATE_RET OPRT_OK on success, error code otherwise
 */
OPERATE_RET app_system_info(void);

/**
 * @brief Get battery status
 * @param percentage Pointer to the variable to store the battery percentage
 * @param is_charging Pointer to the variable to store the charging status
 * @return void
 */
void app_battery_get_status(uint8_t *percentage, bool *is_charging);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SYSTEM_INFO_H__ */
