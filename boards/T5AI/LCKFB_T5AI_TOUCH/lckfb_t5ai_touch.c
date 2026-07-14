/**
 * @file lckfb_t5ai_touch.c
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

#include "board_com_api.h"
#include "lckfb_t5ai_ex_module.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define BOARD_SPEAKER_EN_PIN         TUYA_GPIO_NUM_23

#define BOARD_BUTTON_PIN             TUYA_GPIO_NUM_7
#define BOARD_BUTTON_ACTIVE_LV       TUYA_GPIO_LEVEL_LOW

#define BOARD_EXPORT_IO_PWR_PIN      TUYA_GPIO_NUM_6
#define BOARD_PALY_PWR_PIN           TUYA_GPIO_NUM_8
#define BOARD_DVP_PWR_PIN            TUYA_GPIO_NUM_9
#define BOARD_LCD_PWR_PIN            TUYA_GPIO_NUM_13
#define BOARD_SENSOR_PWR_PIN         TUYA_GPIO_NUM_24

#define BOARD_PWR_ACTIVE_LV          TUYA_GPIO_LEVEL_HIGH

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
static OPERATE_RET __board_pwr_gpio_init(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E active_level)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T cfg = {0};

    tkl_io_pinmux_config(pin, TUYA_GPIO);

    cfg.mode   = TUYA_GPIO_PUSH_PULL;
    cfg.direct = TUYA_GPIO_OUTPUT;
    cfg.level  = active_level;

    TUYA_CALL_ERR_RETURN(tkl_gpio_init(pin, &cfg));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(pin, active_level));

    return OPRT_OK;
}

static OPERATE_RET __board_power_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__board_pwr_gpio_init(BOARD_EXPORT_IO_PWR_PIN, BOARD_PWR_ACTIVE_LV));
    TUYA_CALL_ERR_RETURN(__board_pwr_gpio_init(BOARD_PALY_PWR_PIN, BOARD_PWR_ACTIVE_LV));
    TUYA_CALL_ERR_RETURN(__board_pwr_gpio_init(BOARD_LCD_PWR_PIN, BOARD_PWR_ACTIVE_LV));
    TUYA_CALL_ERR_RETURN(__board_pwr_gpio_init(BOARD_SENSOR_PWR_PIN, BOARD_PWR_ACTIVE_LV));

#if defined(LCKFB_T5AI_TOUCH_CAMERA) && (LCKFB_T5AI_TOUCH_CAMERA == 1)
    tal_system_sleep(BOARD_CAMERA_SENSOR_PWR_MS);
    TUYA_CALL_ERR_RETURN(__board_pwr_gpio_init(BOARD_DVP_PWR_PIN, BOARD_DVP_PWR_ON_LV));
    PR_NOTICE("DVP PWR pin:%d lv:%d", BOARD_DVP_PWR_PIN, BOARD_DVP_PWR_ON_LV);
    /* GC0308 PWDN: pull LOW to exit power-down mode */
    TUYA_CALL_ERR_RETURN(__board_pwr_gpio_init(BOARD_CAMERA_PWDN_PIN, TUYA_GPIO_LEVEL_LOW));
    PR_NOTICE("CAM PWDN pin:%d lv:0 (active)", BOARD_CAMERA_PWDN_PIN);
    tal_system_sleep(BOARD_CAMERA_PWR_SETTLE_MS);
#else
    TUYA_CALL_ERR_RETURN(__board_pwr_gpio_init(BOARD_DVP_PWR_PIN, BOARD_PWR_ACTIVE_LV));
#endif

#if defined(LCKFB_T5AI_TOUCH_LCD_ST7789_240X320) && (LCKFB_T5AI_TOUCH_LCD_ST7789_240X320 == 1)
    tal_system_sleep(20);
#endif

    return rt;
}

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
    cfg.spk_pin_polarity = TUYA_GPIO_LEVEL_HIGH;

    TUYA_CALL_ERR_RETURN(tdd_audio_register(AUDIO_CODEC_NAME, cfg));
#endif
    return rt;
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

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

static OPERATE_RET __board_rx1_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    extern int bk_uart_set_enable_rx(int id, bool enable);
    bk_uart_set_enable_rx(1, 0);

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

    TUYA_CALL_ERR_LOG(__board_power_init());

    TUYA_CALL_ERR_LOG(__board_rx1_deinit());

    TUYA_CALL_ERR_LOG(__board_register_audio());

    TUYA_CALL_ERR_LOG(__board_register_button());

    TUYA_CALL_ERR_LOG(board_register_ex_module());

    return rt;
}
