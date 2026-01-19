/**
 * @file tuya_lvgl.h
 * @brief Header file for LVGL initialization and mutex management
 *
 * This header file provides the declarations for initializing the LVGL library
 * and managing its related devices and threads. It includes functions to
 * initialize LVGL, lock and unlock the LVGL mutex.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_LVGL_H__
#define __TUYA_LVGL_H__

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
 * @brief Initialize LVGL library and related devices and threads
 * 
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_init(void);

/**
 * @brief Lock the LVGL mutex
 * 
 * @param None
 * @return OPERATE_RET Lock result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_mutex_lock(void);

/**
 * @brief Unlock the LVGL mutex
 * 
 * @param None
 * @return OPERATE_RET Lock result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_mutex_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_LVGL_H__ */
