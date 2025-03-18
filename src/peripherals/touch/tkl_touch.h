/**
 * @file tkl_touch.h
 * @brief Implementation of touch controller initialization and I2C communication.
 * This file provides functionalities for initializing the I2C peripheral for touch device communication,
 * configuring the necessary I2C pins, and initializing the specific touch controller.
 *
 * The touch controller is responsible for detecting touch events and reporting them to the system.
 * This code handles the low-level I2C operations to interact with the touch controller.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */
#ifndef __TKL_TOUCH_H__
#define __TKL_TOUCH_H__

#include <stdint.h>
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_pinmux.h"
#include "tkl_i2c.h"

typedef struct {
    uint16_t x;
    uint16_t y;
} touch_point_t;

/**
 * @brief Initializes the I2C peripheral for touch device communication.
 *
 * This function configures the necessary I2C pins and initializes the I2C
 * interface to communicate with the touch device.
 *
 * @return int Returns OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET touch_i2c_peripheral_init(void);

/**
 * @brief Initializes the touch controller.
 *
 * This function initializes the I2C peripheral and then initializes the specific
 * touch IC (either GT911 or CST816X) based on the build configuration.
 *
 * @return int Returns OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET tkl_touch_init(void);

/**
 * @brief Reads touch data from the touch controller.
 *
 * This function reads the number of touch points and the touch point data from
 * the touch controller. The specific touch IC (either GT911 or CST816X) is
 * read based on the build configuration.
 *
 * @param point_num Pointer to a variable that will hold the number of touch points.
 * @param point Pointer to an array that will hold the touch point data.
 * @param max_num The maximum number of touch points that can be read.
 * @return int Returns OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET tkl_touch_read(uint8_t *point_num, touch_point_t *point, uint8_t max_num);

#endif
