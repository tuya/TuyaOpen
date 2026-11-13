/**
 * @file tdd_motion_i2c.c
 * @brief Shared I2C helpers for motion sensor TDD drivers.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tdd_motion_i2c.h"

#include "tkl_pinmux.h"

static OPERATE_RET __motion_i2c_pin_func(TUYA_I2C_NUM_E port, bool scl,
                                         TUYA_PIN_FUNC_E *pin_func)
{
    static const TUYA_PIN_FUNC_E scl_funcs[] = {
        TUYA_IIC0_SCL, TUYA_IIC1_SCL, TUYA_IIC2_SCL,
        TUYA_IIC3_SCL, TUYA_IIC4_SCL, TUYA_IIC5_SCL,
    };
    static const TUYA_PIN_FUNC_E sda_funcs[] = {
        TUYA_IIC0_SDA, TUYA_IIC1_SDA, TUYA_IIC2_SDA,
        TUYA_IIC3_SDA, TUYA_IIC4_SDA, TUYA_IIC5_SDA,
    };

    if (pin_func == NULL || port >= TUYA_I2C_NUM_MAX ||
        port >= sizeof(scl_funcs) / sizeof(scl_funcs[0])) {
        return OPRT_INVALID_PARM;
    }

    *pin_func = scl ? scl_funcs[port] : sda_funcs[port];
    return OPRT_OK;
}

OPERATE_RET tdd_motion_i2c_pinmux_config(const TDD_MOTION_I2C_CFG_T *cfg)
{
    TUYA_PIN_FUNC_E pin_func;
    OPERATE_RET rt;

    if (cfg == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (cfg->scl_pin < TUYA_GPIO_NUM_MAX) {
        rt = __motion_i2c_pin_func(cfg->port, true, &pin_func);
        if (rt != OPRT_OK) {
            return rt;
        }
        rt = tkl_io_pinmux_config(cfg->scl_pin, pin_func);
        if (rt != OPRT_OK) {
            return rt;
        }
    }

    if (cfg->sda_pin < TUYA_GPIO_NUM_MAX) {
        rt = __motion_i2c_pin_func(cfg->port, false, &pin_func);
        if (rt != OPRT_OK) {
            return rt;
        }
        rt = tkl_io_pinmux_config(cfg->sda_pin, pin_func);
        if (rt != OPRT_OK) {
            return rt;
        }
    }

    return OPRT_OK;
}
