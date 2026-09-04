/**
 * @file nicemcu_t5_dev.c
 * @brief NiceMCU-T5-DEV board: T5 pin breakout (optional I2S audio add-on)
 * @version 0.2
 * @date 2026-08-18
 * @copyright Copyright (c) Tuya Inc. All Rights Reserved.
 */
#include "board_com_api.h"
#include "board_config.h"

#include "tal_api.h"
#include "tdd_button_gpio.h"

#if defined(NICEMCU_T5_DEV_I2S_AUDIO) && (NICEMCU_T5_DEV_I2S_AUDIO == 1)
#include "tdd_audio_i2s_ext.h"
#endif

#include <string.h>

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Register INMP441 mic + MAX98357 amp over I2S1 (optional add-on)
 * @return OPRT_OK on success
 */
static OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(NICEMCU_T5_DEV_I2S_AUDIO) && (NICEMCU_T5_DEV_I2S_AUDIO == 1)
#if defined(AUDIO_CODEC_NAME)
    TDD_AUDIO_I2S_EXT_T cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mic_sample_rate = 16000;
    cfg.spk_sample_rate = 16000;
    cfg.sd_pin = BOARD_SPK_SD_PIN;
    cfg.sd_pin_polarity = BOARD_SPK_SD_ACTIVE_LV;

    TUYA_CALL_ERR_RETURN(tdd_audio_i2s_ext_register(AUDIO_CODEC_NAME, cfg));
    PR_NOTICE("NICEMCU_T5_DEV: I2S duplex registered (mic P42 + spk P43, clk P40/P41)");
#endif
#else
    PR_NOTICE("NICEMCU_T5_DEV: I2S audio add-on disabled");
#endif

    return rt;
}

/**
 * @brief Register user button on P0 (active-low)
 * @return OPRT_OK on success
 */
static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(BUTTON_NAME)
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin = BOARD_BUTTON_PIN,
        .level = BOARD_BUTTON_ACTIVE_LV,
        .mode = BUTTON_IRQ_MODE,
        .pin_type.irq_edge = TUYA_GPIO_IRQ_FALL,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));
    PR_NOTICE("NICEMCU_T5_DEV: button on P%d registered", BOARD_BUTTON_PIN);
#endif

    return rt;
}

/**
 * @brief Register board peripherals
 * @return OPRT_OK on success
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(__board_register_button());

    return rt;
}
