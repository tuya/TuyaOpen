/**
 * @file tdd_camera_gc0310_spi.h
 * @brief GC0310 camera sensor driver over SPI, TDD-owned sensor bring-up
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_CAMERA_GC0310_SPI_H__
#define __TDD_CAMERA_GC0310_SPI_H__

#include "tuya_cloud_types.h"
#include "tdd_camera_dvp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register a GC0310 sensor whose SPI output block is configured by
 *        this driver over the TDD I2C interface.
 *
 * The board supplies the I2C port/pins and the reset pin in cfg->usr_cfg, so
 * the transport (tkl_dvp) does not need the vendor video1 sensor device.
 */
OPERATE_RET tdd_camera_gc0310_spi_register(char *name, TDD_DVP_SR_USR_CFG_T *cfg);


#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_GC0310_SPI_H__ */
