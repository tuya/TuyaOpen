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
    display_cfg.bl.pwm.id            = BOARD_LCD_BL_PWM_ID;
    display_cfg.bl.pwm.cfg.frequency = 1000;
    display_cfg.bl.pwm.cfg.duty      = 10000; // 0-10000

    display_cfg.width     = BOARD_LCD_WIDTH;
    display_cfg.height    = BOARD_LCD_HEIGHT;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation  = BOARD_LCD_ROTATION;

    display_cfg.power.pin = BOARD_LCD_POWER_PIN;

    TUYA_CALL_ERR_RETURN(tdd_disp_rgb_ili9488_register(DISPLAY_NAME, &display_cfg));

    TDD_TP_GT1151_INFO_T tp_cfg = {
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

    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_gt1151_register(DISPLAY_NAME, &tp_cfg));
#endif

    return rt;
}
#elif defined (TUYA_T5AI_BOARD_EX_MODULE_EYES) && (TUYA_T5AI_BOARD_EX_MODULE_EYES ==1)
static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
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

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_st7735s_register(DISPLAY_NAME, &display_cfg));
#endif

#if defined(ENABLE_EYES_TWO_LCD) && (ENABLE_EYES_TWO_LCD == 1)
    DISP_SPI_DEVICE_CFG_T display2_cfg;

    memset(&display2_cfg, 0, sizeof(DISP_SPI_DEVICE_CFG_T));

    display2_cfg.bl.type              = BOARD_LCD_BL_TYPE;
    display2_cfg.bl.gpio.pin          = BOARD_LCD_BL_PIN;
    display2_cfg.bl.gpio.active_level = BOARD_LCD_BL_ACTIVE_LV;

    display2_cfg.width     = BOARD_LCD_WIDTH;
    display2_cfg.height    = BOARD_LCD_HEIGHT;
    display2_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display2_cfg.rotation  = BOARD_LCD_ROTATION;

    display2_cfg.port      = BOARD_LCD_SPI2_PORT;
    display2_cfg.spi_clk   = BOARD_LCD_SPI2_CLK;
    display2_cfg.cs_pin    = BOARD_LCD_SPI2_CS_PIN;
    display2_cfg.dc_pin    = BOARD_LCD_SPI2_DC_PIN;
    display2_cfg.rst_pin   = BOARD_LCD_SPI2_RST_PIN;

    display2_cfg.power.pin  = BOARD_LCD_POWER_PIN;

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_st7735s_register(DISPLAY_NAME_2, &display2_cfg));
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


#if defined (ENABLE_EX_MODULE_CAMERA) && (ENABLE_EX_MODULE_CAMERA ==1)
static OPERATE_RET __board_register_camera(void)
{
#if defined(CAMERA_NAME)
    OPERATE_RET rt = OPRT_OK;
    TDD_DVP_SR_USR_CFG_T camera_cfg = {
        .pwr = {
            .pin = BOARD_CAMERA_POWER_PIN,
        },
        .rst = {
            .pin = BOARD_CAMERA_RST_PIN,
            .active_level = BOARD_CAMERA_RST_ACTIVE_LV,
        },
        .i2c ={
            .port = BOARD_CAMERA_I2C_PORT,
            .clk  = BOARD_CAMERA_I2C_SCL,
            .sda  = BOARD_CAMERA_I2C_SDA,
        },
        .clk = BOARD_CAMERA_CLK,
    };

    TUYA_CALL_ERR_RETURN(tdd_camera_dvp_gc2145_register(CAMERA_NAME, &camera_cfg)); 
#endif

    return OPRT_OK;
}
#else 
static OPERATE_RET __board_register_camera(void)
{
    return OPRT_OK;
}
#endif


OPERATE_RET board_register_ex_module(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__board_register_display());

    TUYA_CALL_ERR_RETURN(__board_register_camera());

    return rt;
}
