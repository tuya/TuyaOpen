/**
 * @file ai_audio_input.h
 * @brief Header file for audio input handling functions including initialization,
 *        enabling/disabling detection, and setting wakeup types.
 *
 * This header file declares the functions and data structures required for managing audio input operations
 * such as initializing the audio system, enabling and disabling audio detection,
 * and setting the type of wakeup mechanism (e.g., VAD, ASR).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __AI_AUDIO_INPUT_H__
#define __AI_AUDIO_INPUT_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
// PCM frame size: 320 bytes
#define AI_AUDIO_PCM_FRAME_TM_MS (10)
#define AI_AUDIO_PCM_FRAME_SIZE  (320)

#define AI_AUDIO_VOICE_FRAME_LEN_GET(tm_ms)                                                                            \
    (((tm_ms > AI_AUDIO_PCM_FRAME_TM_MS) ? (tm_ms) : AI_AUDIO_PCM_FRAME_TM_MS) / AI_AUDIO_PCM_FRAME_TM_MS *            \
     AI_AUDIO_PCM_FRAME_SIZE)
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    AI_AUDIO_INPUT_STATE_IDLE,
    AI_AUDIO_INPUT_STATE_DETECTING,
    AI_AUDIO_INPUT_STATE_GET_VALID_DATA,
    AI_AUDIO_INPUT_STATE_ASR_WAKEUP_WORD,
} AI_AUDIO_INPUT_STATE_E;

typedef enum {
    AI_AUDIO_INPUT_EVT_NONE,
    AI_AUDIO_INPUT_EVT_GET_VALID_VOICE_START,
    AI_AUDIO_INPUT_EVT_GET_VALID_VOICE_STOP,
    AI_AUDIO_INPUT_EVT_ASR_WAKEUP_WORD,
    AI_AUDIO_INPUT_EVT_ASR_WAKEUP_STOP, // Valid audio data can only be retained after the wake-up word is recognized
                                        // again.
} AI_AUDIO_INPUT_EVENT_E;

typedef enum {
    AI_AUDIO_INPUT_VALID_METHOD_MANUAL, // Manually control whether to retain valid audio data.
    AI_AUDIO_INPUT_VALID_METHOD_VAD, // Valid audio data can only be retained after the VAD (Voice Activity Detection)
                                     // detects human voices.
    AI_AUDIO_INPUT_VALID_METHOD_ASR, // Valid audio data can only be retained after the wake-up word is recognized
    AI_AUDIO_INPUT_VALID_METHOD_MAX,
} AI_AUDIO_INPUT_VALID_METHOD_E;

typedef struct {
    AI_AUDIO_INPUT_VALID_METHOD_E get_valid_data_method;
} AI_AUDIO_INPUT_CFG_T;

typedef void (*AI_AUDIO_INOUT_INFORM_CB)(AI_AUDIO_INPUT_EVENT_E event, void *arg);

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initializes the audio input system with the provided configuration and callback.
 * @param cfg Pointer to the configuration structure for audio input.
 * @param cb Callback function to be called for audio input events.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_init(AI_AUDIO_INPUT_CFG_T *cfg, AI_AUDIO_INOUT_INFORM_CB cb);

OPERATE_RET ai_audio_input_enable_get_valid_data(bool is_enable);

OPERATE_RET ai_audio_input_manual_open_get_valid_data(bool is_open);

OPERATE_RET ai_audio_input_stop_asr_awake(void);

OPERATE_RET ai_audio_input_restart_asr_awake_timer(void);

uint32_t ai_audio_get_input_data(uint8_t *buff, uint32_t buff_len);

uint32_t ai_audio_get_input_data_size(void);

void ai_audio_discard_input_data(uint32_t discard_size);

#ifdef __cplusplus
}
#endif

#endif /* __AI_AUDIO_INPUT_H__ */
