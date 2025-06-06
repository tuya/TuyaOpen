/**
 * @file ai_audio_cloud_asr.h
 * @brief Header file for the audio cloud ASR module, which handles audio recording, buffering, and uploading.
 *
 * This header file declares the functions and types necessary for initializing and managing the audio recording
 * process, including setting up buffers, timers, and threads. It also provides functions to write audio data, reset the
 * buffer, post new states, and retrieve the current state.
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

typedef enum {
    AI_CLOUD_ASR_STATE_IDLE = 0,
    AI_CLOUD_ASR_STATE_UPLOAD,
    AI_CLOUD_ASR_STATE_WAIT_ASR, // wait cloud asr response timeout
} AI_CLOUD_ASR_STATE_E;

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initializes the audio cloud ASR module.
 * @param is_enable_interrupt Boolean flag indicating whether interrupts are enabled.
 * @return OPERATE_RET - OPRT_OK if initialization is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_init(void);

/**
 * @brief Starts the audio cloud ASR process.
 * @param none
 * @return OPERATE_RET - OPRT_OK if the start operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_start(void);

/**
 * @brief Stops the audio cloud ASR process.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the stop operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_stop(void);

/**
 * @brief Stops waiting for the cloud ASR response and transitions the state to idle.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_stop_wait_asr(void);

/**
 * @brief Transitions audio cloud ASR process to the idle state, interrupting any ongoing uploads.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_set_idle(bool is_forced);

/**
 * @brief Get the current state of the audio could asr process.
 * @param None
 * @return AI_CLOUD_ASR_STATE_E - The current state of the audio cloud asr process.
 */
AI_CLOUD_ASR_STATE_E ai_audio_cloud_asr_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_AUDIO_CLOUD_ASR_H__ */
