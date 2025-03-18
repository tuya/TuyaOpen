/**
 * @file cst816x.c
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
#include "cst816x.h"

static const uint8_t CST820_CHIP_ID = 0xB7;
static const uint8_t CST816S_CHIP_ID = 0xB4;
static const uint8_t CST816D_CHIP_ID = 0xB6;
static const uint8_t CST816T_CHIP_ID = 0xB5;
static const uint8_t CST716_CHIP_ID = 0x20;

static int cst816x_i2c_port_read(uint16_t dev_addr, uint8_t register_addr, uint8_t *data, uint8_t data_len)
{
    OPERATE_RET ret = OPRT_OK;
    uint8_t read_data;

    ret = tkl_i2c_master_send(TOUCH_I2C_PORT, dev_addr, &register_addr, 1, FALSE);
    if (ret != OPRT_OK) {
        PR_ERR("send addr fail");
        return ret;
    }

    ret = tkl_i2c_master_receive(TOUCH_I2C_PORT, dev_addr, data, data_len, FALSE);
    if (ret != OPRT_OK) {
        PR_ERR("send data fail");
    }

    return ret;
}

static int cst816x_i2c_port_write(uint16_t dev_addr, uint8_t register_addr, uint8_t data)
{
    OPERATE_RET ret = OPRT_OK;
    uint8_t data_buf[2];

    data_buf[0] = register_addr;
    data_buf[1] = data;

    ret = tkl_i2c_master_send(TOUCH_I2C_PORT, dev_addr, data_buf, 2, FALSE);

    return ret;
}

/**
 * @brief Initializes the I2C communication for the CST816x device.
 *
 * This function performs the necessary steps to establish I2C communication
 * with the CST816x touch controller, including reading the chip ID and configuring
 * certain registers.
 *
 * @return int Returns 0 on success, -1 on failure.
 */
int cst816x_i2c_init(void)
{
    uint8_t bRet, Rev;
    uint8_t chip_id;

    if (cst816x_i2c_port_read(CST816_ADDR, REG_CHIP_ID, &chip_id, 1) < 0) {
        PR_ERR("read chip id fail");
        return -1;
    }

    if (chip_id == CST816D_CHIP_ID) {
        PR_DEBUG("Success:Detected chip id 0x%x", chip_id);
        cst816x_i2c_port_write(CST816_ADDR, REG_DIS_AUTOSLEEP, 0x01);
    } else {
        PR_DEBUG("Error: Not Detected.");
        return -1;
    }

    cst816x_i2c_port_write(CST816_ADDR, REG_IRQ_CTL, IRQ_EN_MOTION);

    return 0;
}

/**
 * @brief Reads touch point data from the CST816x device.
 *
 * This function reads the number of touch points and their coordinates from the
 * CST816x touch controller over I2C.
 *
 * @param point_num Pointer to a uint8_t where the number of touch points will be stored.
 * @param touch_coord Pointer to an array of touch_point_t structures where the coordinates will be stored.
 * @param max_num The maximum number of touch points that can be stored in the touch_coord array.
 * @return int Returns 0 on success, -1 on failure.
 */
int cst816x_i2c_read(uint8_t *point_num, touch_point_t *touch_coord, uint8_t max_num)
{
    uint8_t read_num = 1;
    uint8_t x_point_h, x_point_l, y_point_h, y_point_l;
    uint8_t data[13];

    if (point_num == NULL || touch_coord == NULL || max_num == 0) {
        return -1;
    }
    *point_num = 0;

    if (cst816x_i2c_port_read(CST816_ADDR, REG_STATUS, &data, sizeof(data)) < 0) {
        PR_ERR("read point num fail");
        return -1;
    }

    /* get point number */
    read_num = data[REG_TOUCH_NUM] & 3;
    if (read_num > max_num) {
        read_num = max_num;
    } else if (read_num == 0) {
        return 0;
    }

    /* get point coordinates */
    for (uint8_t i = 0; i < read_num; i++) {
        touch_coord[i].x = ((data[REG_XPOS_HIGH] & 0x0f) << 8) + data[REG_XPOS_LOW];
        touch_coord[i].y = ((data[REG_YPOS_HIGH] & 0x0f) << 8) + data[REG_YPOS_LOW];
    }
    *point_num = read_num;

    return 0;
}
