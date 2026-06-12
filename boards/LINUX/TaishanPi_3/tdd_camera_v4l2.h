/**
 * @file tdd_camera_v4l2.h
 * @brief V4L2 (UVC) camera TDD adapter for TDL camera manager.
 */

#ifndef __TDD_CAMERA_V4L2_H__
#define __TDD_CAMERA_V4L2_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register a Linux V4L2 USB camera as a TDL camera device.
 *
 * @param name     Camera device name (used by tdl_camera_find_dev)
 * @param devnode  V4L2 device node path, e.g. "/dev/video0"
 */
OPERATE_RET tdd_camera_v4l2_register(const char *name, const char *devnode);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_V4L2_H__ */
