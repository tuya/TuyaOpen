/**
 * @file tdd_camera_dvp_i2c.c
 * @brief DVP camera I2C interface implementation
 *
 * This file implements the I2C interface for DVP camera sensors, including
 * I2C initialization, register read and write operations. It provides low-level
 * I2C communication functions for camera sensor configuration.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#if defined(ENABLE_I2C) && (ENABLE_I2C == 1)

#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#include "tdd_camera_dvp_i2c.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define DVP_I2C_SCL(port) 		((port) << 1)
#define DVP_I2C_SDA(port) 		(((port) << 1) + 1)

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Initialize I2C interface for DVP camera sensor
 * @param cfg Pointer to I2C configuration structure
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET tdd_dvp_i2c_init(DVP_I2C_CFG_T *cfg)
{
    if (NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_INFO("set dvp i2c, clk: %d sda: %d, idx: %d\r\n", cfg->clk, cfg->sda, cfg->port);

    tkl_io_pinmux_config(cfg->clk, DVP_I2C_SCL(cfg->port));
    tkl_io_pinmux_config(cfg->sda, DVP_I2C_SDA(cfg->port));

    TUYA_IIC_BASE_CFG_T i2c_cfg = {
        .role = TUYA_IIC_MODE_MASTER,
        .speed = TUYA_IIC_BUS_SPEED_100K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT,
    };

    return tkl_i2c_init(cfg->port, &i2c_cfg);
}

/**
 * @brief Read data from camera sensor register via I2C
 * @param cfg Pointer to I2C register configuration structure
 * @param read_len Number of bytes to read
 * @param buf Pointer to buffer to store read data
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET tdd_dvp_i2c_read(DVP_I2C_REG_CFG_T *cfg, uint16_t read_len, uint8_t *buf)
{
    if (NULL == cfg || NULL == buf) {
        return OPRT_INVALID_PARM;
    }

    if (read_len > DVP_I2C_READ_MAX_LEN) {
        PR_ERR("%s, read_len %d is overflow the limit(%d)\r\n", __func__, read_len, DVP_I2C_READ_MAX_LEN);
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    uint8_t write_data[2] = {0};
    uint8_t write_data_len = 0;

    if (cfg->is_16_reg) {
        write_data[0] = (uint8_t)((cfg->reg >> 8) & 0xFF);
        write_data[1] = (uint8_t)(cfg->reg & 0xFF);
        write_data_len = 2;
    }else {
        write_data[0] = (uint8_t)(cfg->reg & 0xFF);
        write_data_len = 1;
    }

    tkl_i2c_master_send(cfg->port, cfg->addr, write_data, write_data_len, 1);

    return tkl_i2c_master_receive(cfg->port, cfg->addr, buf, read_len, 0);
}

/**
 * @brief Write data to camera sensor register via I2C
 * @param cfg Pointer to I2C register configuration structure
 * @param write_len Number of bytes to write
 * @param buf Pointer to buffer containing data to write
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET tdd_dvp_i2c_write(DVP_I2C_REG_CFG_T *cfg, uint16_t write_len, uint8_t *buf)
{
    uint8_t reg_len = 0, write_data_len = 0;
    uint8_t write_buff[DVP_I2C_WRITE_MAX_LEN] = {0};

    if (NULL == cfg || NULL == buf) {
        return OPRT_INVALID_PARM;
    }

    reg_len = cfg->is_16_reg ? 2 : 1;
    write_data_len = reg_len + write_len;
    if (write_data_len > DVP_I2C_WRITE_MAX_LEN) {
        PR_ERR("%s, write_data_len(%d+%d) is overflow the limit(%d)\r\n", __func__, write_len, reg_len, DVP_I2C_WRITE_MAX_LEN);
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    if (cfg->is_16_reg ) {
        write_buff[0] = (uint8_t)((cfg->reg >> 8) & 0xFF);
        write_buff[1] = (uint8_t)(cfg->reg & 0xFF);
    }else {
        write_buff[0] = (uint8_t)(cfg->reg & 0xFF);
    }

    memcpy(write_buff + reg_len, buf, write_len);

    return tkl_i2c_master_send(cfg->port, cfg->addr, write_buff, write_data_len, 0);
}

#endif