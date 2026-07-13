/**
 * @file example_camera.h
 * @brief Camera capture module for photo album example
 * @version 0.2
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#ifndef __EXAMPLE_CAMERA_H__
#define __EXAMPLE_CAMERA_H__

#include "tuya_cloud_types.h"
#include "tdl_camera_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define EXAMPLE_CAMERA_WIDTH   480
#define EXAMPLE_CAMERA_HEIGHT  480

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef void (*EXAMPLE_CAMERA_FRAME_CB_T)(uint8_t *yuv_data, uint16_t width, uint16_t height);

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */

/**
 * @brief Initialize camera with JPEG + YUV dual output
 * @return OPRT_OK on success
 */
OPERATE_RET example_camera_init(void);

/**
 * @brief Enable or disable camera live preview
 * @param[in] enable TRUE to forward frames to callback, FALSE to stop
 * @return none
 */
void example_camera_preview_enable(BOOL_T enable);

/**
 * @brief Register YUV422 frame callback for preview display
 * @param[in] cb Callback invoked with each raw frame (called from camera driver context)
 * @return none
 */
void example_camera_set_frame_cb(EXAMPLE_CAMERA_FRAME_CB_T cb);

/**
 * @brief Capture one JPEG frame synchronously (blocks until frame arrives or timeout)
 * @param[out] data Receives Malloc'd JPEG buffer (caller must Free)
 * @param[out] len JPEG data length in bytes
 * @return OPRT_OK on success
 */
OPERATE_RET example_camera_capture_jpeg(uint8_t **data, uint32_t *len);

void example_camera_release_jpeg(uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __EXAMPLE_CAMERA_H__ */
