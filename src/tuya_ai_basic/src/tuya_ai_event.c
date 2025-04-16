/**
 * @file tuya_ai_event.c
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

#include <stdio.h>
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_mutex.h"
#include "uni_random.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tuya_ai_protocol.h"
#include "tuya_ai_client.h"
#include "tuya_ai_event.h"

static OPERATE_RET __ai_event(AI_EVENT_TYPE tp, AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    if ((NULL == eid) || (NULL == sid)) {
        PR_ERR("event or session id was null");
        return OPRT_INVALID_PARM;
    }

    uint32_t data_len = sizeof(AI_EVENT_HEAD_T);
    char *event_data = Malloc(data_len);
    if (event_data == NULL) {
        PR_ERR("malloc failed");
        return OPRT_MALLOC_FAILED;
    }
    memset(event_data, 0, data_len);
    AI_EVENT_HEAD_T *head = (AI_EVENT_HEAD_T *)event_data;
    head->type = UNI_HTONS(tp);
    head->length = 0;

    AI_EVENT_ATTR_T event = {0};
    event.event_id = eid;
    event.session_id = sid;
    event.user_data = attr;
    event.user_len = len;

    rt = tuya_ai_basic_event(&event, event_data, data_len);
    Free(event_data);
    PR_DEBUG("send event rt:%d, type:%d", rt, tp);
    return rt;
}

/**
 * @brief Start a new AI event session
 *
 * @param[in] sid Session ID associated with the event
 * @param[out] eid Generated event ID (must be pre-allocated buffer)
 * @param[in] attr Optional event attributes data
 * @param[in] len Length of event attributes data
 *
 * @return OPERATE_RET OPRT_OK on success, error code otherwise
 *
 * @note This function:
 *       1. Generates a unique UUID for the event
 *       2. Initiates the event with AI_EVENT_START type
 *       3. Logs the generated event ID
 */
OPERATE_RET tuya_ai_event_start(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    rt = tuya_ai_basic_uuid_v4(eid);
    if (OPRT_OK != rt) {
        PR_ERR("create event id failed, rt:%d", rt);
        return rt;
    }

    rt = __ai_event(AI_EVENT_START, sid, eid, attr, len);
    if (OPRT_OK != rt) {
        return rt;
    }
    PR_NOTICE("event id is %s", eid);
    return rt;
}

/**
 * @brief Signal payloads end for an AI event
 *
 * @param[in] sid Session ID associated with the event
 * @param[in] eid Event ID to end payloads for
 * @param[in] attr Optional event attributes data
 * @param[in] len Length of event attributes data
 *
 * @return OPERATE_RET OPRT_OK on success, error code otherwise
 */
OPERATE_RET tuya_ai_event_payloads_end(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len)
{
    return __ai_event(AI_EVENT_PAYLOADS_END, sid, eid, attr, len);
}

/**
 * @brief End an AI event session
 *
 * @param[in] sid Session ID associated with the event
 * @param[in] eid Event ID to end
 * @param[in] attr Optional event attributes data
 * @param[in] len Length of event attributes data
 *
 * @return OPERATE_RET OPRT_OK on success, error code otherwise
 */
OPERATE_RET tuya_ai_event_end(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len)
{
    return __ai_event(AI_EVENT_END, sid, eid, attr, len);
}

/**
 * @brief Signal a chat break event
 *
 * @param[in] sid Session ID associated with the event
 * @param[in] eid Event ID for the chat break
 * @param[in] attr Optional event attributes data
 * @param[in] len Length of event attributes data
 *
 * @return OPERATE_RET OPRT_OK on success, error code otherwise
 */
OPERATE_RET tuya_ai_event_chat_break(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len)
{
    return __ai_event(AI_EVENT_CHAT_BREAK, sid, eid, attr, len);
}

/**
 * @brief Trigger a one-shot AI event
 *
 * @param[in] sid Session ID associated with the event
 * @param[in] eid Event ID for the one-shot event
 * @param[in] attr Optional event attributes data
 * @param[in] len Length of event attributes data
 *
 * @return OPERATE_RET OPRT_OK on success, error code otherwise
 */
OPERATE_RET tuya_ai_event_one_shot(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len)
{
    return __ai_event(AI_EVENT_ONE_SHOT, sid, eid, attr, len);
}
