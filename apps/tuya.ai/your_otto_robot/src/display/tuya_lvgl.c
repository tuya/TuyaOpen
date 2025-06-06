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

#if defined(PLATFORM_T5)

#include "tkl_display.h"
#include "tal_api.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "tuya_lcd_device.h"
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
static MUTEX_HANDLE sg_lvgl_mutex_hdl = NULL;
static THREAD_HANDLE sg_lvgl_thrd_hdl = NULL;

static TKL_DISP_DEVICE_S sg_display_device = {
    .device_id = 0,
    .device_port = TKL_DISP_LCD,
    .device_info = NULL,
};

/***********************************************************
***********************function define**********************
***********************************************************/
static uint32_t lv_tick_get_cb(void)
{
    return (uint32_t)tal_system_get_millisecond();
}

static void __lvgl_task(void *args)
{
    uint32_t sleep_time = 0;

    while (1) {
        tal_mutex_lock(sg_lvgl_mutex_hdl);

        sleep_time = lv_timer_handler();

        tal_mutex_unlock(sg_lvgl_mutex_hdl);

        if (sleep_time > 500) {
            sleep_time = 500;
        } else if (sleep_time < 4) {
            sleep_time = 4;
        }

        tal_system_sleep(sleep_time);
    }
}

/**
 * @brief Initialize LVGL library and related devices and threads
 *
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    // register lcd device
    TUYA_CALL_ERR_RETURN(tuya_lcd_device_register(sg_display_device.device_id));

    /*lvgl init*/
    lv_init();
    lv_tick_set_cb(lv_tick_get_cb);
    lv_port_disp_init(&sg_display_device);
#ifdef LVGL_ENABLE_TOUCH
    lv_port_indev_init();
#endif

    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_lvgl_mutex_hdl));

    THREAD_CFG_T cfg = {
        .thrdname = "lvgl",
        .priority = THREAD_PRIO_1,
        .stackDepth = 1024 * 4,
    };

    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_lvgl_thrd_hdl, NULL, NULL, __lvgl_task, NULL, &cfg));

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
    if (NULL == sg_lvgl_mutex_hdl) {
        return OPRT_INVALID_PARM;
    }

    return tal_mutex_lock(sg_lvgl_mutex_hdl);
}

/**
 * @brief Unlock the LVGL mutex
 *
 * @param None
 * @return OPERATE_RET Lock result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_mutex_unlock(void)
{
    if (NULL == sg_lvgl_mutex_hdl) {
        return OPRT_INVALID_PARM;
    }

    return tal_mutex_unlock(sg_lvgl_mutex_hdl);
}

#endif // PLATFORM_T5