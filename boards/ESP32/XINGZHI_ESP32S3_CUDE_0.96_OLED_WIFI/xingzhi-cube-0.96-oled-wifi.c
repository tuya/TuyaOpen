/**
 * @file xingzhi-cube-0.96-oled-wifi.c
 * @brief Implementation of common board-level hardware registration APIs for peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "tdd_audio_no_codec.h"

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
#include "tdd_button_gpio.h"
#endif

#include "tdd_disp_esp_ssd1306.h"
#include "board_com_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
/* SSD1306 OLED over I2C */
#define OLED_I2C_PORT (0)
#define OLED_I2C_ADDR (0x3C)
#define OLED_I2C_SCL  (42)
#define OLED_I2C_SDA  (41)
#define OLED_WIDTH    (128)
#define OLED_HEIGHT   (64)

#define DISPLAY_WIDTH  OLED_WIDTH
#define DISPLAY_HEIGHT OLED_HEIGHT

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
/*
 * ESP32S3 breadboard typically exposes a BOOT button on GPIO0.
 * If your hardware uses a different GPIO, adjust this macro accordingly.
 */
#ifndef BOARD_BUTTON_PIN
#define BOARD_BUTTON_PIN       TUYA_GPIO_NUM_0
#endif

#ifndef BOARD_BUTTON_ACTIVE_LV
#define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW
#endif
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
int board_display_init(void);

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(AUDIO_CODEC_NAME)
    TDD_AUDIO_NO_CODEC_T cfg = {0};
    cfg.i2s_id = 0;
    cfg.mic_sample_rate = 16000;
    cfg.spk_sample_rate = 16000;

    TUYA_CALL_ERR_RETURN(tdd_audio_no_codec_register(AUDIO_CODEC_NAME, cfg));
#endif

    return rt;
}

static OPERATE_RET __board_register_button(void)
{
#if !defined(ENABLE_BUTTON) || (ENABLE_BUTTON != 1)
    return OPRT_OK;
#else
    OPERATE_RET rt = OPRT_OK;

#if defined(BUTTON_NAME)
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin   = BOARD_BUTTON_PIN,
        .level = BOARD_BUTTON_ACTIVE_LV,
        .mode  = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));
#endif

    return rt;
#endif
}

/**
 * @brief Registers all the hardware peripherals (audio, button, LED) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_CALL_ERR_LOG(__board_register_button());

    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(board_display_init());

    return rt;
}

int board_display_init(void)
{
    TDD_DISP_ESP_LCD_CFG_T cfg = {
        .width     = DISPLAY_WIDTH,
        .height    = DISPLAY_HEIGHT,
        .pixel_fmt = TUYA_PIXEL_FMT_MONOCHROME,
        .rotation  = TUYA_DISPLAY_ROTATION_0,
        .is_swap   = false,
        .bl.type   = TUYA_DISP_BL_TP_NONE,
    };

    OLED_SSD1306_HW_CFG_T hw = {
        .i2c_port     = OLED_I2C_PORT,
        .sda_io       = OLED_I2C_SDA,
        .scl_io       = OLED_I2C_SCL,
        .dev_addr     = OLED_I2C_ADDR,
        .panel_height = OLED_HEIGHT,
    };

    return tdd_disp_esp_ssd1306_register(DISPLAY_NAME, &hw, &cfg);
}
