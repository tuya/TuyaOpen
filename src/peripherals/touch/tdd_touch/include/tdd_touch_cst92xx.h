/**
 * @file tdd_touch_cst92xx.h
 * @brief tdd_touch_cst92xx module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_TOUCH_CST92XX_H__
#define __TDD_TOUCH_CST92XX_H__

#include "tuya_cloud_types.h"
#include "tdl_touch_driver.h"
#include "tdd_touch_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef struct {
    TDD_TOUCH_I2C_CFG_T i2c_cfg;
    TDL_TOUCH_CONFIG_T tp_cfg;
} TDD_TOUCH_CST92XX_INFO_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdd_touch_i2c_cst92xx_register(char *name, TDD_TOUCH_CST92XX_INFO_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_TOUCH_CST92XX_H__ */
