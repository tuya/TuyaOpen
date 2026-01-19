/**
 * @file tdd_camera_dvp.h
 * @brief DVP (Digital Video Port) camera driver interface header
 *
 * This header file defines the DVP camera driver interface, including
 * sensor configuration structures, interface function callbacks, and
 * device registration functions. It provides a unified interface for
 * DVP camera sensor drivers to register with the camera management system.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_CAMERA_DVP_H__
#define __TDD_CAMERA_DVP_H__

#include "tuya_cloud_types.h"
#include "tdl_camera_driver.h"
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
    uint32_t              clk;
}TDD_DVP_SR_USR_CFG_T;

typedef struct {
    TDD_DVP_SR_USR_CFG_T  usr_cfg;
    uint16_t              max_fps;
    uint32_t              max_width;
    uint32_t              max_height;
    TUYA_FRAME_FMT_E      fmt;
}TDD_DVP_SR_CFG_T;

typedef struct {
    void *arg;
    OPERATE_RET (*rst     )(TUYA_CAMERA_IO_CTRL_T *rst_pin, void *arg);
    OPERATE_RET (*init    )(DVP_I2C_CFG_T *i2c, void *arg);
    OPERATE_RET (*set_ppi )(DVP_I2C_CFG_T *i2c, TUYA_CAMERA_PPI_E ppi, uint16_t fps, void *arg);
}TDD_DVP_SR_INTFS_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Register a DVP camera device
 * @param name Camera device name
 * @param sr_cfg Pointer to DVP sensor configuration structure containing
 *               user configuration, maximum FPS, resolution, and frame format
 * @param sr_intfs Pointer to DVP sensor interface functions structure containing
 *                 reset, initialization, and PPI setting callbacks
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET tdl_camera_dvp_device_register(char *name, TDD_DVP_SR_CFG_T *sr_cfg, TDD_DVP_SR_INTFS_T *sr_intfs);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_DVP_H__ */
