/**
 * @file tdd_camera_gc2145.h
 * @brief GC2145 camera sensor driver header
 *
 * This header file defines the interface for GC2145 camera sensor driver
 * registration. The GC2145 is a 2-megapixel CMOS image sensor that supports
 * various output formats including JPEG and YUV422.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_CAMERA_GC2145_H__
#define __TDD_CAMERA_GC2145_H__

#include "tuya_cloud_types.h"
#include "tdd_camera_dvp.h"

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
 * @brief Register GC2145 camera sensor device
 * @param name Camera device name
 * @param cfg Pointer to DVP sensor user configuration structure containing
 *            power control, reset control, I2C configuration, and clock settings
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET tdd_camera_dvp_gc2145_register(char *name, TDD_DVP_SR_USR_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_GC2145_H__ */
