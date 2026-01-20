/**
 * @file tuya_ai_event.c
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
#include <stdio.h>
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_mutex.h"
#include "uni_random.h"
#include "uni_log.h"
#include "tal_memory.h"
#include "tuya_ai_protocol.h"
#include "tuya_ai_client.h"
#include "tuya_ai_event.h"
#include "tuya_ai_private.h"

STATIC OPERATE_RET __ai_event(AI_EVENT_ATTR_T *event, AI_EVENT_HEAD_T *head, char *payload)
{
    OPERATE_RET rt = OPRT_OK;
    if (event == NULL || (NULL == event->session_id) ||
        (NULL == event->event_id) || strlen(event->session_id) == 0 ||
        strlen(event->event_id) == 0) {
        PR_ERR("event or session id was null");
        return OPRT_INVALID_PARM;
    }

    uint32_t data_len = SIZEOF(AI_EVENT_HEAD_T) + head->length;
    char *event_data = OS_MALLOC(data_len);
    if (event_data == NULL) {
        PR_ERR("malloc failed");
        return OPRT_MALLOC_FAILED;
    }
    memset(event_data, 0, data_len);
    AI_EVENT_HEAD_T *event_head = (AI_EVENT_HEAD_T *)event_data;
    event_head->type = UNI_HTONS(head->type);
    event_head->length = UNI_HTONS(head->length);
    if (payload && head->length) {
        memcpy(event_data + SIZEOF(AI_EVENT_HEAD_T), payload, head->length);
    }
    rt = tuya_ai_basic_event(event, event_data, data_len, NULL);
    OS_FREE(event_data);
    PR_DEBUG("send event rt:%d, type:%d", rt, head->type);
    return rt;
}

OPERATE_RET tuya_ai_event_start(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    if (eid == NULL) {
        return OPRT_INVALID_PARM;
    } else if (eid[0] == '\0') {
        // generate event id if not provided
        rt = tuya_ai_basic_uuid_short(eid);
        if (OPRT_OK != rt) {
            PR_ERR("create event id failed, rt:%d", rt);
            return rt;
        }
    }

    AI_EVENT_ATTR_T event = {0};
    event.session_id = sid;
    event.event_id = eid;
    event.user_data = attr;
    event.user_len = len;
    AI_EVENT_HEAD_T head = {0};
    head.type = AI_EVENT_START;
    head.length = 0;
    rt = __ai_event(&event, &head, NULL);
    if (OPRT_OK != rt) {
        return rt;
    }
    PR_NOTICE("event id is %s", eid);
    return rt;
}

OPERATE_RET tuya_ai_event_payloads_end(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len)
{
    AI_EVENT_ATTR_T event = {0};
    event.session_id = sid;
    event.event_id = eid;
    event.user_data = attr;
    event.user_len = len;
    AI_EVENT_HEAD_T head = {0};
    head.type = AI_EVENT_PAYLOADS_END;
    head.length = 0;
    return __ai_event(&event, &head, NULL);
}

OPERATE_RET tuya_ai_event_end(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len)
{
    AI_EVENT_ATTR_T event = {0};
    event.session_id = sid;
    event.event_id = eid;
    event.user_data = attr;
    event.user_len = len;
    AI_EVENT_HEAD_T head = {0};
    head.type = AI_EVENT_END;
    head.length = 0;
    return __ai_event(&event, &head, NULL);
}

OPERATE_RET tuya_ai_event_chat_break(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len)
{
    AI_EVENT_ATTR_T event = {0};
    event.session_id = sid;
    event.event_id = eid;
    event.user_data = attr;
    event.user_len = len;
    AI_EVENT_HEAD_T head = {0};
    head.type = AI_EVENT_CHAT_BREAK;
    head.length = 0;
    return __ai_event(&event, &head, NULL);
}

OPERATE_RET tuya_ai_event_one_shot(AI_SESSION_ID sid, AI_EVENT_ID eid, uint8_t *attr, uint32_t len)
{
    AI_EVENT_ATTR_T event = {0};
    event.session_id = sid;
    event.event_id = eid;
    event.user_data = attr;
    event.user_len = len;
    AI_EVENT_HEAD_T head = {0};
    head.type = AI_EVENT_ONE_SHOT;
    head.length = 0;
    return __ai_event(&event, &head, NULL);
}

OPERATE_RET tuya_ai_event_mcp(AI_SESSION_ID sid, AI_EVENT_ID eid, char *data)
{
    AI_EVENT_ATTR_T event = {0};
    event.session_id = sid;
    event.event_id = eid;
    AI_EVENT_HEAD_T head = {0};
    head.type = AI_EVENT_MCP_CMD;
    head.length = strlen(data);
    if (event.event_id == NULL || event.event_id[0] == '\0') {
        char tmp_eid[AI_UUID_V4_LEN + 1] = {0};
        tuya_ai_basic_uuid_short(tmp_eid);
        event.event_id = tmp_eid;
        return __ai_event(&event, &head, data);
    }
    return __ai_event(&event, &head, data);
}
