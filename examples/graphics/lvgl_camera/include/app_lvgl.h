/**
 * @file app_lvgl.h
 * @brief app_lvgl module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_LVGL_H__
#define __APP_LVGL_H__

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

void app_lvgl_init(void);

void app_lvgl_deinit(void);

OPERATE_RET app_lvgl_display_start(void);

OPERATE_RET app_lvgl_display_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_LVGL_H__ */
