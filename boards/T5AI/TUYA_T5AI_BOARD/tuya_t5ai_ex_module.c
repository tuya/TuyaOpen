/**
 * @file tuya_t5ai_ex_module.c
 * @version 0.1
 * @date 2025-07-01
 */

#include "tal_api.h"

#include "tuya_t5ai_ex_module.h"

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
#if defined (TUYA_T5AI_BOARD_EX_MODULE_35565LCD) && (TUYA_T5AI_BOARD_EX_MODULE_35565LCD ==1)
static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    DISP_RGB_DEVICE_CFG_T display_cfg;

    memset(&display_cfg, 0, sizeof(DISP_RGB_DEVICE_CFG_T));

    display_cfg.sw_spi_cfg.spi_clk = BOARD_LCD_SW_SPI_CLK_PIN;
    display_cfg.sw_spi_cfg.spi_sda = BOARD_LCD_SW_SPI_SDA_PIN;
    display_cfg.sw_spi_cfg.spi_csx = BOARD_LCD_SW_SPI_CSX_PIN;
    display_cfg.sw_spi_cfg.spi_dc  = BOARD_LCD_SW_SPI_DC_PIN;
    display_cfg.sw_spi_cfg.spi_rst = BOARD_LCD_SW_SPI_RST_PIN;

    display_cfg.bl.type              = BOARD_LCD_BL_TYPE;
    display_cfg.bl.gpio.pin          = BOARD_LCD_BL_PIN;
    display_cfg.bl.gpio.active_level = BOARD_LCD_BL_ACTIVE_LV;

    display_cfg.width     = BOARD_LCD_WIDTH;
    display_cfg.height    = BOARD_LCD_HEIGHT;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation  = BOARD_LCD_ROTATION;

    display_cfg.power.pin = BOARD_LCD_POWER_PIN;

    TUYA_CALL_ERR_RETURN(tdd_disp_rgb_ili9488_register(DISPLAY_NAME, &display_cfg));

    TDD_TOUCH_I2C_CFG_T touch_cfg = {
        .port    = BOARD_TOUCH_I2C_PORT,
        .scl_pin = BOARD_TOUCH_I2C_SCL_PIN,
        .sda_pin = BOARD_TOUCH_I2C_SDA_PIN,
    };

    TUYA_CALL_ERR_RETURN(tdd_touch_i2c_gt1151_register(DISPLAY_NAME, &touch_cfg));
#endif

    return rt;
}
#elif defined (TUYA_T5AI_BOARD_EX_MODULE_EYES) && (TUYA_T5AI_BOARD_EX_MODULE_EYES ==1)
static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    DISP_QSPI_DEVICE_CFG_T display_cfg;

    memset(&display_cfg, 0, sizeof(DISP_QSPI_DEVICE_CFG_T));

    display_cfg.bl.type              = BOARD_LCD_BL_TYPE;
    display_cfg.bl.gpio.pin          = BOARD_LCD_BL_PIN;
    display_cfg.bl.gpio.active_level = BOARD_LCD_BL_ACTIVE_LV;

    display_cfg.width     = BOARD_LCD_WIDTH;
    display_cfg.height    = BOARD_LCD_HEIGHT;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation  = BOARD_LCD_ROTATION;

    display_cfg.port      = BOARD_LCD_QSPI_PORT;
    display_cfg.spi_clk   = BOARD_LCD_QSPI_CLK;
    display_cfg.cs_pin    = BOARD_LCD_QSPI_CS_PIN;
    display_cfg.dc_pin    = BOARD_LCD_QSPI_DC_PIN;
    display_cfg.rst_pin   = BOARD_LCD_QSPI_RST_PIN;

    display_cfg.power.pin          = BOARD_LCD_POWER_PIN;

    TUYA_CALL_ERR_RETURN(tdd_disp_qspi_st7735s_register(DISPLAY_NAME, &display_cfg));
#endif

    return rt;
}
#elif defined (TUYA_T5AI_BOARD_EX_MODULE_29E_INK) && (TUYA_T5AI_BOARD_EX_MODULE_29E_INK ==1)
static OPERATE_RET __board_register_display(void)   
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    DISP_SPI_DEVICE_CFG_T display_cfg;

    memset(&display_cfg, 0, sizeof(DISP_SPI_DEVICE_CFG_T));

    display_cfg.bl.type   = BOARD_LCD_BL_TYPE;

    display_cfg.width     = BOARD_LCD_WIDTH;
    display_cfg.height    = BOARD_LCD_HEIGHT;
    display_cfg.rotation  = BOARD_LCD_ROTATION;

    display_cfg.port      = BOARD_LCD_SPI_PORT;
    display_cfg.spi_clk   = BOARD_LCD_SPI_CLK;
    display_cfg.cs_pin    = BOARD_LCD_SPI_CS_PIN;
    display_cfg.dc_pin    = BOARD_LCD_SPI_DC_PIN;
    display_cfg.rst_pin   = BOARD_LCD_SPI_RST_PIN;

    display_cfg.power.pin = BOARD_LCD_POWER_PIN;

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_mono_st7305_register(DISPLAY_NAME, &display_cfg, BOARD_LCD_CASET_XS));
#endif

    return rt;
}
#elif defined (TUYA_T5AI_BOARD_EX_MODULE_096_OLED) && (TUYA_T5AI_BOARD_EX_MODULE_096_OLED ==1)
static OPERATE_RET __board_register_display(void)   
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    DISP_I2C_OLED_DEVICE_CFG_T display_cfg;
    DISP_SSD1306_INIT_CFG_T  init_cfg;

    memset(&display_cfg, 0, sizeof(DISP_I2C_OLED_DEVICE_CFG_T));
    memset(&init_cfg, 0, sizeof(DISP_SSD1306_INIT_CFG_T));

    display_cfg.bl.type   = BOARD_LCD_BL_TYPE;

    display_cfg.width     = BOARD_LCD_WIDTH;
    display_cfg.height    = BOARD_LCD_HEIGHT;
    display_cfg.rotation  = BOARD_LCD_ROTATION;
    display_cfg.port      = BOARD_LCD_I2C_PORT;
    display_cfg.addr      = BOARD_LCD_I2C_SLAVER_ADDR;

    display_cfg.power.pin = BOARD_LCD_POWER_PIN;

    init_cfg.is_color_inverse = BOARD_LCD_COLOR_INVERSE;
    init_cfg.com_pin_cfg      = BOARD_LCD_COM_PIN_CFG;

    TUYA_CALL_ERR_RETURN(tdd_disp_i2c_oled_ssd1306_register(DISPLAY_NAME, &display_cfg, &init_cfg));
#endif

    return rt;
}

#else
static OPERATE_RET __board_register_display(void)
{
    return OPRT_OK;
}

#endif

OPERATE_RET board_register_ex_module(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__board_register_display());

    return rt;
}
