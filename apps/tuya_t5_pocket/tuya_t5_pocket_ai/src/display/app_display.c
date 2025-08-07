/**
 * @file app_display.c
 * @brief Handle display initialization and message processing
 *
 * This source file provides the implementation for initializing the display system,
 * creating a message queue, and handling display messages in a separate task.
 * It includes functions to initialize the display, send messages to the display,
 * and manage the display task.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_memory.h"

#include "app_display.h"

#include "lvgl.h"
#include "lv_vendor.h"

#include "ai_pocket_pet_app.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
extern void pocket_pet_button_init(void);

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Initialize the display system
 *
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET app_display_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    lv_vendor_init(DISPLAY_NAME);

    lv_demo_ai_pocket_pet();

    pocket_pet_button_init();

    lv_vendor_start();

    PR_DEBUG("app_display_init success");

    return rt;
}
