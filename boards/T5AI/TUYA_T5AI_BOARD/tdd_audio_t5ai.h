/**
 * @file tdd_audio_t5ai.h
 * @brief tdd_audio_t5ai module is used to
 * @version 0.1
 * @date 2025-04-24
 */

#ifndef __TDD_AUDIO_T5AI_H__
#define __TDD_AUDIO_T5AI_H__

#include "tuya_cloud_types.h"

#include "tdl_audio_driver.h"

#include "tkl_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t aec_enable;
    TKL_AI_CHN_E ai_chn;
    TKL_AUDIO_SAMPLE_E sample_rate;
    TKL_AUDIO_DATABITS_E data_bits;
    TKL_AUDIO_CHANNEL_E channel;

    // spk
    TKL_AUDIO_SAMPLE_E spk_sample_rate;
    int spk_pin;
    int spk_pin_polarity;
} TDD_AUDIO_T5AI_T;

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tdd_audio_t5ai_register(char *name, TDD_AUDIO_T5AI_T cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_AUDIO_T5AI_H__ */
