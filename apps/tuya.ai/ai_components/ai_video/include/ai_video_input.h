/**
 * @file ai_video_input.h
 * @brief AI video input module header
 *
 * This header file defines the types and functions for AI video input processing,
 * including camera frame capture, JPEG encoding, and video display functionality.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_VIDEO_INPUT_H__
#define __AI_VIDEO_INPUT_H__

#include "tuya_cloud_types.h"
#include "tdl_camera_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void (*AI_VIDEO_DISP_FLUSH_CB)(TDL_CAMERA_FRAME_T *frame);

typedef struct {
    AI_VIDEO_DISP_FLUSH_CB disp_flush_cb;
} AI_VIDEO_CFG_T;
/***********************************************************
********************function declaration********************
***********************************************************/
/**
@brief Initialize AI video input module
@param vi_cfg Video input configuration
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_video_init(AI_VIDEO_CFG_T *vi_cfg);

/**
@brief Get JPEG frame from camera
@param image_data Pointer to store image data pointer
@param image_data_len Pointer to store image data length
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_video_get_jpeg_frame(uint8_t **image_data, uint32_t *image_data_len);

/**
@brief Free JPEG image data
@param image_data Pointer to image data pointer
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_video_jpeg_image_free(uint8_t **image_data);

/**
@brief Start video display
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_video_display_start(void);

/**
@brief Stop video display
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_video_display_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_VIDEO_INPUT_H__ */
