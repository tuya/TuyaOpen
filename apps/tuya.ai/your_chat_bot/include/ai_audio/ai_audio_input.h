/**
 * @file ai_audio_input.h
 * @brief Header file for audio input handling functions including initialization, 
 *        enabling/disabling detection, and setting wakeup types.
 *
 * This header file declares the functions and data structures required for managing audio input operations 
 * such as initializing the audio system,enabling and disabling audio detection, 
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
#define AI_AUDIO_VAD_ACITVE_TM_MS (600)    // vad active duration

#define AI_AUDIO_VOICE_FRAME_LEN_GET(tm_ms) ((tm_ms) / AI_AUDIO_PCM_FRAME_TM_MS * AI_AUDIO_PCM_FRAME_SIZE)
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    AI_AUDIO_INPUT_STATE_IDLE,
    AI_AUDIO_INPUT_STATE_DETECTING,     // detect vad and wakeup keyword
    AI_AUDIO_INPUT_STATE_DETECTED_WORD, // detected wakeup word
    AI_AUDIO_INPUT_STATE_AWAKE,
} AI_AUDIO_INPUT_STATE_E;

typedef enum {
    AI_AUDIO_INPUT_EVT_NONE,
    AI_AUDIO_INPUT_EVT_IDLE,
    AI_AUDIO_INPUT_EVT_ENTER_DETECT, // detecting vad and wakeup keywor
    AI_AUDIO_INPUT_EVT_DETECTING,
    AI_AUDIO_INPUT_EVT_ASR_WORD,
    AI_AUDIO_INPUT_EVT_WAKEUP,
    AI_AUDIO_INPUT_EVT_AWAKE,
} AI_AUDIO_INPUT_EVENT_E;

typedef enum {
    AI_AUDIO_INPUT_WAKEUP_MANUAL, // manual trigger wakeup
    AI_AUDIO_INPUT_WAKEUP_VAD,    // detect vad
    AI_AUDIO_INPUT_WAKEUP_ASR,    // detect vad and asr wakeup keyword
    AI_AUDIO_INPUT_WAKEUP_MAX,
} AI_AUDIO_INPUT_WAKEUP_TP_E;

typedef struct {
    AI_AUDIO_INPUT_WAKEUP_TP_E wakeup_tp;
} AI_AUDIO_INPUT_CFG_T;

typedef void (*AI_AUDIO_INOUT_INFORM_CB)(AI_AUDIO_INPUT_EVENT_E event, uint8_t *data, uint32_t len, void *arg);

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

/**
 * @brief Enables the audio input detection and wakeup functionality.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_enable_detect_wakeup(void);

/**
 * @brief Disables the audio input detection and wakeup functionality.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_disable_detect_wakeup(void);

/**
 * @brief Restarts the ASR detection for wakeup words.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_restart_asr_detect_wakeup_word(void);

/**
 * @brief Triggers the ASR wakeup process.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_trigger_asr_awake(void);

/**
 * @brief Sets the wakeup type for audio input.
 * @param wakeup_tp The type of wakeup to set (e.g., VAD, ASR).
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_set_wakeup_tp(AI_AUDIO_INPUT_WAKEUP_TP_E wakeup_tp);

/**
 * @brief Resets the audio input ring buffer.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_rb_reset(void);

/**
 * @brief Stops or starts the wakeup feed based on the provided flag.
 * @param is_enable Boolean flag to stop (true) or start (false) the wakeup feed.
 * @return None
 */
void ai_audio_input_stop_wakeup_feed(bool is_enable);

#ifdef __cplusplus
}
#endif

#endif /* __AI_AUDIO_INPUT_H__ */
