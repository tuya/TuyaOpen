/**
 * @file tuya_t9_ex_module.c
 * @brief Board-level module registration for the T9 EVB.
 *
 * Display is the first module on this board. Camera and touch registration will
 * be added here as they are brought up.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#include "tuya_t9_ex_module.h"

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
#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
static OPERATE_RET __board_register_display(void)
{
    TDD_DISP_MIPI_BOARD_IO_T io;

    memset(&io, 0, sizeof(io));

    io.reset.pin          = BOARD_LCD_RESET_PIN;
    io.reset.active_level = BOARD_LCD_RESET_ACTIVE_LEVEL;
    io.power.pin          = BOARD_LCD_POWER_PIN;
    io.power.active_level = TUYA_GPIO_LEVEL_HIGH;

    io.bl.type              = TUYA_DISP_BL_TP_GPIO;
    io.bl.gpio.pin          = BOARD_LCD_BL_PIN;
    io.bl.gpio.active_level = BOARD_LCD_BL_ACTIVE_LEVEL;

    io.rotation         = TUYA_DISPLAY_ROTATION_270;
    io.is_swap          = false;
    io.input_decompress = false;
    io.gpio_drive_cap   = TUYA_GPIO_DRIVE_CAP_3;
    io.max_inflight     = 4;

    return tdd_disp_mipi_jd9165t_register(DISPLAY_NAME, &io);
}
#endif

OPERATE_RET board_register_ex_module(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
    TUYA_CALL_ERR_RETURN(__board_register_display());
#endif

    return rt;
}
