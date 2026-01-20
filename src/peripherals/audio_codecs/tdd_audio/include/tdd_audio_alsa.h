/**
 * @file tdd_audio_alsa.h
 * @brief Tuya Device Driver layer audio interface for ALSA (Advanced Linux Sound Architecture).
 *
 * This file defines the device driver interface for audio functionality using ALSA
 * on Linux/Ubuntu platforms. It provides structures and functions for configuring
 * and registering ALSA audio devices with support for various audio parameters
 * including sample rates, data bits, channels, and buffer configuration.
 *
 * The TDD (Tuya Device Driver) layer acts as an abstraction between ALSA-specific
 * implementations and the higher-level TDL (Tuya Driver Layer) audio management system.
 *
 * This driver is only available when ENABLE_AUDIO_ALSA is enabled and the Ubuntu
 * platform is selected.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_AUDIO_ALSA_H__
#define __TDD_AUDIO_ALSA_H__

#include "tuya_cloud_types.h"

#if defined(ENABLE_AUDIO_ALSA) && (ENABLE_AUDIO_ALSA == 1)
#include "tdl_audio_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
// ALSA audio sample rates
typedef enum {
    TDD_ALSA_SAMPLE_8000 = 8000,
    TDD_ALSA_SAMPLE_11025 = 11025,
    TDD_ALSA_SAMPLE_16000 = 16000,
    TDD_ALSA_SAMPLE_22050 = 22050,
    TDD_ALSA_SAMPLE_32000 = 32000,
    TDD_ALSA_SAMPLE_44100 = 44100,
    TDD_ALSA_SAMPLE_48000 = 48000,
} TDD_ALSA_SAMPLE_RATE_E;

// ALSA audio data bits
typedef enum {
    TDD_ALSA_DATABITS_8 = 8,
    TDD_ALSA_DATABITS_16 = 16,
    TDD_ALSA_DATABITS_24 = 24,
    TDD_ALSA_DATABITS_32 = 32,
} TDD_ALSA_DATABITS_E;

// ALSA audio channels
typedef enum {
    TDD_ALSA_CHANNEL_MONO = 1,
    TDD_ALSA_CHANNEL_STEREO = 2,
} TDD_ALSA_CHANNEL_E;

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief ALSA audio configuration structure
 *
 * This structure contains all the configuration parameters needed to initialize
 * an ALSA audio device for both capture (microphone) and playback (speaker).
 */
typedef struct {
    // Capture (microphone) settings
    char capture_device[64];              /**< ALSA capture device name (e.g., "default", "hw:0,0") */
    TDD_ALSA_SAMPLE_RATE_E sample_rate;   /**< Audio sample rate in Hz */
    TDD_ALSA_DATABITS_E data_bits;        /**< Number of bits per sample */
    TDD_ALSA_CHANNEL_E channels;          /**< Number of audio channels */

    // Playback (speaker) settings
    char playback_device[64];             /**< ALSA playback device name (e.g., "default", "hw:0,0") */
    TDD_ALSA_SAMPLE_RATE_E spk_sample_rate; /**< Speaker sample rate in Hz */

    // Buffer settings
    uint32_t buffer_frames;               /**< ALSA buffer size in frames */
    uint32_t period_frames;               /**< ALSA period size in frames */

    // Optional features
    uint8_t aec_enable;                   /**< Enable acoustic echo cancellation (future use) */
} TDD_AUDIO_ALSA_CFG_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Audio Front-End (AFE) feed callback function type
 * 
 * This callback is used to feed audio data to Voice Activity Detection (VAD)
 * or Keyword Spotting (KWS) engines.
 * 
 * @param[in] mic_data  Microphone captured audio data (PCM int16)
 * @param[in] ref_data  Reference audio data, typically from speaker playback (can be NULL)
 * @param[in] out_data  Processed output data from AEC or other processing (can be NULL)
 * @param[in] frames    Number of audio frames in the buffers
 */
typedef void (*TDD_AUDIO_AFE_FEED_CB)(int16_t *mic_data, int16_t *ref_data, int16_t *out_data, uint32_t frames);

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Register an ALSA audio driver with the TDL audio management system
 *
 * This function creates and registers an ALSA audio driver instance with the
 * specified configuration. The driver will be available for use by applications
 * through the TDL audio management API.
 *
 * @param[in] name      Driver name (used to identify this audio device)
 * @param[in] cfg       ALSA configuration parameters
 *
 * @return OPERATE_RET
 * @retval OPRT_OK              Success
 * @retval OPRT_MALLOC_FAILED   Memory allocation failed
 * @retval OPRT_COM_ERROR       Registration failed
 */
OPERATE_RET tdd_audio_alsa_register(char *name, TDD_AUDIO_ALSA_CFG_T cfg);

/**
 * @brief Register a callback function for VAD (Voice Activity Detection) audio feed
 * 
 * The registered callback will be called with captured audio data during recording.
 * This allows VAD processing to run in parallel with normal audio capture.
 *
 * @param[in] cb    Callback function pointer, or NULL to clear the callback
 */
void tdd_audio_alsa_register_vad_feed_cb(TDD_AUDIO_AFE_FEED_CB cb);

/**
 * @brief Register a callback function for KWS (Keyword Spotting) audio feed
 * 
 * The registered callback will be called with captured audio data during recording.
 * This allows KWS processing to run in parallel with normal audio capture.
 *
 * @param[in] cb    Callback function pointer, or NULL to clear the callback
 */
void tdd_audio_alsa_register_kws_feed_cb(TDD_AUDIO_AFE_FEED_CB cb);

/**
 * @brief Unregister the VAD audio feed callback
 */
void tdd_audio_alsa_unregister_vad_feed_cb(void);

/**
 * @brief Unregister the KWS audio feed callback
 */
void tdd_audio_alsa_unregister_kws_feed_cb(void);

#ifdef __cplusplus
}
#endif

#endif /* ENABLE_AUDIO_ALSA */

#endif /* __TDD_AUDIO_ALSA_H__ */

