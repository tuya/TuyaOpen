/**
 * @file board_power_domain_api.c
 * @author Tuya Inc.
 * @brief Implementation of power domain control API for ZECTRIX_NOTE4_TY board.
 *
 * Controls the EPD 3V3, SD card 3V3 and audio AVDD_3V3 load switches.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tkl_gpio.h"
#include "tal_log.h"
#include "board_power_domain_api.h"

/***********************************************************
***********************function define**********************
***********************************************************/

static OPERATE_RET __power_domain_pin_init(TUYA_GPIO_NUM_E pin)
{
    OPERATE_RET          rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T cfg;

    cfg.mode   = TUYA_GPIO_PUSH_PULL;
    cfg.direct = TUYA_GPIO_OUTPUT;
    cfg.level  = BOARD_POWER_DOMAIN_ENABLE_LV; // Default to enabled

    rt = tkl_gpio_init(pin, &cfg);
    if (OPRT_OK != rt) {
        return rt;
    }

    return tkl_gpio_write(pin, BOARD_POWER_DOMAIN_ENABLE_LV);
}

static TUYA_GPIO_NUM_E __power_domain_pin(BOARD_POWER_DOMAIN_E domain)
{
    switch (domain) {
    case BOARD_POWER_DOMAIN_EPD_3V3:
        return BOARD_POWER_EPD_3V3_PIN;
    case BOARD_POWER_DOMAIN_SD_3V3:
        return BOARD_POWER_SD_3V3_PIN;
    case BOARD_POWER_DOMAIN_AUDIO_3V3:
        return BOARD_POWER_AUDIO_3V3_PIN;
    default:
        return TUYA_GPIO_NUM_MAX;
    }
}

OPERATE_RET board_power_domain_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__power_domain_pin_init(BOARD_POWER_EPD_3V3_PIN));
    TUYA_CALL_ERR_RETURN(__power_domain_pin_init(BOARD_POWER_SD_3V3_PIN));
    TUYA_CALL_ERR_RETURN(__power_domain_pin_init(BOARD_POWER_AUDIO_3V3_PIN));

    return rt;
}

OPERATE_RET board_power_domain_epd_3v3_enable(void)
{
    return tkl_gpio_write(BOARD_POWER_EPD_3V3_PIN, BOARD_POWER_DOMAIN_ENABLE_LV);
}

OPERATE_RET board_power_domain_epd_3v3_disable(void)
{
    return tkl_gpio_write(BOARD_POWER_EPD_3V3_PIN, BOARD_POWER_DOMAIN_DISABLE_LV);
}

OPERATE_RET board_power_domain_sd_3v3_enable(void)
{
    return tkl_gpio_write(BOARD_POWER_SD_3V3_PIN, BOARD_POWER_DOMAIN_ENABLE_LV);
}

OPERATE_RET board_power_domain_sd_3v3_disable(void)
{
    return tkl_gpio_write(BOARD_POWER_SD_3V3_PIN, BOARD_POWER_DOMAIN_DISABLE_LV);
}

OPERATE_RET board_power_domain_audio_3v3_enable(void)
{
    return tkl_gpio_write(BOARD_POWER_AUDIO_3V3_PIN, BOARD_POWER_DOMAIN_ENABLE_LV);
}

OPERATE_RET board_power_domain_audio_3v3_disable(void)
{
    return tkl_gpio_write(BOARD_POWER_AUDIO_3V3_PIN, BOARD_POWER_DOMAIN_DISABLE_LV);
}

OPERATE_RET board_power_domain_set(BOARD_POWER_DOMAIN_E domain, BOOL_T enable)
{
    TUYA_GPIO_NUM_E   pin   = __power_domain_pin(domain);
    TUYA_GPIO_LEVEL_E level = enable ? BOARD_POWER_DOMAIN_ENABLE_LV : BOARD_POWER_DOMAIN_DISABLE_LV;

    if (TUYA_GPIO_NUM_MAX == pin) {
        return OPRT_INVALID_PARM;
    }

    return tkl_gpio_write(pin, level);
}

OPERATE_RET board_power_domain_get(BOARD_POWER_DOMAIN_E domain, BOOL_T *enable)
{
    OPERATE_RET       rt  = OPRT_OK;
    TUYA_GPIO_NUM_E   pin = __power_domain_pin(domain);
    TUYA_GPIO_LEVEL_E level;

    if (NULL == enable) {
        return OPRT_INVALID_PARM;
    }
    if (TUYA_GPIO_NUM_MAX == pin) {
        return OPRT_INVALID_PARM;
    }

    rt = tkl_gpio_read(pin, &level);
    if (OPRT_OK != rt) {
        return rt;
    }

    *enable = (level == BOARD_POWER_DOMAIN_ENABLE_LV) ? TRUE : FALSE;
    return rt;
}

OPERATE_RET board_power_domain_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    tkl_gpio_deinit(BOARD_POWER_EPD_3V3_PIN);
    tkl_gpio_deinit(BOARD_POWER_SD_3V3_PIN);
    tkl_gpio_deinit(BOARD_POWER_AUDIO_3V3_PIN);

    return rt;
}
