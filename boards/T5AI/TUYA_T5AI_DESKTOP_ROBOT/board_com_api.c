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

#include "tdd_audio.h"
#include "tdd_button_gpio.h"
#include "tdl_button_manage.h"
#include "tdd_disp_st7789.h"
#include "tdd_tp_cst816x.h"
#include "tdd_camera_gc2145.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define BOARD_POWER_PIN              TUYA_GPIO_NUM_4
#define BOARD_POWER_ACTIVE_LV        TUYA_GPIO_LEVEL_HIGH

#define BOARD_PERI_POWER_PIN         TUYA_GPIO_NUM_5
#define BOARD_PERI_POWER_ACTIVE_LV   TUYA_GPIO_LEVEL_HIGH

#define BOARD_SPEAKER_EN_PIN         TUYA_GPIO_NUM_26

#define BOARD_POWER_BUTTON_NAME      "pwr_button"
#define BOARD_POWER_BUTTON_PIN       TUYA_GPIO_NUM_3
#define BOARD_POWER_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

#define BOARD_BUTTON_PIN             TUYA_GPIO_NUM_28
#define BOARD_BUTTON_ACTIVE_LV       TUYA_GPIO_LEVEL_LOW

#define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_GPIO 
#define BOARD_LCD_BL_PIN             TUYA_GPIO_NUM_42
#define BOARD_LCD_BL_ACTIVE_LV       TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_WIDTH              240
#define BOARD_LCD_HEIGHT             320
#define BOARD_LCD_PIXELS_FMT         TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION           TUYA_DISPLAY_ROTATION_90

#define BOARD_LCD_SPI_PORT           TUYA_SPI_NUM_0
#define BOARD_LCD_SPI_CLK            48000000
#define BOARD_LCD_SPI_CS_PIN         TUYA_GPIO_NUM_45
#define BOARD_LCD_SPI_DC_PIN         TUYA_GPIO_NUM_47
#define BOARD_LCD_SPI_RST_PIN        TUYA_GPIO_NUM_43
#define BOARD_LCD_SPI_MISO_PIN       TUYA_GPIO_NUM_46
#define BOARD_LCD_SPI_CLK_PIN        TUYA_GPIO_NUM_44

#define BOARD_LCD_PIXELS_FMT         TUYA_PIXEL_FMT_RGB565

#define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_MAX
#define BOARD_LCD_POWER_ACTIVE_LV    TUYA_GPIO_LEVEL_HIGH

#define BOARD_TP_I2C_PORT            TUYA_I2C_NUM_0
#define BOARD_TP_I2C_SCL_PIN         TUYA_GPIO_NUM_20
#define BOARD_TP_I2C_SDA_PIN         TUYA_GPIO_NUM_21
#define BOARD_TP_RST_PIN             TUYA_GPIO_NUM_53
#define BOARD_TP_INTR_PIN            TUYA_GPIO_NUM_MAX

#define BOARD_CAMERA_I2C_PORT        TUYA_I2C_NUM_0
#define BOARD_CAMERA_I2C_SCL         TUYA_GPIO_NUM_20
#define BOARD_CAMERA_I2C_SDA         TUYA_GPIO_NUM_21

#define BOARD_CAMERA_RST_PIN         TUYA_GPIO_NUM_50
#define BOARD_CAMERA_RST_ACTIVE_LV   TUYA_GPIO_LEVEL_LOW

#define BOARD_CAMERA_POWER_PIN       TUYA_GPIO_NUM_49
#define BOARD_CAMERA_PWR_ACTIVE_LV   TUYA_GPIO_LEVEL_LOW

#define BOARD_CAMERA_CLK             24000000
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

#if defined(ENABLE_AUDIO_AEC) && (ENABLE_AUDIO_AEC == 1)
    cfg.aec_enable = 1;
#else
    cfg.aec_enable = 0;
#endif

    cfg.ai_chn      = TKL_AI_0;
    cfg.sample_rate = TKL_AUDIO_SAMPLE_16K;
    cfg.data_bits   = TKL_AUDIO_DATABITS_16;
    cfg.channel     = TKL_AUDIO_CHANNEL_MONO;

    cfg.spk_sample_rate  = TKL_AUDIO_SAMPLE_16K;
    cfg.spk_pin          = BOARD_SPEAKER_EN_PIN;
    cfg.spk_pin_polarity = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_RETURN(tdd_audio_register(AUDIO_CODEC_NAME, cfg));
#endif
    return rt;
}

static void __pwr_button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    switch (event) {
    case TDL_BUTTON_LONG_PRESS_START: {
        PR_NOTICE("%s: long press", name);
        tkl_gpio_write(BOARD_POWER_PIN, !BOARD_POWER_ACTIVE_LV);
    } break;

    default:
        break;
    }
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

    BUTTON_GPIO_CFG_T pwr_button_hw_cfg = {
        .pin   = BOARD_POWER_BUTTON_PIN,
        .level = BOARD_POWER_BUTTON_ACTIVE_LV,
        .mode  = BUTTON_IRQ_MODE,
        .pin_type.irq_edge = TUYA_GPIO_IRQ_FALL,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BOARD_POWER_BUTTON_NAME, &pwr_button_hw_cfg));

    // button create
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 500};
    TDL_BUTTON_HANDLE button_hdl = NULL;

    TUYA_CALL_ERR_RETURN(tdl_button_create(BOARD_POWER_BUTTON_NAME, &button_cfg, &button_hdl));
    tdl_button_event_register(button_hdl, TDL_BUTTON_LONG_PRESS_START, __pwr_button_function_cb);


#if defined(BUTTON_NAME)
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin   = BOARD_BUTTON_PIN,
        .level = BOARD_BUTTON_ACTIVE_LV,
        .mode  = BUTTON_IRQ_MODE,
        .pin_type.irq_edge = TUYA_GPIO_IRQ_FALL,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));
#endif

    return rt;
}


static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    // Composite Pinout from chip internal, muxing set the actual pinout for SPI0 interface
    if(BOARD_LCD_SPI_CLK_PIN == TUYA_GPIO_NUM_44) {
        tkl_io_pinmux_config(TUYA_GPIO_NUM_45, TUYA_SPI0_CS);
        tkl_io_pinmux_config(TUYA_GPIO_NUM_44, TUYA_SPI0_CLK);
        tkl_io_pinmux_config(TUYA_GPIO_NUM_46, TUYA_SPI0_MOSI);
        tkl_io_pinmux_config(TUYA_GPIO_NUM_47, TUYA_SPI0_MISO);
    }

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

static OPERATE_RET __board_register_camera(void)
{
#if defined(CAMERA_NAME)
    OPERATE_RET rt = OPRT_OK;
    TDD_DVP_SR_USR_CFG_T camera_cfg = {
        .pwr = {
            .pin = BOARD_CAMERA_POWER_PIN,
            .active_level = BOARD_CAMERA_PWR_ACTIVE_LV,
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
    gpio_cfg.level = BOARD_POWER_ACTIVE_LV;
    TUYA_CALL_ERR_LOG(tkl_gpio_init(BOARD_POWER_PIN, &gpio_cfg));

    gpio_cfg.level = BOARD_PERI_POWER_ACTIVE_LV;
    TUYA_CALL_ERR_LOG(tkl_gpio_init(BOARD_PERI_POWER_PIN, &gpio_cfg));

    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(__board_register_button());
    TUYA_CALL_ERR_LOG(__board_register_display());
    TUYA_CALL_ERR_LOG(__board_register_camera());

    return rt;
}
