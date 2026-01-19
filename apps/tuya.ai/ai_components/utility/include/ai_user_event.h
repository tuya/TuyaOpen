/**
 * @file ai_user_event.h
 * @brief AI user event notification module header
 *
 * This header file defines the types and functions for AI user event
 * notifications. It provides an event-driven mechanism for components
 * to communicate AI-related events such as ASR results, TTS playback,
 * emotion detection, and skill execution.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_USER_EVENT_H__
#define __AI_USER_EVENT_H__

#include "tuya_cloud_types.h"
#include "skill_emotion.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    AI_USER_EVT_IDLE = -1,
    AI_USER_EVT_ASR_EMPTY = 0,
    AI_USER_EVT_ASR_OK,
    AI_USER_EVT_ASR_ERROR,
    AI_USER_EVT_MIC_DATA,
    AI_USER_EVT_TTS_PRE,
    AI_USER_EVT_TTS_START,
    AI_USER_EVT_TTS_DATA,
    AI_USER_EVT_TTS_STOP,
    AI_USER_EVT_TTS_ABORT,
    AI_USER_EVT_TTS_ERROR,
    AI_USER_EVT_VAD_TIMEOUT,
    AI_USER_EVT_TEXT_STREAM_START,  /* 10 */
    AI_USER_EVT_TEXT_STREAM_DATA,
    AI_USER_EVT_TEXT_STREAM_STOP,
    AI_USER_EVT_TEXT_STREAM_ABORT,
    AI_USER_EVT_EMOTION,
    AI_USER_EVT_LLM_EMOTION,
    AI_USER_EVT_SKILL,
    AI_USER_EVT_CHAT_BREAK,
    AI_USER_EVT_SERVER_VAD,
    AI_USER_EVT_END,
    AI_USER_EVT_PLAY_CTL_PLAY,  /* 20 */
    AI_USER_EVT_PLAY_CTL_RESUME,
    AI_USER_EVT_PLAY_CTL_PAUSE,
    AI_USER_EVT_PLAY_CTL_REPLAY,
    AI_USER_EVT_PLAY_CTL_PREV,
    AI_USER_EVT_PLAY_CTL_NEXT,
    AI_USER_EVT_PLAY_CTL_SEQUENTIAL,
    AI_USER_EVT_PLAY_CTL_SEQUENTIAL_LOOP,
    AI_USER_EVT_PLAY_CTL_SINGLE_LOOP,
    AI_USER_EVT_PLAY_CTL_END,
    AI_USER_EVT_PLAY_END,   /* 30 */
    AI_USER_EVT_PLAY_ALERT, 
    AI_USER_EVT_MODE_SWITCH, 
    AI_USER_EVT_MODE_STATE_UPDATE,
    AI_USER_EVT_VEDIO_DISPLAY_START,
    AI_USER_EVT_VEDIO_DISPLAY_END,
    AI_USER_EVT_EXIT, 
}AI_USER_EVT_TYPE_E;

typedef struct {
    AI_USER_EVT_TYPE_E type;
    void *data;
}AI_NOTIFY_EVENT_T;

typedef void (*AI_USER_EVENT_NOTIFY)(AI_NOTIFY_EVENT_T *event);

typedef struct {
    char     *data;
    uint16_t datalen;
    uint32_t timeindex;
} AI_NOTIFY_TEXT_T;

typedef AI_AGENT_EMO_T AI_NOTIFY_EMO_T;

typedef struct {
    uint16_t camera_width;
    uint16_t camera_height;
} AI_NOTIFY_VIDEO_START_T;

typedef struct {
    uint8_t *data;
    uint32_t data_len;
} AI_NOTIFY_MIC_DATA_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
@brief Register a callback function for AI user event notifications
@param cb Callback function pointer to be called when events occur
@return None
*/
void ai_user_event_notify_register(AI_USER_EVENT_NOTIFY cb);

/**
@brief Notify registered callback about an AI user event
@param type Event type to notify
@param data Pointer to event data (can be NULL)
@return None
*/
void ai_user_event_notify(AI_USER_EVT_TYPE_E type,  void *data);

#ifdef __cplusplus
}
#endif

#endif /* __AI_USER_EVENT_H__ */
