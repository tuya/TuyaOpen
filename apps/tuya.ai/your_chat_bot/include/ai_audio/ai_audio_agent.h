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

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef uint8_t AI_AGENT_MSG_TYPE_E;
#define AI_AGENT_MSG_TP_TEXT_ASR    0x01
#define AI_AGENT_MSG_TP_TEXT_NLG    0x02
#define AI_AGENT_MSG_TP_AUDIO_START 0x03
#define AI_AGENT_MSG_TP_AUDIO_DATA  0x04
#define AI_AGENT_MSG_TP_AUDIO_STOP  0x05
#define AI_AGENT_MSG_TP_EMOTION     0x06

typedef struct {
    AI_AGENT_MSG_TYPE_E type;
    uint32_t data_len;
    uint8_t *data;
} AI_AGENT_MSG_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void (*AI_AGENT_MSG_CB)(AI_AGENT_MSG_T *msg);

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initializes the AI service module.
 * @param msg_cb Callback function for handling AI messages.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_init(AI_AGENT_MSG_CB msg_cb);

/**
 * @brief Starts the AI upload process.
 * @param int_enable_interrupt Flag to enable interrupt processing.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_start(uint8_t int_enable_interrupt);

/**
 * @brief Uploads audio data to the AI service.
 * @param is_first Flag indicating if this is the first chunk of data.
 * @param data Pointer to the audio data buffer.
 * @param len Length of the audio data in bytes.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_data(uint8_t is_first, uint8_t *data, uint32_t len);

/**
 * @brief Stops the AI upload process.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_stop(void);

/**
 * @brief Intrrupt the AI upload process.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_intrrupt(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_AUDIO_AGENT_H__ */
