/**
 * @file gt1151.h
 * @brief Implementation of I2C communication functions for GT1151 touch controller.
 * This file contains functions for reading from and writing to the GT1151 touch controller
 * over an I2C bus. It handles the low-level I2C operations to interact with the device.
 *
 * The GT1151 is a capacitive touch controller commonly used in various electronic devices.
 * This code facilitates the communication between the host system and the GT1151 for touch data acquisition.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#ifndef __GT1151_H__
#define __GT1151_H__

#include <stdio.h>
#include "tuya_cloud_types.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#include "tal_api.h"
#include "tkl_touch.h"

#define GT1151_I2C_SLAVE_ADDR  (0x28 >> 1)
#define GT1151_PRODUCT_ID_CODE (0x38353131) // "1151" ASCII code

#define GT1151_ADDR_LEN      (1)
#define GT1151_REG_LEN       (2)
#define GT1151_MAX_TOUCH_NUM (5)

#ifndef TOUCH_SUPPORT_MAX_NUM
#define GT1151_POINT_INFO_NUM (1)
#else
#define GT1151_POINT_INFO_NUM (TOUCH_SUPPORT_MAX_NUM)
#endif
#define GT1151_POINT_INFO_SIZE       (8)
#define GT1151_POINT_INFO_TOTAL_SIZE (GT1151_POINT_INFO_NUM * GT1151_POINT_INFO_SIZE)

#define GT1151_COMMAND_REG (0x8040)

#define GT1151_CONFIG_REG (0x8050)

#define GT1151_PRODUCT_ID       (0x8140)
#define GT1151_FIRMWARE_VERSION (0x8144)
#define GT1151_VENDOR_ID        (0x814A)

#define GT1151_STATUS (0x814E)

#define GT1151_POINT1_REG (0x814F)
#define GT1151_POINT2_REG (0x8157)
#define GT1151_POINT3_REG (0x815F)
#define GT1151_POINT4_REG (0x8167)
#define GT1151_POINT5_REG (0x816F)

#define GT1151_CHECK_SUM (0x813C)

// config parameters and relative position or range.
#define GT1151_X_OUTPUT_MAX_POS   (1)
#define GT1151_Y_OUTPUT_MAX_POS   (3)
#define GT1151_TOUCH_NUMBER_POS   (5)
#define GT1151_TOUCH_NUMBER_MIN   (1)
#define GT1151_TOUCH_NUMBER_MAX   (5)
#define GT1151_MODULE_SWITCH1_POS (6)
#define GT1151_REFRESH_RATE_POS   (15)
#define GT1151_REFRESH_RATE_MIN   (5)
#define GT1151_REFRESH_RATE_MAX   (20)
#define GT1151_CHECK_SUM_POS      (236)

/* Max detectable simultaneous touch points */
#define GT911_I2C_MAX_POINT 5

int gt911_i2c_init(void);

int gt911_i2c_read(uint8_t *point_num, touch_point_t *touch_coord, uint8_t max_num);

#endif /* __gt911_H */
