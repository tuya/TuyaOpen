/**
 * @file tdd_camera_gc0308.h
 * @brief GC0308 camera sensor driver header
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_CAMERA_GC0308_H__
#define __TDD_CAMERA_GC0308_H__

#include "tuya_cloud_types.h"
#include "tdd_camera_dvp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register GC0308 camera sensor device
 * @param name Camera device name
 * @param cfg Pointer to DVP sensor user configuration structure
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET tdd_camera_dvp_gc0308_register(char *name, TDD_DVP_SR_USR_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_GC0308_H__ */
