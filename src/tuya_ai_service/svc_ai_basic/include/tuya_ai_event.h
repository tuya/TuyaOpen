/**
 * @file tuya_ai_event.h
 * @author tuya
 * @brief ai event
 * @version 0.1
 * @date 2025-03-06
 *
 * @copyright Copyright (c) 2023 Tuya Inc. All Rights Reserved.
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
#ifndef __TUYA_AI_EVENT_H__
#define __TUYA_AI_EVENT_H__


#include "tuya_ai_types.h"

#include "tuya_cloud_com_defs.h"
#include "tuya_cloud_types.h"
#include "tuya_ai_protocol.h"

/**
 * @brief recv event cb
 *
 * @param[in] attr event attr
 * @param[in] head event head
 * @param[in] data event data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET(*AI_EVENT_CB)(AI_EVENT_ATTR_T *event, AI_EVENT_HEAD_T *head, VOID *data);

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

/**
 * @brief event mcp command
 *
 * @param[in] sid session id
 * @param[in] eid event id
 * @param[in] data command data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_event_mcp(AI_SESSION_ID sid, AI_EVENT_ID eid, char *data);

#endif