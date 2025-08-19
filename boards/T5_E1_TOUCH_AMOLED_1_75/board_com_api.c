/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for audio, button, and LED peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "board_com_api.h"
#include "tal_api.h"



/***********************************************************
***********************typedef define***********************
***********************************************************/
// #define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_GPIO 
// #define BOARD_LCD_BL_PIN             TUYA_GPIO_NUM_25
// #define BOARD_LCD_BL_ACTIVE_LV       TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_WIDTH              466
#define BOARD_LCD_HEIGHT             466
#define BOARD_LCD_PIXELS_FMT         TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION           TUYA_DISPLAY_ROTATION_180

#define BOARD_LCD_QSPI_PORT           TUYA_QSPI_NUM_0
#define BOARD_LCD_QSPI_CLK            48 * 1000 * 1000
#define BOARD_LCD_QSPI_CS_PIN         TUYA_GPIO_NUM_23
#define BOARD_LCD_QSPI_TE_PIN         TUYA_GPIO_NUM_31
#define BOARD_LCD_QSPI_RST_PIN        TUYA_GPIO_NUM_29

// #define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_MAX

#define BOARD_TOUCH_I2C_PORT          TUYA_I2C_NUM_0
#define BOARD_TOUCH_I2C_SCL_PIN       TUYA_GPIO_NUM_21
#define BOARD_TOUCH_I2C_SDA_PIN       TUYA_GPIO_NUM_20
/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    DISP_QSPI_DEVICE_CFG_T display_cfg;
    PR_DEBUG(DISPLAY_NAME);    
    memset(&display_cfg, 0, sizeof(DISP_QSPI_DEVICE_CFG_T));

    // display_cfg.bl.type              = BOARD_LCD_BL_TYPE;
    // display_cfg.bl.gpio.pin          = BOARD_LCD_BL_PIN;
    // display_cfg.bl.gpio.active_level = BOARD_LCD_BL_ACTIVE_LV;

    display_cfg.width     = BOARD_LCD_WIDTH;
    display_cfg.height    = BOARD_LCD_HEIGHT;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation  = BOARD_LCD_ROTATION;

    display_cfg.port      = BOARD_LCD_QSPI_PORT;
    display_cfg.spi_clk   = BOARD_LCD_QSPI_CLK;
    display_cfg.cs_pin    = BOARD_LCD_QSPI_CS_PIN;
    display_cfg.te_pin    = BOARD_LCD_QSPI_TE_PIN;
    display_cfg.rst_pin   = BOARD_LCD_QSPI_RST_PIN;

    // display_cfg.power.pin          = BOARD_LCD_POWER_PIN;

    TUYA_CALL_ERR_RETURN(tdd_disp_qspi_co5300_register(DISPLAY_NAME, &display_cfg));
    PR_INFO("12345");

    // TDD_TOUCH_I2C_CFG_T touch_cfg = {
    //     .port    = BOARD_TOUCH_I2C_PORT,
    //     .scl_pin = BOARD_TOUCH_I2C_SCL_PIN,
    //     .sda_pin = BOARD_TOUCH_I2C_SDA_PIN,
    // };

    // TUYA_CALL_ERR_RETURN(tdd_touch_i2c_cst9217_register(DISPLAY_NAME, &touch_cfg));
#endif

    return rt;
}


/**
 * @brief Registers all the hardware peripherals (audio, button, LED) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__board_register_display());
    PR_DEBUG("AMOLED 1.75");
    
    return rt;
}
