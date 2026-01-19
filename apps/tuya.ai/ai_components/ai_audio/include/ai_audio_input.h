/**
 * @file ai_audio_input.h
 * @brief AI audio input module header
 *
 * This header file defines the types and functions for AI audio input processing,
 * including VAD (Voice Activity Detection) configuration and audio data output callbacks.
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
#define EVENT_AUDIO_VAD   "EVENT.VAD"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    AI_AUDIO_VAD_MANUAL,    // use key event as vad
    AI_AUDIO_VAD_AUTO,      // use human voice detect 
} AI_AUDIO_VAD_MODE_E;

typedef enum {
    AI_AUDIO_VAD_START = 1,
    AI_AUDIO_VAD_STOP,
} AI_AUDIO_VAD_STATE_E;

typedef int (*AI_AUDIO_OUTPUT)(uint8_t *data, uint16_t datalen);

typedef struct  {
    /* VAD cache = vad_active_ms + vad_off_ms */
    AI_AUDIO_VAD_MODE_E     vad_mode;
    uint16_t                vad_off_ms;        /* Voice activity compensation time, unit: ms */
    uint16_t                vad_active_ms;     /* Voice activity detection threshold, unit: ms */
    uint16_t                slice_ms;          /* Reference macro, AUDIO_RECORDER_SLICE_TIME */

    /* Microphone data processing callback */
    AI_AUDIO_OUTPUT         output_cb;
} AI_AUDIO_INPUT_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
@brief Initialize the AI audio input module
@param cfg Audio input configuration
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_init(AI_AUDIO_INPUT_CFG_T *cfg);

/**
@brief Start audio input
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_start(void);

/**
@brief Stop audio input
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_stop(void);

/**
@brief Deinitialize the AI audio input module
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_deinit(void);

/**
@brief Reset audio input ring buffer and VAD state
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_reset(void);

/**
@brief Set wake-up mode (VAD mode)
@param mode VAD mode (manual or auto)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_wakeup_mode_set(AI_AUDIO_VAD_MODE_E mode);

/**
@brief Set wake-up state
@param is_wakeup Wake-up flag
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_wakeup_set(bool is_wakeup);

#ifdef __cplusplus
}
#endif

#endif /* __AI_AUDIO_INPUT_H__ */
