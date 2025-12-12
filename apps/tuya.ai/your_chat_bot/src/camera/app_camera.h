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

/**
 * @brief Initialize the camera system
 *
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET app_camera_init(void);

/**
 * @brief Deinitialize the camera system
 *
 * @param None
 * @return OPERATE_RET Deinitialization result, OPRT_OK indicates success
 */
OPERATE_RET app_camera_deinit(void);

/**
 * @brief Capture a JPEG image from the camera
 *
 * @param image_data Pointer to pointer to the image data buffer (output parameter)
 * @param image_data_len Pointer to the length of the image data buffer (output parameter)
 * @param timeout_ms Timeout in milliseconds
 * @return OPERATE_RET Capture result, OPRT_OK indicates success
 * @note The image_data buffer is allocated internally and should not be freed by the caller.
 *       The buffer will be reused on the next capture call.
 */
OPERATE_RET app_camera_jpeg_capture(uint8_t **image_data, uint32_t *image_data_len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CAMERA_H__ */
