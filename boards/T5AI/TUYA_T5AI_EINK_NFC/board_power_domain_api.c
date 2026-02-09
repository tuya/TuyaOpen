/**
 * @file board_power_domain_api.c
 * @author Tuya Inc.
 * @brief Implementation of power domain control API for TUYA_T5AI_EINK_NFC board.
 *
 * This module provides APIs to control the power domains for EINK 3V3 and SD card 3V3.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tkl_gpio.h"
#include "board_power_domain_api.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET board_power_domain_init(void)
{
    OPERATE_RET          rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T cfg;

    // Initialize EINK 3V3 power domain GPIO
    cfg.mode   = TUYA_GPIO_PUSH_PULL;
    cfg.direct = TUYA_GPIO_OUTPUT;
    cfg.level  = BOARD_POWER_DOMAIN_ENABLE_LV; // Default to enabled (high)
    rt         = tkl_gpio_init(BOARD_POWER_EINK_3V3_PIN, &cfg);
    if (OPRT_OK != rt) {
        return rt;
    }

    // Set EINK 3V3 to enabled by default
    rt = tkl_gpio_write(BOARD_POWER_EINK_3V3_PIN, BOARD_POWER_DOMAIN_ENABLE_LV);
    if (OPRT_OK != rt) {
        return rt;
    }

    // Initialize SD card 3V3 power domain GPIO
    cfg.level = BOARD_POWER_DOMAIN_ENABLE_LV; // Default to enabled (high)
    rt        = tkl_gpio_init(BOARD_POWER_SD_3V3_PIN, &cfg);
    if (OPRT_OK != rt) {
        return rt;
    }

    // Set SD card 3V3 to enabled by default
    rt = tkl_gpio_write(BOARD_POWER_SD_3V3_PIN, BOARD_POWER_DOMAIN_ENABLE_LV);
    if (OPRT_OK != rt) {
        return rt;
    }

    return rt;
}

OPERATE_RET board_power_domain_eink_3v3_enable(void)
{
    OPERATE_RET rt = OPRT_OK;
    rt             = tkl_gpio_write(BOARD_POWER_EINK_3V3_PIN, BOARD_POWER_DOMAIN_ENABLE_LV);
    return rt;
}

OPERATE_RET board_power_domain_eink_3v3_disable(void)
{
    OPERATE_RET rt = OPRT_OK;
    rt             = tkl_gpio_write(BOARD_POWER_EINK_3V3_PIN, BOARD_POWER_DOMAIN_DISABLE_LV);
    return rt;
}

OPERATE_RET board_power_domain_sd_3v3_enable(void)
{
    OPERATE_RET rt = OPRT_OK;
    rt             = tkl_gpio_write(BOARD_POWER_SD_3V3_PIN, BOARD_POWER_DOMAIN_ENABLE_LV);
    return rt;
}

OPERATE_RET board_power_domain_sd_3v3_disable(void)
{
    OPERATE_RET rt = OPRT_OK;
    rt             = tkl_gpio_write(BOARD_POWER_SD_3V3_PIN, BOARD_POWER_DOMAIN_DISABLE_LV);
    return rt;
}

OPERATE_RET board_power_domain_set(BOARD_POWER_DOMAIN_E domain, BOOL_T enable)
{
    OPERATE_RET       rt = OPRT_OK;
    TUYA_GPIO_NUM_E   pin;
    TUYA_GPIO_LEVEL_E level;

    // Select the appropriate pin
    switch (domain) {
    case BOARD_POWER_DOMAIN_EINK_3V3:
        pin = BOARD_POWER_EINK_3V3_PIN;
        break;
    case BOARD_POWER_DOMAIN_SD_3V3:
        pin = BOARD_POWER_SD_3V3_PIN;
        break;
    default:
        return OPRT_INVALID_PARM;
    }

    // Set the level based on enable state
    level = enable ? BOARD_POWER_DOMAIN_ENABLE_LV : BOARD_POWER_DOMAIN_DISABLE_LV;
    rt    = tkl_gpio_write(pin, level);
    return rt;
}

OPERATE_RET board_power_domain_get(BOARD_POWER_DOMAIN_E domain, BOOL_T *enable)
{
    OPERATE_RET       rt = OPRT_OK;
    TUYA_GPIO_NUM_E   pin;
    TUYA_GPIO_LEVEL_E level;

    if (NULL == enable) {
        return OPRT_INVALID_PARM;
    }

    // Select the appropriate pin
    switch (domain) {
    case BOARD_POWER_DOMAIN_EINK_3V3:
        pin = BOARD_POWER_EINK_3V3_PIN;
        break;
    case BOARD_POWER_DOMAIN_SD_3V3:
        pin = BOARD_POWER_SD_3V3_PIN;
        break;
    default:
        return OPRT_INVALID_PARM;
    }

    // Read the current level
    rt = tkl_gpio_read(pin, &level);
    if (OPRT_OK != rt) {
        return rt;
    }

    // Convert level to boolean
    *enable = (level == BOARD_POWER_DOMAIN_ENABLE_LV) ? TRUE : FALSE;
    return rt;
}

OPERATE_RET board_power_domain_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;
    OPERATE_RET rt1, rt2;

    rt1 = tkl_gpio_deinit(BOARD_POWER_EINK_3V3_PIN);
    rt2 = tkl_gpio_deinit(BOARD_POWER_SD_3V3_PIN);

    // Return error if either deinit failed
    if (OPRT_OK != rt1) {
        return rt1;
    }
    if (OPRT_OK != rt2) {
        return rt2;
    }

    return rt;
}
