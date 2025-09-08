/**
 * @file tdd_camera_dvp_gc2145.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_CAMERA_DVP_GC2145_H__
#define __TDD_CAMERA_DVP_GC2145_H__

#include "tuya_cloud_types.h"
#include "tdd_camera_dvp.h"
#include "tdd_camera_dvp_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_CAMERA_IO_CTRL_T pwr;
    TUYA_CAMERA_IO_CTRL_T rst;
    DVP_I2C_CFG_T         i2c;
}DVP_GC2145_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdl_camera_dvp_gc2145_register(char *name, DVP_GC2145_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_DVP_GC2145_H__ */
