/**
 * @file ai_audio_cloud_asr.h
 * @brief Header file for the audio recorder module, providing declarations for initialization, data writing, resetting,
 * and state management.
 *
 * This header file includes the necessary declarations for functions that manage the audio recording process, such as
 * initializing the recorder, writing audio data to a ring buffer, resetting the buffer, posting new states, and
 * retrieving the current state.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_AUDIO_CLOUD_ASR_H__
#define __AI_AUDIO_CLOUD_ASR_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
// PCM frame size: 320 bytes
#define AI_AUDIO_ASR_FRAME_TM_MS (10)
#define AI_AUDIO_ASR_FRAME_SIZE  (320)

typedef enum {
    AI_CLOUD_ASR_STATE_IDLE = 0,
    AI_CLOUD_ASR_STATE_UPLOAD_START,
    AI_CLOUD_ASR_STATE_UPLOADING,
    AI_CLOUD_ASR_STATE_UPLOAD_STOP,
    AI_CLOUD_ASR_STATE_WAIT_ASR, // wait cloud asr response timeout
} AI_CLOUD_ASR_STATE_E;

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initializes the audio recorder module.
 * @param None
 * @return OPERATE_RET - OPRT_OK if initialization is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_init(bool is_enable_interrupt);

/**
 * @brief Writes data to the audio recorder's ring buffer.
 * @param data Pointer to the data to be written.
 * @param len Length of the data to be written.
 * @return OPERATE_RET - OPRT_OK if the write operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_input(const void *data, uint32_t len);

OPERATE_RET ai_audio_cloud_asr_vad_input(const void *data, uint32_t len);

OPERATE_RET ai_audio_cloud_asr_start(void);

OPERATE_RET ai_audio_cloud_asr_stop(void);

/**
 * @brief Resets the audio recorder's ring buffer if it is not empty.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the reset operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_reset(void);

/**
 * @brief Retrieves the current state of the audio recorder.
 * @param None
 * @return APP_RECORDER_STATE_E - The current state of the audio recorder.
 */
AI_CLOUD_ASR_STATE_E ai_audio_cloud_asr_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_AUDIO_CLOUD_ASR_H__ */
