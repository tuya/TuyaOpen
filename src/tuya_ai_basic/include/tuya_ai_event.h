/**
 * @file tuya_ai_event.h
 * @brief This file contains the implementation of Tuya AI event management,
 * including event generation, processing and session event handling.
 *
 * The Tuya AI event module provides core functionalities for AI event lifecycle
 * management, including event validation, memory allocation and event data
 * packaging. It implements thread-safe event operations with mutex protection.
 *
 * Key features include:
 * - Event parameter validation and error handling
 * - Dynamic memory allocation for event data
 * - Event header structure (AI_EVENT_HEAD_T) handling
 * - Integration with Tuya AI protocol and client layers
 * - Thread-safe operations using mutex mechanisms
 * - Detailed error logging through tal_log system
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_AI_EVENT_H__
#define __TUYA_AI_EVENT_H__

#include "tuya_cloud_com_defs.h"
#include "tuya_cloud_types.h"
#include "tuya_ai_protocol.h"

/**
 * @brief recv event cb
 *
 * @param[in] type event type
 * @param[in] sid session id
 * @param[in] eid event id
 * @param[in] attr user attr
 * @param[in] len user len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET (*AI_EVENT_CB)(AI_EVENT_TYPE type, AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len);

/**
 * @brief event start
 *
 * @param[in] sid session id
 * @param[out] eid event id
 * @param[in] attr attr user data
 * @param[in] len attr user len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_event_start(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len);

/**
 * @brief event end
 *
 * @param[in] sid session id
 * @param[in] eid event id
 * @param[in] attr attr user data
 * @param[in] len attr user len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_event_end(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len);

/**
 * @brief event payloads end
 *
 * @param[in] sid session id
 * @param[in] eid event id
 * @param[in] attr attr user data
 * @param[in] len attr user len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_event_payloads_end(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len);

/**
 * @brief event chat break
 *
 * @param[in] sid session id
 * @param[in] eid event id
 * @param[in] attr attr user data
 * @param[in] len attr user len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_event_chat_break(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len);

/**
 * @brief event one shot
 *
 * @param[in] sid session id
 * @param[in] eid event id
 * @param[in] attr attr user data
 * @param[in] len attr user len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_event_one_shot(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len);
#endif