/**
 * @file gt1151.c
 * @brief Implementation of I2C communication functions for GT1151 touch controller.
 * This file contains functions for reading from and writing to the GT1151 touch controller
 * over an I2C bus. It handles the low-level I2C operations to interact with the device.
 *
 * The GT1151 is a capacitive touch controller commonly used in various electronic devices.
 * This code facilitates the communication between the host system and the GT1151 for touch data acquisition.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "gt1151.h"

static uint8_t point_data[GT1151_POINT_INFO_TOTAL_SIZE] = {0};

static int gt1151_i2c_port_read(uint16_t dev_addr, uint16_t register_addr, uint8_t *data_buf, uint16_t len)
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

static int gt1151_i2c_port_write(uint16_t dev_addr, uint16_t register_addr, uint8_t *data_buf, uint16_t len)
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
 * @brief Initializes the GT1151 I2C device driver.
 *
 * This function sets up the necessary configurations for the GT1151 I2C device.
 * It initializes the I2C interface and prepares the device for communication.
 *
 * @return int Returns 0 on success, negative value on failure.
 */
int gt1151_i2c_init(void)
{
    uint8_t data_buf[5] = {0};
    uint32_t product_id = 0;
    uint16_t x_max = 0, y_max = 0;

    if (gt1151_i2c_port_read(GT1151_I2C_SLAVE_ADDR, GT1151_PRODUCT_ID, &product_id, sizeof(product_id))) {
        PR_ERR("touch i2c read error");
        return -1;
    }

    PR_DEBUG("Touch Product id: 0x%08x\r\n", product_id);
    if (GT1151_PRODUCT_ID_CODE != product_id) {
        PR_ERR("Touch Product ID read fail!");
        return -1;
    }

    // if (gt1151_i2c_port_read(GT1151_I2C_SLAVE_ADDR, GT911_FIRMWARE_VERSION_REG, data_buf, 2)) {
    //     return -1;
    // }
    // PR_DEBUG("Touch Firmware Version: 0x%04x", data_buf);

    // if (gt1151_i2c_port_read(GT1151_I2C_SLAVE_ADDR, GT911_X_RESOLUTION, data_buf, 4)) {
    //     return -1;
    // }
    // x_max = (((uint16_t)data_buf[1] << 8) | data_buf[0]);
    // y_max = (((uint16_t)data_buf[3] << 8) | data_buf[2]);

    // PR_DEBUG("Touch Resolution %dx%d", x_max, y_max);

    return 0;
}

/**
 * @brief Reads touch points data from the GT1151 touch controller via I2C.
 *
 * This function reads the number of touch points and their coordinates from the GT1151
 * touch controller. It communicates with the controller over the I2C bus.
 *
 * @param point_num Pointer to a variable where the number of touch points will be stored.
 * @param touch_coord Pointer to an array where touch coordinates will be stored.
 * @param max_num The maximum number of touch points to read.
 *
 * @return 0 on success, -1 on error or if there are no touch points.
 */
int gt1151_i2c_read(uint8_t *point_num, touch_point_t *touch_coord, uint8_t max_num)
{
    uint8_t read_num;
    uint8_t status;

    if (point_num == NULL || touch_coord == NULL || max_num == 0) {
        PR_ERR("invalid param");
        return -1;
    }

    if (max_num > GT1151_POINT_INFO_NUM) {
        PR_ERR("invalid param");
        return -1;
    }

    *point_num = 0;

    if (gt1151_i2c_port_read(GT1151_I2C_SLAVE_ADDR, GT1151_STATUS, &status, 1)) {
        return -1;
    }

    if (status == 0) {
        /* no touch */
        return 0;
    } else if ((status & 0x80) == 0) {
        /* no touch */
        return 0;
    }

    PR_DEBUG("GT1151 read status: 0x%02x", status & 0x0f);
    read_num = ((status & 0x0f) > max_num) ? max_num : (status & 0x0f);

    /* read gt911 reg */
    memset(point_data, 0, sizeof(point_data));
    if (gt1151_i2c_port_read(GT1151_I2C_SLAVE_ADDR, GT1151_POINT1_REG, point_data, sizeof(point_data))) {
        return -1;
    }

    PR_DEBUG("GT1151 read point data: 0x%02x", point_data[0]);

    /* get point coordinates */
    for (uint8_t i = 0; i < read_num; i++) {
        uint8_t *p_data = &point_data[i * 8];
        touch_coord[i].x = (uint16_t)p_data[2] << 8 | p_data[1];
        touch_coord[i].y = (uint16_t)p_data[4] << 8 | p_data[3];
    }

    *point_num = read_num;

    // clear status
    point_data[0] = 0;
    gt1151_i2c_port_write(GT1151_I2C_SLAVE_ADDR, GT1151_STATUS, &point_data[0], 1);

    return 0;
}
