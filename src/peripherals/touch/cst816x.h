/**
 * @file cst816x.h
 * @brief Implementation of I2C communication and touch control functions for CST816x devices.
 * This file provides functionalities for initializing I2C communication with CST816x touch controllers,
 * reading touch point data, and handling other related operations.
 *
 * The CST816x series of touch controllers are used in various applications requiring capacitive touch sensing.
 * This code is responsible for establishing a connection with the touch controller over an I2C bus,
 * reading touch events, and processing the data for further use in the system.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */
#ifndef __CST816X_H__
#define __CST816X_H__

#include <stdint.h>
#include <stdbool.h>

#include "tuya_cloud_types.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#include "tal_api.h"
#include "tkl_touch.h"

#define CST816_ADDR 0x15

#define REG_STATUS        0x00
#define REG_TOUCH_NUM     0x02
#define REG_XPOS_HIGH     0x03
#define REG_XPOS_LOW      0x04
#define REG_YPOS_HIGH     0x05
#define REG_YPOS_LOW      0x06
#define REG_CHIP_ID       0xA7
#define REG_FW_VERSION    0xA9
#define REG_IRQ_CTL       0xFA
#define REG_DIS_AUTOSLEEP 0xFE

#define IRQ_EN_MOTION 0x70

/**
 * Whether the graphic is filled
 **/
typedef enum {
    CST816S_POINT_MODE = 1,
    CST816S_GESTURE_MODE,
    CST816S_ALL_MODE,
} CST816X_MODE;

int cst816x_i2c_init(void);

int cst816x_i2c_read(uint8_t *point_num, touch_point_t *touch_coord, uint8_t max_num);

#endif /* __CST816X_H__ */