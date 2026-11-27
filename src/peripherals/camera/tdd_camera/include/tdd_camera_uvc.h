/**
 * @file tdd_camera_uvc.h
 * @brief UVC (USB video class) camera driver interface
 *
 * The T5AI UVC stack (bk_uvc + multimedia) has no TKL wrapper yet, so this
 * driver talks to it directly: the camera is opened through the media
 * pipeline and the MJPEG frames are fed into the TDL frame pool. Applications
 * use a UVC camera like any other tdl_camera device.
 *
 * @note The media stack delivers the frames through one global callback, so
 * only one UVC camera can be registered.
 *
 * @version 1.0
 * @date 2026-11-27
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved
 */

#ifndef __TDD_CAMERA_UVC_H__
#define __TDD_CAMERA_UVC_H__

#include "tuya_cloud_types.h"
#include "tdl_camera_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_CAMERA_IO_CTRL_T pwr; /**< camera power gpio, pwr.pin = TUYA_GPIO_NUM_MAX when not controlled */
} TDD_CAMERA_UVC_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Register a UVC camera device.
 *
 * @param[in] name Camera device name, used by tdl_camera_find_dev().
 * @param[in] cfg UVC board configuration, NULL when the camera is powered directly.
 *
 * @return OPRT_OK on success, others on failure.
 */
OPERATE_RET tdd_camera_uvc_register(char *name, TDD_CAMERA_UVC_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_UVC_H__ */
