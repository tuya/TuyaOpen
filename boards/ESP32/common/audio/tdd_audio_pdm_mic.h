/**
 * @file tdd_audio_pdm_mic.h
 * @brief PDM digital microphone (mic-only) TDD audio driver for ESP32/ESP32-S3.
 *
 * For boards that carry a bare PDM MEMS microphone with no audio codec and no
 * speaker (e.g. Seeed XIAO ESP32S3 Sense). Uses the ESP-IDF I2S PDM RX mode.
 * Playback is not supported (there is no output path); play() returns
 * OPRT_NOT_SUPPORTED.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_AUDIO_PDM_MIC_H__
#define __TDD_AUDIO_PDM_MIC_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t  i2s_id;          /*!< I2S port used for PDM RX */
    int      clk_io;          /*!< PDM clock GPIO (output to mic) */
    int      din_io;          /*!< PDM data GPIO (input from mic) */
    uint32_t mic_sample_rate; /*!< Microphone sample rate in Hz (e.g. 16000) */
} TDD_AUDIO_PDM_MIC_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Register a PDM microphone as a TDL audio device (capture only).
 *
 * @param[in] name Audio device name (matches AUDIO_CODEC_NAME).
 * @param[in] cfg  PDM microphone hardware configuration.
 * @return OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tdd_audio_pdm_mic_register(char *name, TDD_AUDIO_PDM_MIC_T cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_AUDIO_PDM_MIC_H__ */
