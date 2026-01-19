/**
 * @file tuya_ai_input.h
 * @brief
 * @version 0.1
 * @date 2025-04-17
 *
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */

#ifndef __TUYA_AI_INPUT_H__
#define __TUYA_AI_INPUT_H__

#include "tuya_ai_types.h"

#include "tuya_cloud_types.h"
#include "tuya_iot_config.h"
#include "tuya_ai_protocol.h"
#include "tuya_ai_biz.h"
#include "tuya_ai_agent.h"

typedef enum {
    /** idle state */
    AI_INPUT_IDLE,
    /** start state */
    AI_INPUT_START_LAZY,
    /** start state */
    AI_INPUT_START,
    /** processing state */
    AI_INPUT_PROC,
    /** stopping state */
    AI_INPUT_STOPPING,
    /** stop state */
    AI_INPUT_STOP
} AI_INPUT_STATE_E;

typedef struct {
    /** video attr */
    AI_VIDEO_ATTR_BASE_T video;
    /** audio attr */
    AI_AUDIO_ATTR_BASE_T audio;
    /** image attr */
    AI_IMAGE_ATTR_BASE_T image;
    /** file attr */
    AI_FILE_ATTR_BASE_T file;
} AI_ATTR_BASE_T;

typedef struct {
    /** send packet type */
    AI_PACKET_PT type;
    /** need used in multi session*/
    bool multi_session;
    /** send channel get cb */
    AI_BIZ_SEND_GET_CB get_cb;
    /** send channel free cb */
    AI_BIZ_SEND_FREE_CB free_cb;
} AI_INPUT_SEND_T;

typedef struct {
    AI_ATTR_BASE_T attr;
    AI_INPUT_SEND_T send[AI_BIZ_MAX_NUM];
} AI_INPUT_CFG_T;

/**
 * @brief ai input start
 * @param[in] force force start new session(break old session)
 */
void tuya_ai_input_start(bool force);

/**
 * @brief get ai input state
 *
 * @return AI_INPUT_STATE_E
 */
AI_INPUT_STATE_E tuya_ai_input_get_state(void);

/**
 * @brief ai input stop
*
 */
void tuya_ai_input_stop(void);

/**
 * @brief ai video input
 *
 * @param[in] timestamp video timestamp
 * @param[in] pts video pts
 * @param[in] data video data
 * @param[in] len video data length
 * @param[in] total_len video total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_video_input(uint64_t timestamp, uint64_t pts, uint8_t *data, uint32_t len, uint32_t total_len);

/**
 * @brief ai audio input direct, no ringbuf
 *
 * @param[in] timestamp audio timestamp
 * @param[in] pts audio pts
 * @param[in] data audio data
 * @param[in] len audio data length
 * @param[in] total_len audio total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_audio_input_direct(uint64_t timestamp, uint64_t pts, uint8_t *data, uint32_t len, uint32_t total_len);

/**
 * @brief ai audio input
 *
 * @param[in] timestamp audio timestamp
 * @param[in] data audio data
 * @param[in] len audio data length
 * @param[in] total_len audio total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_audio_input(uint64_t timestamp, uint64_t pts, uint8_t *data, uint32_t len, uint32_t total_len);

/**
 * @brief ai image input
 *
 * @param[in] timestamp image timestamp
 * @param[in] data image data
 * @param[in] len image data length
 * @param[in] total_len image total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_image_input(uint64_t timestamp, uint8_t *data, uint32_t len, uint32_t total_len);

/**
 * @brief ai text input
 *
 * @param[in] data text data
 * @param[in] len text data length
 * @param[in] total_len text total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_text_input(uint8_t *data, uint32_t len, uint32_t total_len);

/**
 * @brief ai file input
 *
 * @param[in] data file data
 * @param[in] len file data length
 * @param[in] total_len file total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_file_input(uint8_t *data, uint32_t len, uint32_t total_len);

/* AI cloud alert type */
typedef int AI_CLOUD_ALERT_TYPE_E;
#define AI_ALERT_PLAY_ID    "ai-alert"
#define AI_ALERT_PLAY_ID_LEN (sizeof(AI_ALERT_PLAY_ID) - 1)
typedef void(*AI_ALERT_FB_CB)(AI_CLOUD_ALERT_TYPE_E type);

/**
 * @brief ai input alert
 *
 * @param[in] type alert type
 * @param[in] cb alert fallback callback, NULL means no need callback
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 *
 * @note custom_prompt is not supported yet, so it must be NULL
 */
OPERATE_RET tuya_ai_input_alert(AI_CLOUD_ALERT_TYPE_E type, AI_ALERT_FB_CB cb);

/**
 * @brief check ai input is started
 *
 * @return TRUE started, FALSE not started
 */
bool tuya_ai_input_is_started(void);
#endif // __TUYA_AI_INPUT_H__
