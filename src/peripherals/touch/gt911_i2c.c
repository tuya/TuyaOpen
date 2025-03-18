/**
 * @file gt911_i2c.c
 * @brief Implementation of I2C communication functions for GT911 touch controller.
 * This file contains functions for reading from and writing to the GT911 touch controller
 * over an I2C bus. It handles the low-level I2C operations to interact with the device.
 *
 * The GT911 is a capacitive touch controller commonly used in various electronic devices.
 * This code facilitates the communication between the host system and the GT911 for touch data acquisition.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */
#include "gt911_i2c.h"

static uint8_t point_data[8 * GT911_I2C_MAX_POINT] = {0};

static int gt911_i2c_port_read(uint16_t dev_addr, uint16_t register_addr, uint8_t *data_buf, uint16_t len)
{
    OPERATE_RET ret = OPRT_OK;
    uint8_t cmd_bytes[2];
    cmd_bytes[0] = (uint8_t)(register_addr >> 8);
    cmd_bytes[1] = (uint8_t)(register_addr & 0x00FF);

    ret = tkl_i2c_master_send(TOUCH_I2C_PORT, dev_addr, cmd_bytes, 2, FALSE);
    if (ret != OPRT_OK) {
        PR_ERR("send cmd fail");
        return ret;
    }

    return tkl_i2c_master_receive(TOUCH_I2C_PORT, dev_addr, data_buf, len, FALSE);
}

static int gt911_i2c_port_write(uint16_t dev_addr, uint16_t register_addr, uint8_t *data_buf, uint16_t len)
{
    OPERATE_RET ret = OPRT_OK;
    uint8_t *cmd_bytes = tal_malloc(len + 2);

    if (cmd_bytes == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    cmd_bytes[0] = (uint8_t)(register_addr >> 8);
    cmd_bytes[1] = (uint8_t)(register_addr & 0x00FF);

    memcpy(&cmd_bytes[2], data_buf, len);

    ret = tkl_i2c_master_send(TOUCH_I2C_PORT, dev_addr, cmd_bytes, len + 2, FALSE);
    tal_free(cmd_bytes);

    return ret;
}

/**
 * @brief Initializes the GT911 I2C device driver.
 *
 * This function sets up the necessary configurations for the GT911 I2C device.
 * It initializes the I2C interface and prepares the device for communication.
 *
 * @return int Returns 0 on success, negative value on failure.
 */
int gt911_i2c_init(void)
{
    uint8_t data_buf[5] = {0};
    uint32_t product_id = 0;
    uint16_t x_max = 0, y_max = 0;

    if (gt911_i2c_port_read(GT911_I2C_SLAVE_ADDR, GT911_PRODUCT_ID_REG, data_buf, 4)) {
        PR_ERR("touch i2c read error");
        return -1;
    }

    if (strcmp(data_buf, "\x39\x31\x31\x00") != 0) {
        PR_ERR("Touch Product ID read fail!");
        return -1;
    }

    PR_DEBUG("Touch Product ID: %s", data_buf);

    if (gt911_i2c_port_read(GT911_I2C_SLAVE_ADDR, GT911_FIRMWARE_VERSION_REG, data_buf, 2)) {
        return -1;
    }
    PR_DEBUG("Touch Firmware Version: 0x%04x", data_buf);

    if (gt911_i2c_port_read(GT911_I2C_SLAVE_ADDR, GT911_X_RESOLUTION, data_buf, 4)) {
        return -1;
    }
    x_max = (((uint16_t)data_buf[1] << 8) | data_buf[0]);
    y_max = (((uint16_t)data_buf[3] << 8) | data_buf[2]);

    PR_DEBUG("Touch Resolution %dx%d", x_max, y_max);

    return 0;
}

/**
 * @brief Reads touch points data from the GT911 touch controller via I2C.
 *
 * This function reads the number of touch points and their coordinates from the GT911
 * touch controller. It communicates with the controller over the I2C bus.
 *
 * @param point_num Pointer to a variable where the number of touch points will be stored.
 * @param touch_coord Pointer to an array where touch coordinates will be stored.
 * @param max_num The maximum number of touch points to read.
 *
 * @return 0 on success, -1 on error or if there are no touch points.
 */
int gt911_i2c_read(uint8_t *point_num, touch_point_t *touch_coord, uint8_t max_num)
{
    uint8_t read_num;

    if (point_num == NULL || touch_coord == NULL || max_num == 0) {
        return -1;
    }

    *point_num = 0;

    if (gt911_i2c_port_read(GT911_I2C_SLAVE_ADDR, GT911_READ_XY_REG, point_data, 1)) {
        return -1;
    }

    /* no touch */
    if (point_data[0] == 0) {
        return 0;
    } else if (point_data[0] == 0x80) {
        point_data[0] = 0;
        gt911_i2c_port_write(GT911_I2C_SLAVE_ADDR, GT911_READ_XY_REG, &point_data[0], 1);

        return 0;
    }

    if (point_data[0] > GT911_I2C_MAX_POINT) {
        point_data[0] = GT911_I2C_MAX_POINT;
    }

    read_num = (point_data[0] > max_num) ? max_num : point_data[0];

    /* read gt911 reg */
    gt911_i2c_port_read(GT911_I2C_SLAVE_ADDR, GT911_READ_XY_REG, point_data, (8 * read_num));

    /* get point coordinates */
    for (uint8_t i = 0; i < read_num; i++) {
        uint8_t *p_data = &point_data[i * 8];
        touch_coord[i].x = (uint16_t)p_data[3] << 8 | p_data[2];
        touch_coord[i].y = (uint16_t)p_data[5] << 8 | p_data[4];
    }

    *point_num = read_num;

    point_data[0] = 0;
    gt911_i2c_port_write(GT911_I2C_SLAVE_ADDR, GT911_READ_XY_REG, &point_data[0], 1);

    return 0;
}
