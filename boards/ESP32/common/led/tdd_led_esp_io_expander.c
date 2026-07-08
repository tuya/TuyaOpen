/**
 * @file tdd_led_esp_io_expander.c
 * @brief LED driver for LEDs connected via IO expander on ESP32 boards (e.g. XL9555, TCA9554).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include <string.h>

#include "tal_memory.h"
#include "tdl_led_driver.h"
#include "tdd_led_esp_io_expander.h"

/***********************************************************
***********************function define**********************
***********************************************************/

static OPERATE_RET __tdd_led_io_exp_open(TDD_LED_HANDLE_T handle)
{
    TDD_LED_IO_EXP_CFG_T *cfg = (TDD_LED_IO_EXP_CFG_T *)handle;

    if (NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    /* configure pin as output */
    if (cfg->set_dir) {
        cfg->set_dir(cfg->pin_mask, 0 /* output */);
    }

    /* drive to inactive level (LED off) */
    int inactive = (cfg->active_level == TUYA_GPIO_LEVEL_HIGH) ? 0 : 1;
    cfg->set_level(cfg->pin_mask, inactive);

    return OPRT_OK;
}

static OPERATE_RET __tdd_led_io_exp_set(TDD_LED_HANDLE_T handle, bool is_on)
{
    TDD_LED_IO_EXP_CFG_T *cfg = (TDD_LED_IO_EXP_CFG_T *)handle;

    if (NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    int level;
    if (is_on) {
        level = (cfg->active_level == TUYA_GPIO_LEVEL_HIGH) ? 1 : 0;
    } else {
        level = (cfg->active_level == TUYA_GPIO_LEVEL_HIGH) ? 0 : 1;
    }

    cfg->set_level(cfg->pin_mask, level);
    return OPRT_OK;
}

static OPERATE_RET __tdd_led_io_exp_close(TDD_LED_HANDLE_T handle)
{
    TDD_LED_IO_EXP_CFG_T *cfg = (TDD_LED_IO_EXP_CFG_T *)handle;

    if (NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    /* drive to inactive level (LED off) */
    int inactive = (cfg->active_level == TUYA_GPIO_LEVEL_HIGH) ? 0 : 1;
    cfg->set_level(cfg->pin_mask, inactive);

    return OPRT_OK;
}

OPERATE_RET tdd_led_esp_io_expander_register(char *name, TDD_LED_IO_EXP_CFG_T *cfg)
{
    if (NULL == name || NULL == cfg || NULL == cfg->set_level) {
        return OPRT_INVALID_PARM;
    }

    TDD_LED_IO_EXP_CFG_T *p = (TDD_LED_IO_EXP_CFG_T *)tal_malloc(sizeof(TDD_LED_IO_EXP_CFG_T));
    if (NULL == p) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(p, cfg, sizeof(TDD_LED_IO_EXP_CFG_T));

    TDD_LED_INTFS_T intfs = {
        .led_open  = __tdd_led_io_exp_open,
        .led_set   = __tdd_led_io_exp_set,
        .led_close = __tdd_led_io_exp_close,
    };

    return tdl_led_driver_register(name, (TDD_LED_HANDLE_T)p, &intfs);
}
