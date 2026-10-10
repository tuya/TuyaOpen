/**
 * @file tuya_t5ai_core.c
 * @brief tuya_t5ai_core module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "tdd_audio.h"
#include "tdd_led_gpio.h"
#include "tdd_button_gpio.h"
#if defined(BOARD_T5AI_CORE_SSD2677) && (BOARD_T5AI_CORE_SSD2677 == 1)
#include "tdd_disp_ssd2677.h"
#endif
/***********************************************************
************************macro define************************
***********************************************************/
#define BOARD_SPEAKER_EN_PIN TUYA_GPIO_NUM_39

#define BOARD_BUTTON_PIN       TUYA_GPIO_NUM_29
#define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

#define BOARD_LED_PIN       TUYA_GPIO_NUM_9
#define BOARD_LED_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH

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
        .mode  = BUTTON_IRQ_MODE,
        .pin_type.irq_edge = TUYA_GPIO_IRQ_FALL,
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

#if defined(BOARD_T5AI_CORE_SSD2677) && (BOARD_T5AI_CORE_SSD2677 == 1)
/** @brief Register the optional external panel selected by the board configuration. */
static OPERATE_RET __board_register_display(void)
{
    const DISP_EINK_SSD2677_CFG_T cfg = {
        .width             = 800,
        .height            = 480,
        .controller_height = 680,
        .port              = TUYA_SPI_NUM_1,
        .spi_clk           = 4000000,
        .clk_pin           = TUYA_GPIO_NUM_2,
        .sda_pin           = TUYA_GPIO_NUM_4,
        .cs_pin            = TUYA_GPIO_NUM_3,
        .dc_pin            = TUYA_GPIO_NUM_7,
        .rst_pin           = TUYA_GPIO_NUM_8,
        .busy_pin          = TUYA_GPIO_NUM_6,
        .power             = {.pin = TUYA_GPIO_NUM_MAX},
    };
    return tdd_disp_spi_mono_ssd2677_register(DISPLAY_NAME, &cfg);
}
#endif

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

    TUYA_CALL_ERR_LOG(__board_register_led());

#if defined(BOARD_T5AI_CORE_SSD2677) && (BOARD_T5AI_CORE_SSD2677 == 1)
    TUYA_CALL_ERR_RETURN(__board_register_display());
#endif

    return rt;
}