/**
 * @file tdd_audio_pdm_i2s_spk.h
 * @brief PDM microphone + I2S STD speaker (e.g. MAX98357A) audio driver for ESP32-S3.
 *
 * Capture uses ESP-IDF I2S PDM RX; playback uses a separate I2S STD TX port for
 * boards such as Seeed XIAO ESP32S3 Sense with an external MAX98357A module.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_AUDIO_PDM_I2S_SPK_H__
#define __TDD_AUDIO_PDM_I2S_SPK_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    uint8_t  mic_i2s_id;       /*!< I2S port for PDM RX */
    int      mic_clk_io;       /*!< PDM clock GPIO */
    int      mic_din_io;       /*!< PDM data GPIO */
    uint32_t mic_sample_rate;  /*!< Microphone sample rate in Hz (e.g. 16000) */

    uint8_t  spk_i2s_id;       /*!< I2S port for STD TX (speaker) */
    int      spk_bclk_io;      /*!< Speaker BCLK GPIO */
    int      spk_ws_io;        /*!< Speaker WS / LRCK GPIO */
    int      spk_dout_io;      /*!< Speaker DIN GPIO (MCU DOUT -> amp DIN) */
    uint32_t spk_sample_rate;  /*!< Speaker sample rate in Hz (e.g. 16000) */

    int      spk_sd_pin;       /*!< MAX98357 SD pin, or TUYA_GPIO_NUM_MAX if tied high */
    int      spk_sd_polarity; /*!< GPIO level that enables amp (usually HIGH) */
} TDD_AUDIO_PDM_I2S_SPK_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Register PDM mic + I2S STD speaker as a TDL audio device.
 *
 * @param[in] name Audio device name (matches AUDIO_CODEC_NAME).
 * @param[in] cfg  Board audio hardware configuration.
 * @return OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tdd_audio_pdm_i2s_spk_register(char *name, TDD_AUDIO_PDM_I2S_SPK_T cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_AUDIO_PDM_I2S_SPK_H__ */
