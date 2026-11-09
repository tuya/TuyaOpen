/**
 * @file tdd_audio_i2s_ext.h
 * @brief T5 external I2S audio driver for INMP441 mic + MAX98357 amp
 * @version 0.1
 * @date 2026-08-12
 * @copyright Copyright (c) Tuya Inc. All Rights Reserved.
 */
#ifndef __TDD_AUDIO_I2S_EXT_H__
#define __TDD_AUDIO_I2S_EXT_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    uint32_t mic_sample_rate; /*!< PCM sample rate in Hz (e.g. 16000) */
    uint32_t spk_sample_rate; /*!< PCM sample rate in Hz (e.g. 16000) */
    int sd_pin;              /*!< MAX98357 SD pin, or TUYA_GPIO_NUM_MAX if tied high */
    int sd_pin_polarity;     /*!< Level that enables the amp (usually HIGH) */
} TDD_AUDIO_I2S_EXT_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Register INMP441 (I2S0 RX) + MAX98357 (I2S1 TX) audio driver
 * @param[in] name audio codec name (AUDIO_CODEC_NAME)
 * @param[in] cfg  board audio config
 * @return OPRT_OK on success, error code on failure
 * @note Duplex on I2S1: P40=BCLK P41=WS P42=DIN(mic) P43=DOUT(spk).
 */
OPERATE_RET tdd_audio_i2s_ext_register(char *name, TDD_AUDIO_I2S_EXT_T cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_AUDIO_I2S_EXT_H__ */
