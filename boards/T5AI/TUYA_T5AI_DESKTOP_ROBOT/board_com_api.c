/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for audio, button, and LED peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tkl_gpio.h"
#include "tkl_pinmux.h"

#include "tdd_disp_st7789.h"
#include "tdd_tp_cst816x.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define BOARD_POWER_PIN              TUYA_GPIO_NUM_4
#define BOARD_POWER_ACTIVE_LV        TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_GPIO 
#define BOARD_LCD_BL_PIN             TUYA_GPIO_NUM_42
#define BOARD_LCD_BL_ACTIVE_LV       TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_WIDTH              240
#define BOARD_LCD_HEIGHT             320
#define BOARD_LCD_PIXELS_FMT         TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION           TUYA_DISPLAY_ROTATION_0

#define BOARD_LCD_SPI_PORT           TUYA_SPI_NUM_0
#define BOARD_LCD_SPI_CLK            48000000
#define BOARD_LCD_SPI_CS_PIN         TUYA_GPIO_NUM_45
#define BOARD_LCD_SPI_DC_PIN         TUYA_GPIO_NUM_47
#define BOARD_LCD_SPI_RST_PIN        TUYA_GPIO_NUM_43
#define BOARD_LCD_SPI_MISO_PIN       TUYA_GPIO_NUM_46
#define BOARD_LCD_SPI_CLK_PIN        TUYA_GPIO_NUM_44

#define BOARD_LCD_PIXELS_FMT         TUYA_PIXEL_FMT_RGB565

#define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_5
#define BOARD_LCD_POWER_ACTIVE_LV    TUYA_GPIO_LEVEL_HIGH

#define BOARD_TP_I2C_PORT            TUYA_I2C_NUM_0
#define BOARD_TP_I2C_SCL_PIN         TUYA_GPIO_NUM_20
#define BOARD_TP_I2C_SDA_PIN         TUYA_GPIO_NUM_21
#define BOARD_TP_RST_PIN             TUYA_GPIO_NUM_53
#define BOARD_TP_INTR_PIN            TUYA_GPIO_NUM_MAX
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
    // Composite Pinout from chip internal, muxing set the actual pinout for SPI0 interface
    tkl_io_pinmux_config(BOARD_LCD_SPI_CS_PIN,   TUYA_SPI0_CS);
    tkl_io_pinmux_config(BOARD_LCD_SPI_CLK_PIN,  TUYA_SPI0_CLK);
    tkl_io_pinmux_config(BOARD_LCD_SPI_MISO_PIN, TUYA_SPI0_MOSI);

    DISP_SPI_DEVICE_CFG_T display_cfg;

    memset(&display_cfg, 0, sizeof(DISP_SPI_DEVICE_CFG_T));

    display_cfg.bl.type              = BOARD_LCD_BL_TYPE;
    display_cfg.bl.gpio.pin          = BOARD_LCD_BL_PIN;
    display_cfg.bl.gpio.active_level = BOARD_LCD_BL_ACTIVE_LV;

    display_cfg.width     = BOARD_LCD_WIDTH;
    display_cfg.height    = BOARD_LCD_HEIGHT;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation  = BOARD_LCD_ROTATION;

    display_cfg.port      = BOARD_LCD_SPI_PORT;
    display_cfg.spi_clk   = BOARD_LCD_SPI_CLK;
    display_cfg.cs_pin    = BOARD_LCD_SPI_CS_PIN;
    display_cfg.dc_pin    = BOARD_LCD_SPI_DC_PIN;
    display_cfg.rst_pin   = BOARD_LCD_SPI_RST_PIN;

    display_cfg.power.pin          = BOARD_LCD_POWER_PIN;
    display_cfg.power.active_level = BOARD_LCD_POWER_ACTIVE_LV;

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_st7789_register(DISPLAY_NAME, &display_cfg));


    TDD_TP_CST816X_INFO_T cst816x_info = {
        .rst_pin  = BOARD_TP_RST_PIN,
        .intr_pin = BOARD_TP_INTR_PIN,
        .i2c_cfg =
            {
                .port = BOARD_TP_I2C_PORT,
                .scl_pin = BOARD_TP_I2C_SCL_PIN,
                .sda_pin = BOARD_TP_I2C_SDA_PIN,
            },
        .tp_cfg =
            {
                .x_max = BOARD_LCD_WIDTH,
                .y_max = BOARD_LCD_HEIGHT,
                .flags =
                    {
                        .mirror_x = 0,
                        .mirror_y = 0,
                        .swap_xy = 0,
                    },
            },
    };

    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_cst816x_register(DISPLAY_NAME, &cst816x_info));

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

    TUYA_GPIO_BASE_CFG_T gpio_cfg;
    gpio_cfg.mode = TUYA_GPIO_PUSH_PULL;
    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level = TUYA_GPIO_LEVEL_HIGH;
    TUYA_CALL_ERR_LOG(tkl_gpio_init(BOARD_POWER_PIN, &gpio_cfg));

    TUYA_CALL_ERR_LOG(__board_register_display());

    return rt;
}
