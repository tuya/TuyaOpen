/**
 * @file board_com_api.h
 * @author Tuya Inc.
 * @brief Common board-level hardware registration API for the AC79 devkit.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __JIELI_BOARD_COM_API_H__
#define __JIELI_BOARD_COM_API_H__

#include "tuya_cloud_types.h"

/* ---- Camera module -------------------------------------------------------
 * Same shape as the T5AI boards: the board names the module it carries and
 * hands these pins to the TDD sensor driver. Exactly one module can be fitted
 * (the two connectors share pins), so this is a choice, not a runtime probe.
 * See the "Camera module" choice in the board Kconfig. */
#if defined(JIELI_BOARD_CAMERA_GC2145) && (JIELI_BOARD_CAMERA_GC2145 == 1)
/* J1 24-pin DVP. The sensor's I2C is wired to PH0/PH1, which the adapter
 * drives with its software IIC port (iic0); the TDD owns reset and tables. */
#define BOARD_CAMERA_I2C_PORT  TUYA_I2C_NUM_0
#define BOARD_CAMERA_I2C_SCL   JIELI_BOARD_IIC0_SCL_PIN
#define BOARD_CAMERA_I2C_SDA   JIELI_BOARD_IIC0_SDA_PIN
#define BOARD_CAMERA_RST_PIN   TUYA_GPIO_NUM_115 /* PH3 */
#define BOARD_CAMERA_POWER_PIN TUYA_GPIO_NUM_MAX
#define BOARD_CAMERA_CLK       24000000
#elif defined(JIELI_BOARD_CAMERA_GC0310) && (JIELI_BOARD_CAMERA_GC0310 == 1)
/* J4 12-pin SPI: the TDD owns the sensor. Its I2C sits on PH0/PH1, so the
 * adapter bit-bangs that port (the pads carry the sensor's 2-wire SPI too). */
#define BOARD_CAMERA_I2C_PORT  TUYA_I2C_NUM_1
#define BOARD_CAMERA_I2C_SCL   JIELI_BOARD_IIC1_SCL_PIN
#define BOARD_CAMERA_I2C_SDA   JIELI_BOARD_IIC1_SDA_PIN
#define BOARD_CAMERA_RST_PIN   TUYA_GPIO_NUM_115 /* PH3 */
#define BOARD_CAMERA_POWER_PIN TUYA_GPIO_NUM_MAX
#define BOARD_CAMERA_CLK       24000000
#endif

OPERATE_RET board_register_hardware(void);

#endif /* __JIELI_BOARD_COM_API_H__ */
