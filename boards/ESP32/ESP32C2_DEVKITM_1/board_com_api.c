/**
 * @file board_com_api.c
 * @brief Implementation of common board-level hardware registration APIs for button and LED peripherals.
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "board_com_api.h"
#include "tdd_button_gpio.h"
#include "tdd_led_esp_ws1280_spi.h"

/***********************************************************
************************macro define************************
***********************************************************/

#define BOARD_BUTTON_PIN       TUYA_GPIO_NUM_9
#define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

#define BOARD_LED_PIN TUYA_GPIO_NUM_8

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
*********************function define************************
***********************************************************/

/**
 * @brief Registers the BOOT button (GPIO9, active low).
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin                = BOARD_BUTTON_PIN,
        .level              = BOARD_BUTTON_ACTIVE_LV,
        .mode               = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));

    return rt;
}

/**
 * @brief Registers the RGB LED (GPIO8, WS2812 via SPI backend).
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
static OPERATE_RET __board_register_led(void)
{
    OPERATE_RET rt = OPRT_OK;

    TDD_LED_WS1280_SPI_CFG_T led_hw_cfg = {
        .gpio      = BOARD_LED_PIN,
        .led_count = 1,
        .color     = 0x1F1F1F, // 0xRRGGBB format, low-brightness white
    };

    TUYA_CALL_ERR_RETURN(tdd_led_esp_ws1280_spi_register(LED_NAME, &led_hw_cfg));

    return rt;
}

/**
 * @brief Registers all the hardware peripherals (BOOT button, RGB LED) on the board.
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
