/**
 * @file tdd_led_gpio.c
 * @author Tuya Inc.
 * @brief Implementation of GPIO-based LED driver functions for controlling LEDs via GPIO pins.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_led_gpio.h"

#include "tkl_gpio.h"
#include "tal_api.h"

#include "tdl_led_driver.h"
/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __tdd_led_gpio_set(TDD_LED_HANDLE_T handle, bool is_on)
{
    TDD_LED_GPIO_CFG_T *led_cfg = (TDD_LED_GPIO_CFG_T *)handle;
    TUYA_GPIO_LEVEL_E level = 0;

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    if(true == is_on) {
        level = (true == led_cfg->level) ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW;
    }else {
        level = (true == led_cfg->level) ? TUYA_GPIO_LEVEL_LOW : TUYA_GPIO_LEVEL_HIGH;
    }

    PR_NOTICE("led:%d gpio write %d", is_on, level);

    return tkl_gpio_write(led_cfg->pin, level);
}

static OPERATE_RET __tdd_led_gpio_open(TDD_LED_HANDLE_T handle)
{
    TDD_LED_GPIO_CFG_T *led_cfg = (TDD_LED_GPIO_CFG_T *)handle;
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {0};

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.mode   = led_cfg->mode;
    gpio_cfg.level  = (true == led_cfg->level) ? TUYA_GPIO_LEVEL_LOW : TUYA_GPIO_LEVEL_HIGH;
 
    return tkl_gpio_init(led_cfg->pin, &gpio_cfg);
}

static OPERATE_RET __tdd_led_gpio_close(TDD_LED_HANDLE_T handle)
{
    TDD_LED_GPIO_CFG_T *led_cfg = (TDD_LED_GPIO_CFG_T *)handle;

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }
 
    return tkl_gpio_deinit(led_cfg->pin);
}

/**
 * @brief Registers a GPIO-based LED device
 * 
 * @param dev_name The name of the LED device to register.
 * @param led_cfg A pointer to the TDD_LED_GPIO_CFG_T structure containing GPIO configuration.
 * 
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdd_led_gpio_register(char *dev_name, TDD_LED_GPIO_CFG_T *led_cfg)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_LED_GPIO_CFG_T *tdd_led_cfg  = NULL;
    TDD_LED_INTFS_T intfs;

    if(NULL == dev_name || NULL == led_cfg) {
        return OPRT_INVALID_PARM;
    }

    tdd_led_cfg = (TDD_LED_GPIO_CFG_T *)tal_malloc(sizeof(TDD_LED_GPIO_CFG_T));
    TUYA_CHECK_NULL_RETURN(tdd_led_cfg, OPRT_MALLOC_FAILED);
    memcpy(tdd_led_cfg, led_cfg, sizeof(TDD_LED_GPIO_CFG_T));

    memset(&intfs, 0x00, sizeof(TDD_LED_INTFS_T));
    intfs.led_open  = __tdd_led_gpio_open;
    intfs.led_set   = __tdd_led_gpio_set;
    intfs.led_close = __tdd_led_gpio_close;

    return tdl_led_driver_register(dev_name, (TDD_LED_HANDLE_T)tdd_led_cfg, &intfs);
}