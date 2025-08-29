/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for audio, button, and LED peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tdd_audio.h"
#include "tdd_button_gpio.h"

#include "tdd_disp_co5300.h"
#include "tdd_touch_cst92xx.h"

/***********************************************************
***********************macro define***********************
***********************************************************/
#define BOARD_SPEAKER_EN_PIN TUYA_GPIO_NUM_28

#define BOARD_BUTTON_PIN       TUYA_GPIO_NUM_12
#define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

#define BOARD_LCD_RST_PIN   TUYA_GPIO_NUM_29
#define BOARD_LCD_QSPI_PORT TUYA_QSPI_NUM_0
#define BOARD_LCD_QSPI_CLK  (80 * 1000000)

#define BOARD_LCD_BL_TYPE TUYA_DISP_BL_TP_NONE

#define BOARD_LCD_POWER_PIN TUYA_GPIO_NUM_MAX

#define BOARD_LCD_WIDTH      466
#define BOARD_LCD_HEIGHT     466
#define BOARD_LCD_PIXELS_FMT TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION   TUYA_DISPLAY_ROTATION_0

#define BOARD_TOUCH_I2C_PORT    TUYA_I2C_NUM_0
#define BOARD_TOUCH_I2C_SCL_PIN TUYA_GPIO_NUM_20
#define BOARD_TOUCH_I2C_SDA_PIN TUYA_GPIO_NUM_21

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(AUDIO_CODEC_NAME)
    TDD_AUDIO_T5AI_T cfg = {0};
    memset(&cfg, 0, sizeof(TDD_AUDIO_T5AI_T));

    cfg.aec_enable = 1;

    cfg.ai_chn = TKL_AI_0;
    cfg.sample_rate = TKL_AUDIO_SAMPLE_16K;
    cfg.data_bits = TKL_AUDIO_DATABITS_16;
    cfg.channel = TKL_AUDIO_CHANNEL_MONO;

    cfg.spk_sample_rate = TKL_AUDIO_SAMPLE_16K;
    cfg.spk_pin = BOARD_SPEAKER_EN_PIN;
    cfg.spk_pin_polarity = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_RETURN(tdd_audio_register(AUDIO_CODEC_NAME, cfg));
#endif
    return rt;
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(BUTTON_NAME)
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin = BOARD_BUTTON_PIN,
        .level = BOARD_BUTTON_ACTIVE_LV,
        .mode = BUTTON_IRQ_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));
#endif

    return rt;
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    DISP_QSPI_DEVICE_CFG_T display_cfg;

    memset(&display_cfg, 0, sizeof(DISP_QSPI_DEVICE_CFG_T));

    display_cfg.bl.type = BOARD_LCD_BL_TYPE;

    display_cfg.width = BOARD_LCD_WIDTH;
    display_cfg.height = BOARD_LCD_HEIGHT;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation = BOARD_LCD_ROTATION;

    display_cfg.port = BOARD_LCD_QSPI_PORT;
    display_cfg.spi_clk = BOARD_LCD_QSPI_CLK;
    display_cfg.rst_pin = BOARD_LCD_RST_PIN;

    display_cfg.power.pin = BOARD_LCD_POWER_PIN;

    TUYA_CALL_ERR_RETURN(tdd_disp_qspi_co5300_register(DISPLAY_NAME, &display_cfg));

    TDD_TOUCH_CST92XX_INFO_T cst92xx_info = {
        .i2c_cfg =
            {
                .port = BOARD_TOUCH_I2C_PORT,
                .scl_pin = BOARD_TOUCH_I2C_SCL_PIN,
                .sda_pin = BOARD_TOUCH_I2C_SDA_PIN,
            },
        .tp_cfg =
            {
                .x_max = BOARD_LCD_WIDTH,
                .y_max = BOARD_LCD_HEIGHT,
                .flags =
                    {
                        .mirror_x = 1,
                        .mirror_y = 1,
                        .swap_xy = 0,
                    },
            },
    };

    TUYA_CALL_ERR_RETURN(tdd_touch_i2c_cst92xx_register(DISPLAY_NAME, &cst92xx_info));
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

    TUYA_CALL_ERR_LOG(__board_register_audio());

    TUYA_CALL_ERR_LOG(__board_register_button());

    TUYA_CALL_ERR_LOG(__board_register_display());

    return rt;
}
