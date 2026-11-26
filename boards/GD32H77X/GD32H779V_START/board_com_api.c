/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for audio, button, and LED peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_pinmux.h"

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

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* Keep the chip-level TKL UART0 default at PA9/PA10. On this board PA9 is
     * tied to the USB VBUS_HS0 rail through R13, so override UART0 to the
     * validated PB6/PB7 alternate route before TAL UART initialization. */
    TUYA_CALL_ERR_LOG(tkl_io_pinmux_config(TUYA_GPIO_NUM_22, TUYA_UART0_TX));
    TUYA_CALL_ERR_LOG(tkl_io_pinmux_config(TUYA_GPIO_NUM_23, TUYA_UART0_RX));

    return OPRT_OK;
}
