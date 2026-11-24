/**
 * @file tdd_audio.h
 * @brief Tuya Device Driver layer audio interface for T5AI platform.
 *
 * This file defines the device driver interface for audio functionality on the T5AI
 * platform. It provides structures and functions for configuring and registering
 * audio devices with support for various audio parameters including sample rates,
 * data bits, channels, and speaker configuration. The interface also includes
 * support for acoustic echo cancellation (AEC) functionality.
 *
 * The TDD (Tuya Device Driver) layer acts as an abstraction between the hardware-specific
 * implementations and the higher-level TDL (Tuya Driver Layer) audio management system.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_AUDIO_T5AI_H__
#define __TDD_AUDIO_T5AI_H__

#include "tuya_cloud_types.h"

#if defined(ENABLE_MEDIA) && (ENABLE_MEDIA == 1)
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
/**
 * @brief T5AI audio device configuration.
 */
typedef struct {
    uint8_t aec_enable;                  /**< Enable hardware AEC. */
    TKL_AI_CHN_E ai_chn;                 /**< Audio input channel. */
    TKL_AUDIO_SAMPLE_E sample_rate;      /**< Capture sample rate. */
    TKL_AUDIO_DATABITS_E data_bits;      /**< Capture bits per sample. */
    TKL_AUDIO_CHANNEL_E channel;         /**< Capture channel mode. */

    TKL_AUDIO_SAMPLE_E spk_sample_rate;  /**< Playback sample rate. */
    int spk_pin;                         /**< Speaker enable GPIO, or -1 if unused. */
    int spk_pin_polarity;                /**< Active level of the speaker enable GPIO. */
} TDD_AUDIO_T5AI_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Register the T5AI audio driver.
 *
 * This function is called by board initialization code only. Application code
 * must use the tdl_audio_* abstraction.
 *
 * @param[in] name Unique TDL audio device name, normally AUDIO_CODEC_NAME.
 * @param[in] cfg  T5AI audio hardware configuration.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdd_audio_register(char *name, TDD_AUDIO_T5AI_T cfg);

#ifdef __cplusplus
}
#endif

#endif

#endif /* __TDD_AUDIO_T5AI_H__ */
