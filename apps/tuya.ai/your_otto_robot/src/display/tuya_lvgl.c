/**
 * @file tuya_lvgl.c
 * @brief Initialize and manage LVGL library, related devices, and threads
 *
 * This source file provides the implementation for initializing the LVGL library,
 * registering LCD devices, setting up display and input devices, creating and
 * managing a mutex for thread synchronization, and starting a task for LVGL
 * operations.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "lv_vendor.h"
#include "tuya_lvgl.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Initialize LVGL library and related devices and threads
 *
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_init(void)
{
    lv_vendor_init(DISPLAY_NAME);

    lv_vendor_start(5, 1024*8);

    return OPRT_OK;
}

/**
 * @brief Lock the LVGL mutex
 *
 * @param None
 * @return OPERATE_RET Lock result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_mutex_lock(void)
{
    lv_vendor_disp_lock();

    return OPRT_OK;
}

/**
 * @brief Unlock the LVGL mutex
 *
 * @param None
 * @return OPERATE_RET Lock result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_mutex_unlock(void)
{
    lv_vendor_disp_unlock();

    return OPRT_OK;
}
