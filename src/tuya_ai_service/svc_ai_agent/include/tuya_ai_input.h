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
    BOOL_T multi_session;
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
 *
 * @param[in] scode solution code
 * @param[in] force force start new session(break old session)
 *
 * @return VOID
 */
VOID tuya_ai_input_start_s(CHAR_T *scode, BOOL_T force);
#define tuya_ai_input_start(force) tuya_ai_input_start_s(tuya_ai_agent_get_scode(NULL), force)

/**
 * @brief ai input stop
*
* @param[in] scode solution code
* @return VOID
 */
VOID tuya_ai_input_stop_s(CHAR_T *scode);
#define tuya_ai_input_stop() tuya_ai_input_stop_s(tuya_ai_agent_get_scode(NULL))
/**
 * @brief ai video input
 *
 * @param[in] scode solution code
 * @param[in] timestamp video timestamp
 * @param[in] pts video pts
 * @param[in] frame_type video frame type
 * @param[in] data video data
 * @param[in] len video data length
 * @param[in] total_len video total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_video_input_s(CHAR_T *scode, UINT64_T timestamp, UINT64_T pts, AI_VIDEO_FRAME_TYPE frame_type, BYTE_T *data, UINT_T len, UINT_T total_len);
#define tuya_ai_video_input(timestamp, pts, data, len, total_len) tuya_ai_video_input_s(tuya_ai_agent_get_scode(NULL), timestamp, pts, AI_VIDEO_FRAME_TYPE_I, data, len, total_len)
/**
 * @brief ai audio input direct, no ringbuf
 *
 * @param[in] scode solution code
 * @param[in] timestamp audio timestamp
 * @param[in] pts audio pts
 * @param[in] data audio data
 * @param[in] len audio data length
 * @param[in] total_len audio total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_audio_input_direct_s(CHAR_T *scode, UINT64_T timestamp, UINT64_T pts, BYTE_T *data, UINT_T len, UINT_T total_len);
#define tuya_ai_audio_input_direct(timestamp, pts, data, len, total_len) tuya_ai_audio_input_direct_s(tuya_ai_agent_get_scode(NULL), timestamp, pts, data, len, total_len)

/**
 * @brief ai audio input
 *
 * @param[in] scode solution code
 * @param[in] timestamp audio timestamp
 * @param[in] data audio data
 * @param[in] len audio data length
 * @param[in] total_len audio total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_audio_input_s(CHAR_T *scode, UINT64_T timestamp, UINT64_T pts, BYTE_T *data, UINT_T len, UINT_T total_len);
#define tuya_ai_audio_input(timestamp, pts, data, len, total_len) tuya_ai_audio_input_s(tuya_ai_agent_get_scode(NULL), timestamp, pts, data, len, total_len)

/**
 * @brief ai image input
 *
 * @param[in] scode solution code
 * @param[in] timestamp image timestamp
 * @param[in] data image data
 * @param[in] len image data length
 * @param[in] total_len image total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_image_input_s(CHAR_T *scode, UINT64_T timestamp, BYTE_T *data, UINT_T len, UINT_T total_len);
#define tuya_ai_image_input(timestamp, data, len, total_len) tuya_ai_image_input_s(tuya_ai_agent_get_scode(NULL), timestamp, data, len, total_len)

/**
 * @brief ai text input
 *
 * @param[in] scode solution code
 * @param[in] data text data
 * @param[in] len text data length
 * @param[in] total_len text total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_text_input_s(CHAR_T *scode, BYTE_T *data, UINT_T len, UINT_T total_len);
#define tuya_ai_text_input(data, len, total_len) tuya_ai_text_input_s(tuya_ai_agent_get_scode(NULL), data, len, total_len)

/**
 * @brief ai file input
 *
 * @param[in] scode solution code
 * @param[in] data file data
 * @param[in] len file data length
 * @param[in] total_len file total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_file_input_s(CHAR_T *scode, BYTE_T *data, UINT_T len, UINT_T total_len);
#define tuya_ai_file_input(data, len, total_len) tuya_ai_file_input_s(tuya_ai_agent_get_scode(NULL), data, len, total_len)

/* AI cloud alert type */
typedef INT_T AI_CLOUD_ALERT_TYPE_E;

/**
 * @brief ai input alert
 *
 * @param[in] type alert type
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 *
 * @note custom_prompt is not supported yet, so it must be NULL
 */
OPERATE_RET tuya_ai_input_alert(AI_CLOUD_ALERT_TYPE_E type);

/**
 * @brief ai input alert with custom alert type
 *
 * @param[in] type alert type
 * @param[in] event_type custom event type string
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_input_alert_custom(AI_CLOUD_ALERT_TYPE_E type, CONST CHAR_T *event_type);

/**
 * @brief check ai input is started
 *
 * @param[in] scode solution code
 *
 * @return TRUE started, FALSE not started
 */
BOOL_T tuya_ai_input_is_started_s(CHAR_T *scode);
#define tuya_ai_input_is_started() tuya_ai_input_is_started_s(tuya_ai_agent_get_scode(NULL))

/**
 *  @brief get how many bytes can ai input ringbuf receive
 *
 *  @param[out] ptr_to_capacity_out pointer to receive result
 *  @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_input_get_buffer_free_capacity_s(CHAR_T *scode, SIZE_T* ptr_to_capacity_out);
#define tuya_ai_input_get_buffer_free_capacity(ptr_to_capacity_out) tuya_ai_input_get_buffer_free_capacity_s(tuya_ai_agent_get_scode(NULL), ptr_to_capacity_out)

/**
 * @brief Mute or unmute audio input
 *
 * @param[in] mute TRUE: mute audio input; FALSE: unmute audio input
 *
 * @return none
 */
VOID tuya_ai_input_mute_audio(BOOL_T mute);

#endif // __TUYA_AI_INPUT_H__
