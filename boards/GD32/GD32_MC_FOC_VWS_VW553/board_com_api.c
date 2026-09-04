/**
 * @file board_com_api.c
 * @brief Implementation of common board-level hardware registration APIs for button, and LED peripherals.
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "board_com_api.h"
#include "tkl_pinmux.h"

/***********************************************************
************************macro define************************
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
static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;
    return rt;
}

static OPERATE_RET __board_register_led(void)
{
    OPERATE_RET rt = OPRT_OK;

    return rt;
}

/**
 * @brief Registers all the hardware peripherals (button, LED) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
/**
 * @brief Route the peripherals this board did not wire to their reset pins.
 *
 * USART0 receives on PA15 here rather than the PA1 the chip resets to. Stating it through
 * tkl_io_pinmux_config() keeps the whole mapping in one place - the alternative, a #define
 * override inside the vendor sdk's uart.h, has to be duplicated for every peripheral and
 * leaves an application unable to tell which pins it actually got.
 *
 * tkl_uart_init() checks for this and then leaves its own default pin untouched, so the
 * order that matters is only "before the port is opened", not before anything else.
 */
static OPERATE_RET __board_register_pinmux(void)
{
    return tkl_io_pinmux_config(TUYA_IO_PIN_15, TUYA_UART0_RX);
}

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_pinmux());

    return rt;
}
