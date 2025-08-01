/**
 * @file tuya_t5ai_pocket.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for audio, button, LED, and LCD display
 * peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tkl_pinmux.h"
#include "tal_api.h"
#include "tdd_audio.h"
#include "tdd_led_gpio.h"
#include "tdd_button_gpio.h"
#include "tdd_disp_st7305.h"     // Add LCD driver header
#include "board_audio_mux_api.h" // Add audio mux API header
#include "board_bmi270_api.h"    // Add BMI270 sensor API header
#include "board_axp2101_api.h"   // Add AXP2101 power management API header

/***********************************************************
************************macro define************************
***********************************************************/
#define BOARD_SPEAKER_EN_PIN TUYA_GPIO_NUM_42

#define BOARD_BUTTON_PIN TUYA_GPIO_NUM_26

#define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

#define BOARD_LED_PIN       TUYA_GPIO_NUM_28
#define BOARD_LED_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH

// Audio Mux Router: Controls signal routing to the MIC2 input of the codec.
//   - SEL = Low  : Route 1 (Microphone input to MIC2)
//   - SEL = High : Route 2 (Speaker loopback to MIC2)
// The SEL pin determines whether MIC2 receives audio from the microphone or from the speaker loopback.
#define BOARD_AUDIO_MUX_SEL_PIN         TUYA_GPIO_NUM_23
#define BOARD_AUDIO_MUX_SEL_MIC_LV      TUYA_GPIO_LEVEL_LOW
#define BOARD_AUDIO_MUX_SEL_LOOPBACK_LV TUYA_GPIO_LEVEL_HIGH

#if defined(TUYA_T5AI_POCKET_LCD)
#define BOARD_LCD_BL_TYPE TUYA_DISP_BL_TP_NONE

#define BOARD_LCD_WIDTH    168
#define BOARD_LCD_HEIGHT   384
#define BOARD_LCD_ROTATION TUYA_DISPLAY_ROTATION_0
#define BOARD_LCD_CASET_XS 0x17

#define BOARD_LCD_SPI_PORT     TUYA_SPI_NUM_0
#define BOARD_LCD_SPI_CLK      48000000
#define BOARD_LCD_SPI_CS_PIN   TUYA_GPIO_NUM_45
#define BOARD_LCD_SPI_DC_PIN   TUYA_GPIO_NUM_47
#define BOARD_LCD_SPI_RST_PIN  TUYA_GPIO_NUM_43
#define BOARD_LCD_SPI_MISO_PIN TUYA_GPIO_NUM_46

#define BOARD_LCD_POWER_PIN TUYA_GPIO_NUM_MAX
#endif

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
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));
#endif

    return rt;
}

static OPERATE_RET __board_register_led(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(LED_NAME)
    TDD_LED_GPIO_CFG_T led_gpio;

    led_gpio.pin = BOARD_LED_PIN;
    led_gpio.level = BOARD_LED_ACTIVE_LV;
    led_gpio.mode = TUYA_GPIO_PUSH_PULL;

    TUYA_CALL_ERR_RETURN(tdd_led_gpio_register(LED_NAME, &led_gpio));
#endif

    return rt;
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)

    // Composite Pinout from chip internal, muxing set the actual pinout for SPI0 interface
    tkl_io_pinmux_config(BOARD_LCD_SPI_CS_PIN, TUYA_SPI0_CS);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_44, TUYA_SPI0_CLK);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_46, TUYA_SPI0_MOSI);

    DISP_SPI_DEVICE_CFG_T display_cfg;

    memset(&display_cfg, 0, sizeof(DISP_SPI_DEVICE_CFG_T));

    display_cfg.bl.type = BOARD_LCD_BL_TYPE;

    display_cfg.width = BOARD_LCD_WIDTH;
    display_cfg.height = BOARD_LCD_HEIGHT;
    display_cfg.rotation = BOARD_LCD_ROTATION;

    display_cfg.port = BOARD_LCD_SPI_PORT;
    display_cfg.spi_clk = BOARD_LCD_SPI_CLK;
    display_cfg.cs_pin = BOARD_LCD_SPI_CS_PIN;
    display_cfg.dc_pin = BOARD_LCD_SPI_DC_PIN;
    display_cfg.rst_pin = BOARD_LCD_SPI_RST_PIN;

    display_cfg.power.pin = BOARD_LCD_POWER_PIN;

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_mono_st7305_register(DISPLAY_NAME, &display_cfg, BOARD_LCD_CASET_XS));
#endif

    return rt;
}

static OPERATE_RET __board_register_audio_mux_router(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Initialize the audio multiplexer with default microphone route
    rt = board_audio_mux_init();
    if (OPRT_OK != rt) {
        return rt;
    }

    return rt;
}

static OPERATE_RET __board_register_bmi270_sensor(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Register BMI270 sensor
    rt = board_bmi270_register();
    if (OPRT_OK != rt) {
        PR_ERR("BMI270 sensor registration failed: %d", rt);
        return rt;
    }

    return rt;
}

static OPERATE_RET __board_register_axp2101(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Initialize AXP2101 power management IC
    rt = board_axp2101_init();
    if (OPRT_OK != rt) {
        PR_ERR("AXP2101 initialization failed: %d", rt);
        return rt;
    }

    return rt;
}

/**
 * @brief Registers all the hardware peripherals (audio, button, LED, display) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_axp2101());
    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(__board_register_button());
    TUYA_CALL_ERR_LOG(__board_register_led());
    TUYA_CALL_ERR_LOG(__board_register_display());
    TUYA_CALL_ERR_LOG(__board_register_audio_mux_router());
    TUYA_CALL_ERR_LOG(__board_register_bmi270_sensor());

    return rt;
}