/**
 * @file tuya_t5_ai_board.c
 * @brief tuya_t5_ai_board module is used to
 * @version 0.1
 * @date 2025-04-24
 */

#include "tuya_cloud_types.h"

#include "tdd_audio_t5ai.h"

#include "tkl_audio.h"

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
#if defined(SPEAKER_EN_PIN)
int app_audio_driver_init(const char *name)
{
    TDD_AUDIO_T5AI_T cfg = {0};
    memset(&cfg, 0, sizeof(TDD_AUDIO_T5AI_T));

#if defined(ENABLE_AEC) && (ENABLE_AEC == 1)
    cfg.aec_enable = 1;
#else
    cfg.aec_enable = 0;
#endif

    cfg.ai_chn = TKL_AI_0;
    cfg.sample_rate = TKL_AUDIO_SAMPLE_16K;
    cfg.data_bits = TKL_AUDIO_DATABITS_16;
    cfg.channel = TKL_AUDIO_CHANNEL_MONO;

    cfg.spk_sample_rate = TKL_AUDIO_SAMPLE_16K;
    cfg.spk_pin = SPEAKER_EN_PIN;
    cfg.spk_pin_polarity = TUYA_GPIO_LEVEL_LOW;

    return tdd_audio_t5ai_register(name, cfg);
}
#endif