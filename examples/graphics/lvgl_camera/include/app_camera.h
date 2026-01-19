/**
 * @file app_camera.h
 * @brief app_camera module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_CAMERA_H__
#define __APP_CAMERA_H__

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

OPERATE_RET app_camera_init(void);

OPERATE_RET app_camera_deinit(void);

OPERATE_RET app_camera_display_start(void);

OPERATE_RET app_camera_display_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CAMERA_H__ */
