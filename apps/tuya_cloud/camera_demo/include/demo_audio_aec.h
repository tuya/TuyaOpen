/**
 * @file demo_audio_aec.h
 * @brief Speex AEC + RNN VAD frontend (align TuyaOS wukong)
 * @version 1.1
 * @date 2026-08-06
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __DEMO_AUDIO_AEC_H__
#define __DEMO_AUDIO_AEC_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef enum {
    DEMO_AUDIO_VAD_STOP = 0,
    DEMO_AUDIO_VAD_START = 1,
} DEMO_AUDIO_VAD_FLAG_E;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Init Speex AES + RNN VAD (align OS wukong_audio_frontend_init)
 * @param[in] min_speech_len_ms min speech length for RNN VAD (OS default 500)
 * @param[in] max_speech_interval_ms max speech gap (OS default 1000)
 * @param[in] frame_bytes frame size in bytes (16k/16bit/20ms = 640)
 * @return OPRT_OK on success
 */
OPERATE_RET demo_audio_aec_init(UINT32_T min_speech_len_ms, UINT32_T max_speech_interval_ms, UINT32_T frame_bytes);

/**
 * @brief Destroy Speex/RNN resources and free linearaec buffer
 * @return none
 */
VOID_T demo_audio_aec_deinit(VOID_T);

/**
 * @brief Soft AEC + VAD process. Register via tkl_ai_set_vad_aec_algorithm.
 * @param[in] mic_data near-end mic PCM
 * @param[in] ref_data far-end / playback reference PCM
 * @param[out] out_data echo-cancelled PCM
 * @return 0 on success, negative on error
 * @note Signature matches aec_vad_process_fun / OS wukong_audio_frontend_process.
 */
INT_T demo_audio_aec_process(SHORT_T *mic_data, SHORT_T *ref_data, SHORT_T *out_data);

/**
 * @brief Start RNN VAD (align OS wukong_audio_frontend_vad_start)
 * @return OPRT_OK on success
 */
OPERATE_RET demo_audio_aec_vad_start(VOID_T);

/**
 * @brief Stop RNN VAD (align OS wukong_audio_frontend_vad_stop)
 * @return OPRT_OK on success
 */
OPERATE_RET demo_audio_aec_vad_stop(VOID_T);

/**
 * @brief Get current VAD flag for uplink gating
 * @return DEMO_AUDIO_VAD_START or DEMO_AUDIO_VAD_STOP
 */
INT_T demo_audio_aec_vad_get_flag(VOID_T);

#ifdef __cplusplus
}
#endif

#endif /* __DEMO_AUDIO_AEC_H__ */
