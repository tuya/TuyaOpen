/**
 * @file tuya_t5ai_pixel.c
 * @brief tuya_t5ai_pixel module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include <string.h>

#include "tdd_audio.h"
#include "tdd_led_gpio.h"
#include "tdd_button_gpio.h"
#include "board_buzzer_api.h"
#include "board_bmi270_api.h"

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
#include "tkl_pinmux.h"
#include "tdd_pixel_ws2812.h"
#include "tdd_pixel_type.h"
#include "tdl_pixel_dev_manage.h"
#endif
/***********************************************************
************************macro define************************
***********************************************************/
#define BOARD_SPEAKER_EN_PIN TUYA_GPIO_NUM_42

#define BOARD_BUTTON_OK_PIN       TUYA_GPIO_NUM_44
#define BOARD_BUTTON_OK_ACTIVE_LV TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON_A_PIN        TUYA_GPIO_NUM_45
#define BOARD_BUTTON_A_ACTIVE_LV  TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON_B_PIN        TUYA_GPIO_NUM_46
#define BOARD_BUTTON_B_ACTIVE_LV  TUYA_GPIO_LEVEL_LOW

#define BOARD_LED_PIN       TUYA_GPIO_NUM_47
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
    BUTTON_GPIO_CFG_T button_hw_cfg;

    memset(&button_hw_cfg, 0, sizeof(BUTTON_GPIO_CFG_T));

    // register OK button
    button_hw_cfg.pin = BOARD_BUTTON_OK_PIN;
    button_hw_cfg.level = BOARD_BUTTON_OK_ACTIVE_LV;
    button_hw_cfg.mode = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));

    // register A button
    button_hw_cfg.pin = BOARD_BUTTON_A_PIN;
    button_hw_cfg.level = BOARD_BUTTON_A_ACTIVE_LV;
    button_hw_cfg.mode = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_2, &button_hw_cfg));

    // register B button
    button_hw_cfg.pin = BOARD_BUTTON_B_PIN;
    button_hw_cfg.level = BOARD_BUTTON_B_ACTIVE_LV;
    button_hw_cfg.mode = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_3, &button_hw_cfg));

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

static OPERATE_RET __board_register_buzzer(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Initialize buzzer driver
    TUYA_CALL_ERR_RETURN(board_buzzer_init());

    return rt;
}

static OPERATE_RET __board_register_pixel_led(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
    char device_name[32] = "pixel";
#if defined(PIXEL_DEVICE_NAME)
    strncpy(device_name, PIXEL_DEVICE_NAME, sizeof(device_name) - 1);
    device_name[sizeof(device_name) - 1] = '\0';
#endif

    PIXEL_DRIVER_CONFIG_T dev_init_cfg = {
        .port = TUYA_SPI_NUM_0,
        .line_seq = RGB_ORDER,
    };

    rt = tdd_ws2812_driver_register(device_name, &dev_init_cfg);
    if (OPRT_OK == rt) {
        PR_NOTICE("Pixel LED driver registered: %s", device_name);
    } else {
        PR_ERR("Failed to register pixel LED driver '%s': %d", device_name, rt);
    }
#endif
    return rt;
}

// static OPERATE_RET __board_register_bmi270_sensor(void)
// {
//     OPERATE_RET rt = OPRT_OK;

//     // Register BMI270 sensor
//     rt = board_bmi270_register();
//     if (OPRT_OK != rt) {
//         PR_ERR("BMI270 sensor registration failed: %d", rt);
//         return rt;
//     }

//     return rt;
// }

/**
 * @brief Registers all the hardware peripherals (audio, button, LED, buzzer, pixel LED) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_audio());

    TUYA_CALL_ERR_LOG(__board_register_button());

    TUYA_CALL_ERR_LOG(__board_register_led());

    TUYA_CALL_ERR_LOG(__board_register_buzzer());

    TUYA_CALL_ERR_LOG(__board_register_pixel_led());

    // TUYA_CALL_ERR_LOG(__board_register_bmi270_sensor());

    return rt;
}
