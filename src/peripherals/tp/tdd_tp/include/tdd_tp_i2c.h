/**
 * @file tdd_tp_i2c.h
 * @brief I2C communication interface definitions for tp controllers
 *
 * This header file defines the I2C communication interface structures and
 * function prototypes for tp controller devices in the TDD layer. It provides
 * configuration structures for I2C parameters and common I2C read/write functions
 * used by various tp controller drivers.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_TP_I2C_H__
#define __TDD_TP_I2C_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_I2C_NUM_E port;
    TUYA_PIN_NAME_E scl_pin;
    TUYA_PIN_NAME_E sda_pin;
} TDD_TP_I2C_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
void tdd_tp_i2c_pinmux_config(TDD_TP_I2C_CFG_T *cfg);

OPERATE_RET tdd_tp_i2c_port_read(TUYA_I2C_NUM_E port, uint16_t dev_addr, uint16_t reg_addr, uint8_t reg_addr_len,
                                    uint8_t *data, uint8_t data_len);

OPERATE_RET tdd_tp_i2c_port_write(TUYA_I2C_NUM_E port, uint16_t dev_addr, uint16_t reg_addr, uint8_t reg_addr_len,
                                     uint8_t *data, uint8_t data_len);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_TP_I2C_H__ */
