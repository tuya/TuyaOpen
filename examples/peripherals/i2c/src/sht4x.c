/**
 * @file sht4x.c
 * @brief SHT4x sensor driver for Tuya IoT projects.
 *
 * This file provides an implementation of a driver for the SHT4x sensor, which is a humidity and temperature sensor.
 * It demonstrates the configuration and usage of the SHT4x sensor using the Tuya SDK.
 * The example covers initializing the sensor, sending commands to the sensor, and reading data from the sensor.
 *
 * The SHT4x sensor driver aims to help developers understand how to interface with the SHT4x sensor in Tuya IoT
 * projects. It includes detailed examples of setting up sensor configurations, sending commands, and reading data from
 * the sensor.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms, showcasing fundamental
 * sensor operations that are critical for IoT device development.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define CRC_OK  (0)
#define CRC_ERR (-1)

#define SR_I2C_ADDR_SHT4X_A 0x44 // SHT4x-A */

#define SHT4X_CMD_MEAS_PREC_H    0xFD // measurement with high precision
#define SHT4X_CMD_READ_SERIALNBR 0x89 // read serial number

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
 * @brief get CRC8 value for sht4x
 *
 * @param[in] data: data to be calculated
 * @param[in] len: data length
 *
 * @return CRC8 value
 */
static uint8_t __sht4x_get_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t i;
    uint8_t crc = 0xFF;

    while (len--) {
        crc ^= *data;
        for (i = 8; i > 0; --i) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
        data++;
    }
    return crc;
}

/**
 * @brief check CRC8
 *
 * @param[in] data: data to be checked
 * @param[in] len: data length
 * @param[in] crc_val: crc value
 *
 * @return check result
 */
static int __sht4x_check_crc8(const uint8_t *data, const uint16_t len, const uint8_t crc_val)
{
    if (__sht4x_get_crc8(data, len) != crc_val) {
        return CRC_ERR;
    }
    return CRC_OK;
}

/**
 * @brief read data from sht4x
 *
 * @param[in] port: i2c port
 * @param[in] len: data length
 * @param[out] data: data received from sht3x
 *
 * @return none
 */
static OPERATE_RET __sht4x_read_data(const uint8_t port, const uint16_t len, uint8_t *data)
{
    return tkl_i2c_master_receive(port, SR_I2C_ADDR_SHT4X_A, data, len, FALSE);
}

/**
 * @brief write command to sht4x
 *
 * @param[in] port: i2c port
 * @param[in] cmd: control command
 *
 * @return none
 */
static OPERATE_RET __sht4x_write_cmd(const uint8_t port, const uint16_t cmd)
{
    return tkl_i2c_master_send(port, SR_I2C_ADDR_SHT4X_A, cmd, 1, FALSE);
}

/**
 * @brief read serial number
 *
 * @param[in] dev: device resource
 * @param[out] serial_nbr: serial number
 *
 * @return OPRT_OK on success, others on error
 */
OPERATE_RET __sht4x_read_serial_number(int port, uint32_t *serial_nbr)
{
    uint8_t buf[6] = {0};
    OPERATE_RET ret = OPRT_OK;

    ret = __sht4x_write_cmd(port, SHT4X_CMD_READ_SERIALNBR);
    if (ret != OPRT_OK) {
        return ret;
    }

    __sht4x_delay_ms(20);

    ret = __sht4x_read_data(port, 6, buf);
    if (ret != OPRT_OK) {
        return ret;
    }

    if ((CRC_ERR == __sht4x_check_crc8(buf, 2, buf[2])) || (CRC_ERR == __sht4x_check_crc8(buf + 3, 2, buf[5]))) {
        PR_ERR("[SHT4x] The received serial number can't pass the CRC8 check.");
        return OPRT_CRC32_FAILED;
    }

    *serial_nbr = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[3] << 8) | ((uint32_t)buf[4]);
    return OPRT_OK;
}

/**
 * @brief read temperature and humidity from sht4x
 *
 * @param[in] dev: device resource
 * @param[out] temp: temperature value
 * @param[out] humi: humidity value
 *
 * @return OPRT_OK on success, others on error
 */
OPERATE_RET sht4x_read_temp_humi(int port, uint16_t *temp, uint16_t *humi)
{
    uint8_t buf[6] = {0};
    OPERATE_RET ret = OPRT_OK;

    static uint8_t first_read = 0;
    if (0 == first_read) {
        ret = __sht4x_write_cmd(port, SHT4X_CMD_MEAS_PREC_H);
        if (ret != OPRT_OK) {
            return ret;
        }
        tal_system_sleep(20);
        first_read = 1;
    }

    ret = __sht4x_read_data(port, 6, buf);
    if (ret != OPRT_OK) {
        return ret;
    }

    if ((CRC_ERR == __sht4x_check_crc8(buf, 2, buf[2])) || (CRC_ERR == __sht4x_check_crc8(buf + 3, 2, buf[5]))) {
        PR_ERR("[SHT4x] The received temp_humi data can't pass the CRC8 check.");
        return OPRT_CRC32_FAILED;
    }

    *temp = ((uint16_t)buf[0] << 8) | buf[1];
    *humi = ((uint16_t)buf[3] << 8) | buf[4];

    return OPRT_OK;
}
