/**
 * @file tdd_camera_ov5640.h
 * @brief OV5640 DVP camera sensor driver interface.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_CAMERA_OV5640_H__
#define __TDD_CAMERA_OV5640_H__

#include "tuya_cloud_types.h"
#include "tdd_camera_dvp.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET tdd_camera_dvp_ov5640_register(char *name, TDD_DVP_SR_USR_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_OV5640_H__ */
