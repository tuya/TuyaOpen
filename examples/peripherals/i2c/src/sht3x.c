/**
 * @file sht3x.c
 * @brief SHT3x sensor driver for Tuya IoT projects.
 *
 * This file provides an implementation of a driver for the SHT3x sensor, which is a humidity and temperature sensor.
 * It demonstrates the configuration and usage of the SHT3x sensor using the Tuya SDK.
 * The example covers initializing the sensor, sending commands to the sensor, and reading data from the sensor.
 *
 * The SHT3x sensor driver aims to help developers understand how to interface with the SHT3x sensor in Tuya IoT
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

#define SR_I2C_ADDR_SHT3X_A 0x44 // SHT3x : ADDR pin - GND

#define SHT3X_CMD_FETCH_DATA    0xE000 // readout measurements for periodic mode
#define SHT3X_CMD_MEAS_PERI_1_H 0x2130 // measurement: periodic 1 mps, high repeatability

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
 * @brief get CRC8 value for sht3x
 *
 * @param[in] data: data to be calculated
 * @param[in] len: data length
 *
 * @return CRC8 value
 */
static uint8_t __sht3x_get_crc8(const uint8_t *data, uint16_t len)
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
static int __sht3x_check_crc8(const uint8_t *data, const uint16_t len, const uint8_t crc_val)
{
    if (__sht3x_get_crc8(data, len) != crc_val) {
        return CRC_ERR;
    }
    return CRC_OK;
}

/**
 * @brief read data from sht3x
 *
 * @param[in] dev: device resource
 * @param[in] len: data length
 * @param[out] data: data received from sht3x
 *
 * @return none
 */
static OPERATE_RET __sht3x_read_data(const uint8_t port, const uint16_t len, uint8_t *data)
{
    return tkl_i2c_master_receive(port, SR_I2C_ADDR_SHT3X_A, data, len, FALSE);
}

/**
 * @brief write command to sht3x
 *
 * @param[in] dev: device resource
 * @param[in] cmd: control command
 *
 * @return none
 */
static OPERATE_RET __sht3x_write_cmd(const uint8_t port, const uint16_t cmd)
{
    uint8_t cmd_bytes[2];
    cmd_bytes[0] = (uint8_t)(cmd >> 8);
    cmd_bytes[1] = (uint8_t)(cmd & 0x00FF);

    return tkl_i2c_master_send(port, SR_I2C_ADDR_SHT3X_A, cmd_bytes, 2, FALSE);
}

/**
 * @brief read temperature and humidity from sht3x
 *
 * @param[in] dev: device resource
 * @param[out] temp: temperature value
 * @param[out] humi: humidity value
 *
 * @return OPRT_OK on success, others on error
 */
OPERATE_RET sht3x_read_temp_humi(int port, uint16_t *temp, uint16_t *humi)
{
    uint8_t buf[6] = {0};
    OPERATE_RET ret = OPRT_OK;

    static uint8_t first_read = 0;

    if (first_read == 0) {
        ret = __sht3x_write_cmd(port, SHT3X_CMD_MEAS_PERI_1_H);
        tal_system_sleep(20);
        if (ret != OPRT_OK) {
            return ret;
        }
        first_read = 1;
    }

    ret = __sht3x_write_cmd(port, SHT3X_CMD_FETCH_DATA);
    if (ret != OPRT_OK) {
        return ret;
    }

    ret = __sht3x_read_data(port, 6, buf);
    if (ret != OPRT_OK)
        return ret;

    if ((CRC_ERR == __sht3x_check_crc8(buf, 2, buf[2])) || (CRC_ERR == __sht3x_check_crc8(buf + 3, 2, buf[5]))) {
        PR_ERR("[SHT3x] The received temp_humi data can't pass the CRC8 check.");
        return OPRT_CRC32_FAILED;
    }

    *temp = ((uint16_t)buf[0] << 8) | buf[1];
    *humi = ((uint16_t)buf[3] << 8) | buf[4];

    return OPRT_OK;
}