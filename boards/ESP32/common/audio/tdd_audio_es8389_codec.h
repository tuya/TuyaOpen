/**
 * @file tdd_audio_es8389_codec.h
 * @author Tuya Inc.
 * @brief ES8389 audio codec driver implementation for ESP32
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_AUDIO_ES8389_CODEC_H__
#define __TDD_AUDIO_ES8389_CODEC_H__

#include "tuya_cloud_types.h"

#include "tdl_audio_driver.h"
#include "tdd_audio_codec_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t i2c_id;
    TDD_AUDIO_I2C_HANDLE i2c_handle;
    uint8_t i2s_id;
    TDD_AUDIO_I2S_TX_HANDLE i2s_tx_handle;
    TDD_AUDIO_I2S_RX_HANDLE i2s_rx_handle;
    uint32_t mic_sample_rate;
    uint32_t spk_sample_rate;
    uint8_t es8389_addr;
    int pa_pin;
    int default_volume;
} TDD_AUDIO_ES8389_CODEC_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tdd_audio_es8389_codec_register(char *name, TDD_AUDIO_ES8389_CODEC_T cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_AUDIO_ES8389_CODEC_H__ */