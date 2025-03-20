/**
 * @file tkl_touch.c
 * @brief Implementation of touch controller initialization and I2C communication.
 * This file provides functionalities for initializing the I2C peripheral for touch device communication,
 * configuring the necessary I2C pins, and initializing the specific touch controller.
 *
 * The touch controller is responsible for detecting touch events and reporting them to the system.
 * This code handles the low-level I2C operations to interact with the touch controller.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */
#include "tkl_touch.h"

/**
 * @brief Initializes the I2C peripheral for touch device communication.
 *
 * This function configures the necessary I2C pins and initializes the I2C
 * interface to communicate with the touch device.
 *
 * @return int Returns OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET touch_i2c_peripheral_init(void)
{
    TUYA_IIC_BASE_CFG_T cfg;

    tkl_io_pinmux_config(TOUCH_I2C_SCL, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(TOUCH_I2C_SDA, TUYA_IIC0_SDA);

    /*i2c init*/
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    tkl_i2c_init(TOUCH_I2C_PORT, &cfg);
    return 0;
}

/**
 * @brief Initializes the touch controller.
 *
 * This function initializes the I2C peripheral and then initializes the specific
 * touch IC (either GT911 or CST816X) based on the build configuration.
 *
 * @return int Returns OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET tkl_touch_init(void)
{
    touch_i2c_peripheral_init();

#if defined ENABLE_TOUCH_GT911
    if (gt911_i2c_init() != OPRT_OK)
        return OPRT_COM_ERROR;
#elif defined(ENABLE_TOUCH_GT1151)
    if (gt1151_i2c_init() != OPRT_OK)
        return OPRT_COM_ERROR;
#elif defined(ENABLE_TOUCH_CST816X)
    if (cst816x_i2c_init() != OPRT_OK)
        return OPRT_COM_ERROR;
#else
#error "Not support touch IC"
    return OPRT_INVALID_PARM;
#endif

    return OPRT_OK;
}

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
OPERATE_RET tkl_touch_read(uint8_t *point_num, touch_point_t *point, uint8_t max_num)
{
    OPERATE_RET ret = OPRT_OK;

    if (point_num == NULL || point == NULL)
        return OPRT_INVALID_PARM;

#if defined ENABLE_TOUCH_GT911
    ret = gt911_i2c_read(point_num, point, max_num);
#elif defined(ENABLE_TOUCH_GT1151)
    ret = gt1151_i2c_read(point_num, point, max_num);
#elif defined(ENABLE_TOUCH_CST816X)
    ret = cst816x_i2c_read(point_num, point, max_num);
#else
#error "Not support touch IC"
#endif

    if (ret != OPRT_OK) {
        PR_ERR("cst816x read failed\n");
        return ret;
    }
    return OPRT_OK;
}
