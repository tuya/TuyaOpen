/**
 * @file tuya_ai_internal.h
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

#ifndef __TUYA_AI_INTERNAL_H__
#define __TUYA_AI_INTERNAL_H__

#include "tuya_cloud_types.h"
#include "tuya_ai_client.h"
#include "tuya_ai_biz.h"
#include "tuya_ai_event.h"
#include "tuya_ai_input.h"
#include "tuya_ai_output.h"

typedef struct {
    AI_PACKET_PT type;
    AI_BIZ_HD_T biz;
    uint32_t len;
    uint32_t total_len;
} AI_RINGBUF_HEAD_T;

/**
 * @brief ai agent upload stream
 *
 * @param[in] ptype packet type
 * @param[in] biz business header
 * @param[in] data stream data
 * @param[in] len stream length
 * @param[in] total_len total stream length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_agent_upload_stream(AI_PACKET_PT ptype, AI_BIZ_HD_T *biz, char *data, uint32_t len, uint32_t total_len);

/**
 * @brief ai agent start
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_agent_start(void);

/**
 * @brief ai agent end
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_agent_end(void);

/**
 * @brief ai input init
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_input_init(void);

/**
 * @brief ai input deinit
 *
 */
void tuya_ai_input_deinit(void);

/**
 * @brief notify alert input is done
 *
 * @param[in] alert_tts_start is alert tts start
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_input_alert_notify(bool alert_tts_start);

/**
 * @brief cloud trigger input handler
 *
 * @param[in] request_id request id
 * @param[in] content content
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_input_cloud_trigger(const char *request_id, const char *content);

/**
 * @brief ai output init
 *
 * @param[in] cbs callback functions
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_output_init(AI_OUTPUT_CBS_T *cbs);

/**
 * @brief ai output deinit
 *
 */
void tuya_ai_output_deinit(void);

/**
 * @brief ai output alert
 *
 * @param[in] type alert type
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_output_alert(AI_ALERT_TYPE_E type);

/**
 * @brief ai output text
 *
 * @param[in] type text type
 * @param[in] root text json data
 * @param[in] eof text eof
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_output_text(char *scode, AI_TEXT_TYPE_E type, cJSON *root, bool eof);

/**
 * @brief ai output media
 *
 * @param[in] type media type
 * @param[in] data media data
 * @param[in] len media length
 * @param[in] total_len media total_len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_output_media(char *scode, AI_PACKET_PT type, char *data, uint32_t len, uint32_t total_len);
#endif // __TUYA_AI_INTERNAL_H__
