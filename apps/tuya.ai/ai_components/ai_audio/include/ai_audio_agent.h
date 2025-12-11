/**
 * @file ai_audio_agent.h
 * @brief Provides declarations for the AI service module functions and types.
 *
 * This header file contains the function declarations and type definitions for
 * managing the AI service module, including initialization, starting the upload
 * process, uploading audio data, and stopping the upload process. It defines
 * message types and a callback function type for handling AI messages.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __AI_AUDIO_AGENT_H__
#define __AI_AUDIO_AGENT_H__

#include "tuya_cloud_types.h"
#include "tuya_ai_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef enum {
    AI_AGENT_CHAT_STREAM_START,
    AI_AGENT_CHAT_STREAM_DATA,
    AI_AGENT_CHAT_STREAM_STOP,
    AI_AGENT_CHAT_STREAM_ABORT,
} AI_AGENT_CHAT_STREAM_E;

typedef enum {
    AI_AGENT_MSG_TP_TEXT_ASR,
    AI_AGENT_MSG_TP_TEXT_NLG_START,
    AI_AGENT_MSG_TP_TEXT_NLG_DATA,
    AI_AGENT_MSG_TP_TEXT_NLG_STOP,
    AI_AGENT_MSG_TP_AUDIO_START,
    AI_AGENT_MSG_TP_AUDIO_DATA,
    AI_AGENT_MSG_TP_AUDIO_STOP,
    AI_AGENT_MSG_TP_EMOTION,
} AI_AGENT_MSG_TYPE_E;

typedef struct {
    AI_AGENT_MSG_TYPE_E type;
    uint32_t data_len;
    uint8_t *data;
} AI_AGENT_MSG_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    void (*ai_agent_msg_cb)(AI_AGENT_MSG_T *msg);
    void (*ai_agent_event_cb)(AI_EVENT_TYPE event, AI_EVENT_ID event_id);
    void (*ai_agent_alert_cb)(int type);
} AI_AGENT_CBS_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initializes the AI service module.
 * @param msg_cb Callback function for handling AI messages.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_init(AI_AGENT_CBS_T *cbs);

/**
 * @brief Starts the AI audio upload process.
 * @param enable_vad Flag to enable cloud vad.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_start(uint8_t enable_vad);

/**
 * @brief Uploads audio data to the AI service.
 * @param data Pointer to the audio data buffer.
 * @param len Length of the audio data in bytes.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_data(uint8_t *data, uint32_t len);

/**
 * @brief Stops the AI audio upload process.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_stop(void);

/**
 * @brief Intrrupt the AI chat process.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_chat_intrrupt(void);

/**
 * @brief Send cloud alert to the AI service.
 * @param type Alert type
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_cloud_alert(int type);

/**
 * @brief Stops the AI chat process.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_text_agent_upload(uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __AI_AUDIO_AGENT_H__ */
