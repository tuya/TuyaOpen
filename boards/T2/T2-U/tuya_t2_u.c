/**
 * @file tuya_t2_u.c
 * @brief Implementation of common board-level hardware registration APIs for button, and LED peripherals.
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "tdd_led_gpio.h"
#include "tdd_button_gpio.h"

#include "board_com_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define BOARD_BUTTON_PIN             TUYA_GPIO_NUM_7
#define BOARD_BUTTON_ACTIVE_LV       TUYA_GPIO_LEVEL_LOW

#define BOARD_LED_PIN                TUYA_GPIO_NUM_26
#define BOARD_LED_ACTIVE_LV          TUYA_GPIO_LEVEL_LOW

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(BUTTON_NAME)
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin   = BOARD_BUTTON_PIN,
        .level = BOARD_BUTTON_ACTIVE_LV,
        .mode  = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));
#endif

    return rt;
}

static OPERATE_RET __board_register_led(void)
{ 
    OPERATE_RET rt = OPRT_OK;

#if defined(LED_NAME) 
    TDD_LED_GPIO_CFG_T led_gpio;

    led_gpio.pin   = BOARD_LED_PIN;
    led_gpio.level = BOARD_LED_ACTIVE_LV;
    led_gpio.mode  = TUYA_GPIO_PUSH_PULL;

    TUYA_CALL_ERR_RETURN(tdd_led_gpio_register(LED_NAME, &led_gpio));
#endif

    return rt;
}

/**
 * @brief Registers all the hardware peripherals (button, LED) on the board.
 * 
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_button());

    TUYA_CALL_ERR_LOG(__board_register_led());

    return rt;
}