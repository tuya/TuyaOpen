/**
 * @file tdd_tp_cst92xx.h
 * @brief tdd_tp_cst92xx module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_TP_CST92XX_H__
#define __TDD_TP_CST92XX_H__

#include "tuya_cloud_types.h"
#include "tdl_tp_driver.h"
#include "tdd_tp_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef struct {
    TUYA_GPIO_NUM_E  rst_pin;
    TDD_TP_I2C_CFG_T i2c_cfg;
    TDD_TP_CONFIG_T  tp_cfg;
} TDD_TP_CST92XX_INFO_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdd_tp_i2c_cst92xx_register(char *name, TDD_TP_CST92XX_INFO_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_TP_CST92XX_H__ */
