#include "tuya_cloud_types.h"
#include "tdd_audio_no_codec.h"
#include "tdd_led_gpio.h"
#include "tdd_button_gpio.h"
#include "board_com_api.h"
#include "tkl_gpio.h"
#include "tal_api.h"

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
OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(AUDIO_CODEC_NAME)
    TDD_AUDIO_NO_CODEC_T cfg = {0};
    cfg.i2s_id = 0;
    cfg.mic_sample_rate = 16000;
    cfg.spk_sample_rate = 16000;

    TUYA_CALL_ERR_RETURN(tdd_audio_no_codec_register(AUDIO_CODEC_NAME, cfg));

    TUYA_GPIO_BASE_CFG_T mic_en = {
        .mode = TUYA_GPIO_PUSH_PULL, .direct = TUYA_GPIO_OUTPUT, .level = TUYA_GPIO_LEVEL_HIGH};
    TUYA_CALL_ERR_LOG(tkl_gpio_init(TUYA_GPIO_NUM_0, &mic_en));
#endif

    return rt;
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(BOARD_SW1_PIN)
    BUTTON_GPIO_CFG_T sw1_hw_cfg = {
        .pin = BOARD_SW1_PIN,
        .level = TUYA_GPIO_LEVEL_LOW,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BOARD_SW1_NAME, &sw1_hw_cfg));
#endif

#if defined(BOARD_SW2_PIN)
    BUTTON_GPIO_CFG_T sw2_hw_cfg = {
        .pin = BOARD_SW2_PIN,
        .level = TUYA_GPIO_LEVEL_LOW,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BOARD_SW2_NAME, &sw2_hw_cfg));
#endif

    return rt;
}

static OPERATE_RET __board_register_led(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(BOARD_LEDB_PIN)
    TDD_LED_GPIO_CFG_T ledb_gpio = {
        .pin = BOARD_LEDB_PIN,
        .level = TUYA_GPIO_LEVEL_LOW,
        .mode = TUYA_GPIO_PUSH_PULL,
    };

    TUYA_CALL_ERR_RETURN(tdd_led_gpio_register(BOARD_LEDB_NAME, &ledb_gpio));
#endif

#if defined(BOARD_LEDR_PIN)
    TDD_LED_GPIO_CFG_T ledr_gpio = {
        .pin = BOARD_LEDR_PIN,
        .level = TUYA_GPIO_LEVEL_LOW,
        .mode = TUYA_GPIO_PUSH_PULL,
    };

    TUYA_CALL_ERR_RETURN(tdd_led_gpio_register(BOARD_LEDR_NAME, &ledr_gpio));
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

    TUYA_CALL_ERR_LOG(__board_register_led());

    return rt;
}