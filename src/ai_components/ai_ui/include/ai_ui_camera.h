/**
 * @file ai_ui_camera.h
 * @brief ai_ui_camera module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_UI_CAMERA_H__
#define __AI_UI_CAMERA_H__

#include "tuya_cloud_types.h"
#include "ai_ui_manage.h"

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
OPERATE_RET ai_ui_camera_open(void);

OPERATE_RET ai_ui_camera_flush(AI_UI_VIDEO_T *video);

OPERATE_RET ai_ui_camera_set_thumbnail_jpeg(uint8_t *jpeg, uint32_t len);

OPERATE_RET ai_ui_camera_close(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_UI_CAMERA_H__ */
