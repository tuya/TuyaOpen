/**
 * @file tdd_camera_csi2.h
 * @brief MIPI CSI2 camera driver interface header
 *
 * This header defines the MIPI CSI2 camera driver interface, extending
 * TuyaOpen's camera subsystem with native CSI2 support while maintaining
 * identical frame semantics, lifecycle and callbacks.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_CAMERA_CSI2_H__
#define __TDD_CAMERA_CSI2_H__

#include "tuya_cloud_types.h"
#include "tdl_camera_driver.h"
#include "tdd_camera_dvp_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    TUYA_CAMERA_IO_CTRL_T pwr;
    TUYA_CAMERA_IO_CTRL_T rst;
    DVP_I2C_CFG_T         i2c;
    uint32_t              clk;
    uint8_t               data_lanes;
} TDD_CSI2_SR_USR_CFG_T;

/**
 * @brief Register a MIPI CSI2 camera device
 * @param name Camera device name
 * @param cfg Pointer to CSI2 configuration structure
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET tdd_camera_csi2_register(char *name, TDD_CSI2_SR_USR_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_CSI2_H__ */
