/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for audio, button, and LED peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tkl_gpio.h"
#include "tal_api.h"

#include "tdd_audio.h"
#include "tdd_button_gpio.h"
#include "tdl_button_manage.h"

#include "tdd_disp_co5300.h"
#include "tdd_tp_cst92xx.h"

/***********************************************************
***********************macro define***********************
***********************************************************/
#define BOARD_PWR_EN_PIN           TUYA_GPIO_NUM_19
#define BOARD_PWR_EN_ACTIVE_LV     TUYA_GPIO_LEVEL_HIGH

#define BOARD_BUTTON_PWR_PIN       TUYA_GPIO_NUM_18
#define BOARD_BUTTON_PWR_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

#define BOARD_SPEAKER_EN_PIN       TUYA_GPIO_NUM_28
   
#define BOARD_BUTTON_PIN           TUYA_GPIO_NUM_12
#define BOARD_BUTTON_ACTIVE_LV     TUYA_GPIO_LEVEL_LOW
   
#define BOARD_LCD_RST_PIN          TUYA_GPIO_NUM_29
#define BOARD_LCD_QSPI_PORT        TUYA_QSPI_NUM_0
#define BOARD_LCD_QSPI_CLK         (80 * 1000000)

#define BOARD_LCD_POWER_PIN        TUYA_GPIO_NUM_MAX
   
#define BOARD_LCD_WIDTH            466
#define BOARD_LCD_HEIGHT           466
#define BOARD_LCD_X_OFFSET         6
#define BOARD_LCD_Y_OFFSET         0
#define BOARD_LCD_PIXELS_FMT       TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION         TUYA_DISPLAY_ROTATION_0
   
#define BOARD_TP_I2C_PORT          TUYA_I2C_NUM_0
#define BOARD_TP_I2C_SCL_PIN       TUYA_GPIO_NUM_20
#define BOARD_TP_I2C_SDA_PIN       TUYA_GPIO_NUM_21
#define BOARD_TP_RST_PIN           TUYA_GPIO_NUM_42
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

static void __button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    switch (event) {
    case TDL_BUTTON_PRESS_DOWN: {
        PR_NOTICE("%s: single click", name);
    } break;

    case TDL_BUTTON_LONG_PRESS_START: {
        PR_NOTICE("%s: long press", name);
        tkl_gpio_write(BOARD_PWR_EN_PIN, TUYA_GPIO_LEVEL_LOW);
    } break;

    default:
        break;
    }
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(BUTTON_NAME)
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin = BOARD_BUTTON_PIN,
        .level = BOARD_BUTTON_ACTIVE_LV,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));
#endif

#if defined(BUTTON_NAME_2)
    TUYA_GPIO_BASE_CFG_T pin_cfg;
    pin_cfg.mode = TUYA_GPIO_PUSH_PULL;
    pin_cfg.direct = TUYA_GPIO_OUTPUT;
    pin_cfg.level = BOARD_PWR_EN_ACTIVE_LV;
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(BOARD_PWR_EN_PIN, &pin_cfg));

    BUTTON_GPIO_CFG_T button_2_hw_cfg = {
        .pin = BOARD_BUTTON_PWR_PIN,
        .level = BOARD_BUTTON_PWR_ACTIVE_LV,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_2, &button_2_hw_cfg));
    // button create
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 500};
    TDL_BUTTON_HANDLE button_hdl = NULL;

    TUYA_CALL_ERR_RETURN(tdl_button_create(BUTTON_NAME_2, &button_cfg, &button_hdl));
    tdl_button_event_register(button_hdl, TDL_BUTTON_PRESS_DOWN, __button_function_cb);
    tdl_button_event_register(button_hdl, TDL_BUTTON_LONG_PRESS_START, __button_function_cb);

#endif

    return rt;
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    DISP_QSPI_DEVICE_CFG_T display_cfg;

    memset(&display_cfg, 0, sizeof(DISP_QSPI_DEVICE_CFG_T));

    display_cfg.bl.type = TUYA_DISP_BL_TP_CUSTOM;

    display_cfg.width     = BOARD_LCD_WIDTH;
    display_cfg.height    = BOARD_LCD_HEIGHT;
    display_cfg.x_offset  = BOARD_LCD_X_OFFSET;
    display_cfg.y_offset  = BOARD_LCD_Y_OFFSET;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation  = BOARD_LCD_ROTATION;

    display_cfg.port = BOARD_LCD_QSPI_PORT;
    display_cfg.spi_clk = BOARD_LCD_QSPI_CLK;
    display_cfg.rst_pin = BOARD_LCD_RST_PIN;

    display_cfg.power.pin = BOARD_LCD_POWER_PIN;

    TUYA_CALL_ERR_RETURN(tdd_disp_qspi_co5300_register(DISPLAY_NAME, &display_cfg));

    TUYA_CALL_ERR_RETURN(tdl_disp_custom_backlight_register(DISPLAY_NAME,\
                                                            tdd_qspi_co5300_send_cmd_set_bl,\
                                                            NULL));

    TDD_TP_CST92XX_INFO_T cst92xx_info = {
        .rst_pin = BOARD_TP_RST_PIN,
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
                        .mirror_x = 1,
                        .mirror_y = 1,
                        .swap_xy = 0,
                    },
            },
    };

    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_cst92xx_register(DISPLAY_NAME, &cst92xx_info));
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
