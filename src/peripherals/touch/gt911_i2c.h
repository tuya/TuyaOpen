/**
 * @file gt911_i2c.h
 * @brief Implementation of I2C communication functions for GT911 touch controller.
 * This file contains functions for reading from and writing to the GT911 touch controller
 * over an I2C bus. It handles the low-level I2C operations to interact with the device.
 *
 * The GT911 is a capacitive touch controller commonly used in various electronic devices.
 * This code facilitates the communication between the host system and the GT911 for touch data acquisition.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */
#ifndef _GT911_I2C_H
#define _GT911_I2C_H

#include <stdio.h>
#include "tuya_cloud_types.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#include "tal_api.h"
#include "tkl_touch.h"

#define GT911_CUSTOM_CFG           (1)
#define GT911_I2C_SLAVE_ADDR       (0x28 >> 1)
#define GT911_READ_XY_REG          (0x814E)
#define GT911_CLEARBUF_REG         (0x814E)
#define GT911_POINT1_REG           (0x814F)
#define GT911_POINT2_REG           (0x8157)
#define GT911_POINT3_REG           (0x815F)
#define GT911_POINT4_REG           (0x8167)
#define GT911_POINT5_REG           (0x816F)
#define GT911_CONFIG_REG           (0x8047)
#define GT911_COMMAND_REG          (0x8040)
#define GT911_PRODUCT_ID_REG       (0x8140)
#define GT911_VENDOR_ID_REG        (0x814A)
#define GT911_CONFIG_VERSION_REG   (0x8047)
#define GT911_CONFIG_CHECKSUM_REG  (0x80FF)
#define GT911_FIRMWARE_VERSION_REG (0x8144)
#define GT911_X_RESOLUTION         (0x8146)
#define GT911_Y_RESOLUTION         (0x8148)

/* Max detectable simultaneous touch points */
#define GT911_I2C_MAX_POINT 5

int gt911_i2c_init(void);

int gt911_i2c_read(uint8_t *point_num, touch_point_t *touch_coord, uint8_t max_num);

#endif /* __gt911_H */
