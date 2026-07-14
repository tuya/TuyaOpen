/**
 * @file lckfb_t5ai_ex_module.c
 * @version 0.1
 * @date 2025-07-01
 */

#include "tal_api.h"
#include "tkl_pinmux.h"
#include "tkl_gpio.h"
#include "tkl_i2c.h"

#include "lckfb_t5ai_ex_module.h"

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
**************************************************************/

/* ---- LCD ---- */
#if defined (LCKFB_T5AI_TOUCH_LCD_ST7789_240X320) && (LCKFB_T5AI_TOUCH_LCD_ST7789_240X320 ==1)
static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    DISP_SPI_DEVICE_CFG_T display_cfg;

    tkl_io_pinmux_config(BOARD_TP_I2C_SCL_PIN, TUYA_IIC1_SCL);
    tkl_io_pinmux_config(BOARD_TP_I2C_SDA_PIN, TUYA_IIC1_SDA);

    memset(&display_cfg, 0, sizeof(DISP_SPI_DEVICE_CFG_T));

    display_cfg.bl.type                   = BOARD_LCD_BL_TYPE;
    display_cfg.bl.pwm.id                 = BOARD_LCD_BL_PWM_ID;
    display_cfg.bl.pwm.cfg.frequency      = BOARD_LCD_BL_PWM_FREQ;
    display_cfg.bl.pwm.cfg.duty           = BOARD_LCD_BL_PWM_CYCLE;
    display_cfg.bl.pwm.cfg.cycle          = BOARD_LCD_BL_PWM_CYCLE;
    display_cfg.bl.pwm.cfg.polarity       = TUYA_PWM_POSITIVE;
    display_cfg.bl.pwm.cfg.count_mode     = TUYA_PWM_CNT_UP;

    display_cfg.width     = BOARD_LCD_WIDTH;
    display_cfg.height    = BOARD_LCD_HEIGHT;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation  = BOARD_LCD_ROTATION;

    display_cfg.port    = BOARD_LCD_SPI_PORT;
    display_cfg.spi_clk = BOARD_LCD_SPI_CLK;
    display_cfg.cs_pin  = BOARD_LCD_SPI_CS_PIN;
    display_cfg.dc_pin  = BOARD_LCD_SPI_DC_PIN;
    display_cfg.rst_pin = BOARD_LCD_SPI_RST_PIN;

    display_cfg.power.pin          = BOARD_LCD_POWER_PIN;
    display_cfg.power.active_level = BOARD_LCD_POWER_ACTIVE_LV;

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_st7789_register(DISPLAY_NAME, &display_cfg));

#if !defined(EXAMPLE_DISABLE_TOUCH) || (EXAMPLE_DISABLE_TOUCH == 0)
    TDD_TP_FT6336_INFO_T tp_cfg = {
        .rst_pin  = BOARD_TP_RST_PIN,
        .intr_pin = BOARD_TP_INT_PIN,
        .i2c_cfg =
            {
                .port    = BOARD_TP_I2C_PORT,
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
                        .swap_xy  = 0,
                    },
            },
    };

    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_ft6336_register(DISPLAY_NAME, &tp_cfg));
#else
    PR_NOTICE("[LCKFB_TP] touch panel is disabled by EXAMPLE_DISABLE_TOUCH");
#endif
#endif

    return rt;
}
#else
static OPERATE_RET __board_register_display(void)
{
    return OPRT_OK;
}
#endif


/* ---- Camera ---- */
#if defined (LCKFB_T5AI_TOUCH_CAMERA) && (LCKFB_T5AI_TOUCH_CAMERA ==1)
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
        .i2c = {
            .port = BOARD_CAMERA_I2C_PORT,
            .clk  = BOARD_CAMERA_I2C_SCL,
            .sda  = BOARD_CAMERA_I2C_SDA,
        },
        .clk = BOARD_CAMERA_CLK,
    };

    tkl_io_pinmux_config(BOARD_CAMERA_I2C_SCL, TUYA_IIC1_SCL);
    tkl_io_pinmux_config(BOARD_CAMERA_I2C_SDA, TUYA_IIC1_SDA);

    TUYA_CALL_ERR_RETURN(tdd_camera_dvp_gc0308_register(CAMERA_NAME, &camera_cfg));
#endif

    return OPRT_OK;
}
#else
static OPERATE_RET __board_register_camera(void)
{
    return OPRT_OK;
}
#endif


/* ---- SC7A20 IMU sensor ---- */
#if defined(LCKFB_T5AI_TOUCH_SC7A20) && (LCKFB_T5AI_TOUCH_SC7A20 == 1)
static TDD_SC7A20_CFG_T s_sc7a20_cfg;

static OPERATE_RET __board_register_sc7a20(void)
{
    s_sc7a20_cfg.i2c_port  = BOARD_SC7A20_I2C_PORT;
    s_sc7a20_cfg.i2c_addr  = SC7A20_I2C_ADDR_SA0_HIGH;
    s_sc7a20_cfg.range     = SC7A20_RANGE_2G;
    s_sc7a20_cfg.odr       = SC7A20_ODR_100HZ;
    s_sc7a20_cfg.low_power = false;
    s_sc7a20_cfg.hpf_data  = false;

    tkl_io_pinmux_config(BOARD_SC7A20_I2C_SCL_PIN, TUYA_IIC1_SCL);
    tkl_io_pinmux_config(BOARD_SC7A20_I2C_SDA_PIN, TUYA_IIC1_SDA);

    TUYA_CALL_ERR_RETURN(tdd_sc7a20_init(&s_sc7a20_cfg));

    PR_NOTICE("[SC7A20] initialized");
    return OPRT_OK;
}
#else
static OPERATE_RET __board_register_sc7a20(void)
{
    return OPRT_OK;
}
#endif


/* ---- SDIO ---- */
#if defined (LCKFB_T5AI_TOUCH_SDIO) && (LCKFB_T5AI_TOUCH_SDIO ==1)
static OPERATE_RET __board_sdio_pin_register(void)
{
    tkl_io_pinmux_config(TUYA_GPIO_NUM_14, TUYA_SDIO_CLK);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_15, TUYA_SDIO_CMD);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_16, TUYA_SDIO_DATA0);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_17, TUYA_SDIO_DATA1);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_18, TUYA_SDIO_DATA2);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_19, TUYA_SDIO_DATA3);

    return OPRT_OK;
}
#else
static OPERATE_RET __board_sdio_pin_register(void)
{
    return OPRT_OK;
}
#endif


OPERATE_RET board_register_ex_module(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__board_register_display());

    TUYA_CALL_ERR_RETURN(__board_register_camera());

    TUYA_CALL_ERR_RETURN(__board_sdio_pin_register());

    TUYA_CALL_ERR_RETURN(__board_register_sc7a20());

    return rt;
}
