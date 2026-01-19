/**
 * @file tuya_ai_input.c
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
#include "tal_memory.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "tal_mutex.h"
#include "tuya_ringbuf.h"
#include "uni_log.h"
#include "tuya_ai_agent.h"
#include "tuya_ai_biz.h"
#include "base_event_info.h"
#include "base_event.h"
#include "tuya_ai_internal.h"
#include "tuya_ai_input.h"
#include "tal_queue.h"
#include "tal_sw_timer.h"
#include "tal_workq_service.h"

#ifndef AI_INPUT_STACK_SIZE
#define AI_INPUT_STACK_SIZE (4608)
#endif
#ifndef AI_INPUT_RINGBUF_SIZE
#define AI_INPUT_RINGBUF_SIZE (20*1024)
#endif
#ifndef AI_INPUT_BUF_SIZE
#define AI_INPUT_BUF_SIZE (6*1024)
#endif

#define AI_INPUT_TASK_DELAY   (80)

#define AI_ALERT_DEFAULT_TIMEOUT   (1500) // ms

typedef enum {
    /** alert idle state */
    AI_ALERT_IDLE = 0,
    /** alert wait response state */
    AI_ALERT_WAIT_RESP,
    /** alert timeout state */
    AI_ALERT_TIMEOUT,
} AI_ALERT_STATE_E;

typedef struct {
    AI_ALERT_STATE_E state;
    TIMER_ID timer;
    AI_ALERT_FB_CB cb;
    AI_CLOUD_ALERT_TYPE_E type;
} AI_ALERT_CTX_T;

typedef struct {
    THREAD_HANDLE thread;
    AI_INPUT_STATE_E state;
    TUYA_RINGBUFF_T ringbuf;
    uint32_t lazy_input;
    MUTEX_HANDLE mutex;
    QUEUE_HANDLE queue;
    char *input_buf;
    bool terminate;
    bool queue_sync;
    AI_ALERT_CTX_T alert;
} AI_INPUT_CTX_T;
STATIC AI_INPUT_CTX_T ai_input_ctx;

STATIC VOID_T __alert_timeout_cb(TIMER_ID timer_id, VOID_T *arg);

OPERATE_RET tuya_ai_input_write(AI_RINGBUF_HEAD_T *head, uint8_t *data)
{
    uint32_t rt = 0;
    // if ((ai_input_ctx.state != AI_INPUT_PROC) && (ai_input_ctx.state != AI_INPUT_STOPPING)) {
    //     return OPRT_OK;
    // }

    if (data == NULL || head->len == 0) {
        return OPRT_OK;
    }
    if (head->len > AI_INPUT_BUF_SIZE) {
        PR_ERR("input data len is too long %d, type:%d", head->len, head->type);
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ai_input_ctx.mutex);
    uint32_t free_size = tuya_ring_buff_free_size_get(ai_input_ctx.ringbuf);
    if (free_size < (SIZEOF(AI_RINGBUF_HEAD_T) + head->len)) {
        tal_mutex_unlock(ai_input_ctx.mutex);
        return OPRT_RESOURCE_NOT_READY;
    }
    rt = tuya_ring_buff_write(ai_input_ctx.ringbuf, (VOID_T *)head, SIZEOF(AI_RINGBUF_HEAD_T));
    if (rt != SIZEOF(AI_RINGBUF_HEAD_T)) {
        PR_ERR("input ring buf write head failed %d %d", rt, SIZEOF(AI_RINGBUF_HEAD_T));
        goto EXIT;
    }
    rt = tuya_ring_buff_write(ai_input_ctx.ringbuf, data, head->len);
    if (rt != head->len) {
        PR_ERR("ring buf write pbuf failed %d %d", rt, head->len);
        goto EXIT;
    }
    tal_mutex_unlock(ai_input_ctx.mutex);
    return OPRT_OK;

EXIT:
    tuya_ring_buff_reset(ai_input_ctx.ringbuf);
    ai_input_ctx.state = AI_INPUT_STOP;
    tal_mutex_unlock(ai_input_ctx.mutex);
    return OPRT_COM_ERROR;
}

OPERATE_RET tuya_ai_input_read(AI_RINGBUF_HEAD_T *head, char *buf)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t read_len = 0, total_len = 0;

    if (ai_input_ctx.ringbuf == NULL) {
        PR_ERR("ring buffer is not initialized");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(ai_input_ctx.mutex);
    while (total_len < AI_INPUT_BUF_SIZE) {
        if (ai_input_ctx.state == AI_INPUT_STOP) {
            break;
        }
        read_len = tuya_ring_buff_peek(ai_input_ctx.ringbuf, (VOID_T *)head, SIZEOF(AI_RINGBUF_HEAD_T));
        if (read_len != SIZEOF(AI_RINGBUF_HEAD_T)) {
            break;
        }
        if (head->len + total_len > AI_INPUT_BUF_SIZE) {
            break;
        }
        read_len = tuya_ring_buff_read(ai_input_ctx.ringbuf, head, SIZEOF(AI_RINGBUF_HEAD_T));
        if (read_len != SIZEOF(AI_RINGBUF_HEAD_T)) {
            break;
        }
        read_len = tuya_ring_buff_read(ai_input_ctx.ringbuf, buf + total_len, head->len);
        if (read_len != head->len) {
            PR_ERR("ring buf read pbuf failed %d %d", read_len, head->len);
            rt = OPRT_COM_ERROR;
            break;
        }
        total_len += read_len;
    }
    head->len = total_len;
    head->total_len = total_len;
    tal_mutex_unlock(ai_input_ctx.mutex);
    return rt;
}

bool tuya_ai_input_is_started(VOID)
{
    uint32_t cnt = 0;
    while (!ai_input_ctx.queue_sync) {
        tal_system_sleep(50);
        if (cnt++ > 100) {
            PR_ERR("input failed, ai input not started");
            return FALSE;
        }
    }
    return TRUE;
}

OPERATE_RET tuya_ai_video_input(uint64_t timestamp, uint64_t pts, uint8_t *data, uint32_t len, uint32_t total_len)
{
    if (!tuya_ai_input_is_started()) {
        return OPRT_RESOURCE_NOT_READY;
    }
    AI_BIZ_HD_T biz = {0};
    biz.video.timestamp = timestamp;
    biz.video.pts = pts;
    return tuya_ai_agent_upload_stream(AI_PT_VIDEO, &biz, (char *)data, len, total_len);
}

OPERATE_RET tuya_ai_audio_input_direct(uint64_t timestamp, uint64_t pts, uint8_t *data, uint32_t len, uint32_t total_len)
{
    if (!tuya_ai_input_is_started()) {
        return OPRT_RESOURCE_NOT_READY;
    }
    AI_BIZ_HD_T biz = {0};
    biz.audio.timestamp = timestamp;
    biz.audio.pts = pts;
    return tuya_ai_agent_upload_stream(AI_PT_AUDIO, &biz, (char *)data, len, total_len);
}

OPERATE_RET tuya_ai_audio_input(uint64_t timestamp, uint64_t pts, uint8_t *data, uint32_t len, uint32_t total_len)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t cnt = 0;
    AI_RINGBUF_HEAD_T head = {0};
    head.type = AI_PT_AUDIO;
    head.len = len;
    head.total_len = total_len;
    head.biz.audio.timestamp = timestamp;
    head.biz.audio.pts = pts;
    rt = tuya_ai_input_write(&head, data);
    while (rt == OPRT_RESOURCE_NOT_READY) {
        tal_system_sleep(10);
        rt = tuya_ai_input_write(&head, data);
        if (cnt++ > 1000) {
            PR_ERR("audio input failed %d", rt);
            break;
        }
    }
    return rt;
}

OPERATE_RET tuya_ai_image_input(uint64_t timestamp, uint8_t *data, uint32_t len, uint32_t total_len)
{
    if (!tuya_ai_input_is_started()) {
        return OPRT_RESOURCE_NOT_READY;
    }
    AI_BIZ_HD_T biz = {0};
    biz.image.timestamp = timestamp;
    return tuya_ai_agent_upload_stream(AI_PT_IMAGE, &biz, (char *)data, len, total_len);
}

OPERATE_RET tuya_ai_text_input(uint8_t *data, uint32_t len, uint32_t total_len)
{
    if (!tuya_ai_input_is_started()) {
        return OPRT_RESOURCE_NOT_READY;
    }
    return tuya_ai_agent_upload_stream(AI_PT_TEXT, NULL, (char *)data, len, total_len);
}

OPERATE_RET tuya_ai_file_input(uint8_t *data, uint32_t len, uint32_t total_len)
{
    if (!tuya_ai_input_is_started()) {
        return OPRT_RESOURCE_NOT_READY;
    }
    return tuya_ai_agent_upload_stream(AI_PT_FILE, NULL, (char *)data, len, total_len);
}

VOID tuya_ai_input_start(bool force)
{
    OPERATE_RET rt = OPRT_OK;
    if (!tuya_ai_agent_is_ready()) {
        return;
    }

    AI_INPUT_STATE_E state = AI_INPUT_IDLE;
    if (force) {
        state = AI_INPUT_START;
    } else {
        state = AI_INPUT_START_LAZY;
    }

    ai_input_ctx.queue_sync = FALSE;
    rt = tal_queue_post(ai_input_ctx.queue, &state, 0);
    if (OPRT_OK != rt) {
        PR_ERR("queue post err, rt:%d", rt);
    } else {
        PR_DEBUG("ai input start, state:%d", state);
    }
    return;
}

AI_INPUT_STATE_E tuya_ai_input_get_state(VOID)
{
    return ai_input_ctx.state;
}

VOID tuya_ai_input_stop(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t cnt = 0;
    if (!tuya_ai_agent_is_ready()) {
        return;
    }
    AI_INPUT_STATE_E state = AI_INPUT_STOPPING;
    ai_input_ctx.lazy_input = 0;
    ai_input_ctx.queue_sync = FALSE;
    rt = tal_queue_post(ai_input_ctx.queue, &state, 0);
    if (OPRT_OK != rt) {
        PR_ERR("queue post err, rt:%d", rt);
    } else {
        PR_DEBUG("ai input stop, state:%d", state);
        while (!ai_input_ctx.queue_sync) {
            tal_system_sleep(100);
            if (cnt++ >= 5) {
                PR_ERR("ai input stop timeout, cnt:%d", cnt);
                break;
            }
        }
    }
    return;
}

VOID tuya_ai_input_deinit(VOID)
{
    ai_input_ctx.terminate = TRUE;
}

STATIC VOID __ai_input_free(VOID)
{
    if (ai_input_ctx.thread) {
        tal_thread_delete(ai_input_ctx.thread);
        ai_input_ctx.thread = NULL;
    }
    if (ai_input_ctx.ringbuf) {
        tuya_ring_buff_free(ai_input_ctx.ringbuf);
        ai_input_ctx.ringbuf = NULL;
    }
    if (ai_input_ctx.input_buf) {
        Free(ai_input_ctx.input_buf);
        ai_input_ctx.input_buf = NULL;
    }
    if (ai_input_ctx.mutex) {
        tal_mutex_release(ai_input_ctx.mutex);
        ai_input_ctx.mutex = NULL;
    }
    if (ai_input_ctx.queue) {
        tal_queue_free(ai_input_ctx.queue);
        ai_input_ctx.queue = NULL;
    }
    if (ai_input_ctx.alert.timer) {
        tal_sw_timer_delete(ai_input_ctx.alert.timer);
        ai_input_ctx.alert.timer = NULL;
    }
    memset(&ai_input_ctx, 0, SIZEOF(ai_input_ctx));
    PR_DEBUG("ai input deinit");
}

STATIC VOID __ai_input_thread(VOID* arg)
{
    OPERATE_RET rt = OPRT_OK;
    AI_INPUT_STATE_E queue_state = AI_INPUT_IDLE;
    AI_RINGBUF_HEAD_T head = {0};

    while (!ai_input_ctx.terminate && tal_thread_get_state(ai_input_ctx.thread) == THREAD_STATE_RUNNING) {
        queue_state = ai_input_ctx.state;
        if (tal_queue_fetch(ai_input_ctx.queue, &queue_state, 0) == 0) {
            PR_DEBUG("recv queue state %d", queue_state);
        }
        switch (queue_state) {
        case AI_INPUT_START: {
            tuya_ai_agent_event(AI_EVENT_CHAT_BREAK, 0);
            tuya_ai_output_stop(TRUE);
            if (tuya_ai_agent_is_ready()) {
                ai_input_ctx.state = AI_INPUT_PROC;
                rt = tuya_ai_agent_start();
                if (OPRT_OK != rt) {
                    ai_input_ctx.state = AI_INPUT_STOP;
                }
            } else {
                tuya_ai_output_alert(AT_NETWORK_FAIL);
                ai_input_ctx.state = AI_INPUT_IDLE;
            }
            PR_DEBUG("start queue sync");
            ai_input_ctx.queue_sync = TRUE;
        }
        break;
        case AI_INPUT_START_LAZY: {
            if (tuya_ai_agent_is_ready()) {
                ai_input_ctx.state = AI_INPUT_PROC;
                rt = tuya_ai_agent_start();
                if (OPRT_OK != rt) {
                    ai_input_ctx.state = AI_INPUT_STOP;
                }
            } else {
                tuya_ai_output_alert(AT_NETWORK_FAIL);
                ai_input_ctx.state = AI_INPUT_IDLE;
            }
            PR_DEBUG("start lazy queue sync");
            ai_input_ctx.queue_sync = TRUE;
        }
        break;
        case AI_INPUT_PROC: {
            tal_mutex_lock(ai_input_ctx.mutex);
            rt = tuya_ai_input_read(&head, ai_input_ctx.input_buf);
            tal_mutex_unlock(ai_input_ctx.mutex);
            if ((rt == OPRT_OK) && (head.len > 0)) {
                tuya_ai_agent_upload_stream(head.type, &head.biz, ai_input_ctx.input_buf, head.len, head.total_len);
            }
        }
        break;
        case AI_INPUT_STOPPING: {
            if (ai_input_ctx.lazy_input++ < 30) {
                tal_mutex_lock(ai_input_ctx.mutex);
                rt = tuya_ai_input_read(&head, ai_input_ctx.input_buf);
                tal_mutex_unlock(ai_input_ctx.mutex);
                if ((rt == OPRT_OK) && (head.len > 0)) {
                    tuya_ai_agent_upload_stream(head.type, &head.biz, ai_input_ctx.input_buf, head.len, head.total_len);
                    ai_input_ctx.state = AI_INPUT_STOPPING;
                } else {
                    ai_input_ctx.state = AI_INPUT_STOP;
                }
            } else {
                ai_input_ctx.state = AI_INPUT_STOP;
            }
        }
        break;
        case AI_INPUT_STOP: {
            tuya_ai_agent_end();
            tal_mutex_lock(ai_input_ctx.mutex);
            tuya_ring_buff_reset(ai_input_ctx.ringbuf);
            tal_mutex_unlock(ai_input_ctx.mutex);
            ai_input_ctx.state = AI_INPUT_IDLE;
            ai_input_ctx.queue_sync = TRUE;
        }
        break;
        case AI_INPUT_IDLE:
            break;
        default:
            break;
        }

        if (ai_input_ctx.state != AI_INPUT_STOPPING) {
            tal_system_sleep(AI_INPUT_TASK_DELAY);
        } else {
            tal_system_sleep(10);
        }
    }
    __ai_input_free();
}

OPERATE_RET tuya_ai_input_init(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    if (ai_input_ctx.input_buf) {
        return OPRT_OK;
    }
    memset(&ai_input_ctx, 0, SIZEOF(ai_input_ctx));
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&ai_input_ctx.mutex));
    TUYA_CALL_ERR_GOTO(tal_queue_create_init(&ai_input_ctx.queue, SIZEOF(AI_INPUT_STATE_E), 3), EXIT);
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__alert_timeout_cb, NULL, &ai_input_ctx.alert.timer), EXIT);
    ai_input_ctx.input_buf = Malloc(AI_INPUT_BUF_SIZE);
    TUYA_CHECK_NULL_GOTO(ai_input_ctx.input_buf, EXIT);
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(AI_INPUT_RINGBUF_SIZE, OVERFLOW_PSRAM_STOP_TYPE, &ai_input_ctx.ringbuf),
                       EXIT);
#else
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(AI_INPUT_RINGBUF_SIZE, OVERFLOW_STOP_TYPE, &ai_input_ctx.ringbuf), EXIT);
#endif

    THREAD_CFG_T thrd_param = {0};
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "ai_agent_input";
    thrd_param.stackDepth = AI_INPUT_STACK_SIZE;
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    thrd_param.psram_mode = 1;
#endif
    rt = tal_thread_create_and_start(&ai_input_ctx.thread, NULL, NULL, __ai_input_thread, NULL, &thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("ai inpput thread create err, rt:%d", rt);
        goto EXIT;
    }
    PR_DEBUG("ai input init success");
    return rt;

EXIT:
    __ai_input_free();
    return rt;
}

OPERATE_RET tuya_ai_input_alert(AI_CLOUD_ALERT_TYPE_E type, AI_ALERT_FB_CB cb)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("[cloud] alert type: %d", type);

    // get network status
    if (!tuya_ai_agent_is_ready()) {
        PR_ERR("network is not ready");
        return OPRT_COM_ERROR;
    }

    ty_cJSON *json = ty_cJSON_CreateObject();
    if (json == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    tuya_ai_input_stop();
    tuya_ai_output_stop(TRUE);
    tuya_ai_agent_set_scode(AI_AGENT_SCODE_ALERT);
    char eid[AI_UUID_V4_LEN + 1] = {0};
    tuya_ai_basic_uuid_v4(eid);
    memcpy(eid, AI_ALERT_PLAY_ID, AI_ALERT_PLAY_ID_LEN);    // set eid start from AI_ALERT_PLAY_ID
    tuya_ai_agent_set_eid(eid);
    tuya_ai_input_start(FALSE);
    tal_mutex_lock(ai_input_ctx.mutex);
    tal_sw_timer_stop(ai_input_ctx.alert.timer);
    ai_input_ctx.alert.state = AI_ALERT_IDLE;
    ai_input_ctx.alert.cb = cb;
    ai_input_ctx.alert.type = type;
    tal_mutex_unlock(ai_input_ctx.mutex);

    // eg. alert_prompt = {"type":"hintTone","eventType":"networkConnected"}
    ty_cJSON_AddStringToObject(json, "type", "hintTone");
    switch (type) {
    case AT_NETWORK_CONNECTED:
        ty_cJSON_AddStringToObject(json, "eventType", "networkConnected");
        break;
    case AT_PLEASE_AGAIN:
        ty_cJSON_AddStringToObject(json, "eventType", "pleaseAgain");
        break;
    case AT_WAKEUP:
        ty_cJSON_AddStringToObject(json, "eventType", "wakeUp");
        break;
    case AT_LONG_KEY_TALK:
        ty_cJSON_AddStringToObject(json, "eventType", "talkModeSwitch_longPressTalk");
        break;
    case AT_KEY_TALK:
        ty_cJSON_AddStringToObject(json, "eventType", "talkModeSwitch_pressTalk");
        break;
    case AT_WAKEUP_TALK:
        ty_cJSON_AddStringToObject(json, "eventType", "talkModeSwitch_wakeWordTalk");
        break;
    case AT_RANDOM_TALK:
        ty_cJSON_AddStringToObject(json, "eventType", "talkModeSwitch_continuousTalk");
        break;
    default:
        ty_cJSON_Delete(json);
        PR_WARN("alert type %d is not support", type);
        return OPRT_NOT_SUPPORTED;
    }
    char *alert_prompt = ty_cJSON_PrintUnformatted(json);
    if (alert_prompt == NULL) {
        ty_cJSON_Delete(json);
        return OPRT_MALLOC_FAILED;
    }
    ty_cJSON_Delete(json);
    tuya_ai_text_input((uint8_t *)alert_prompt, strlen(alert_prompt), strlen(alert_prompt));
    Free(alert_prompt);

    tuya_ai_input_stop();

    // wait for tts input use timer
    tal_mutex_lock(ai_input_ctx.mutex);
    ai_input_ctx.alert.state = AI_ALERT_WAIT_RESP;
    tal_sw_timer_start(ai_input_ctx.alert.timer, AI_ALERT_DEFAULT_TIMEOUT, TAL_TIMER_ONCE);
    tal_mutex_unlock(ai_input_ctx.mutex);

    return rt;
}

OPERATE_RET tuya_ai_input_alert_notify(bool alert_tts_start)
{
    tal_mutex_lock(ai_input_ctx.mutex);
    if (ai_input_ctx.alert.state == AI_ALERT_TIMEOUT) {
        tal_mutex_unlock(ai_input_ctx.mutex);
        return OPRT_TIMEOUT;
    }

    if (alert_tts_start && ai_input_ctx.alert.timer) {
        PR_DEBUG("alert tts start, stop alert timer");
        tal_sw_timer_stop(ai_input_ctx.alert.timer);
        ai_input_ctx.alert.state = AI_ALERT_IDLE;
    }
    tal_mutex_unlock(ai_input_ctx.mutex);
    return OPRT_OK;
}

STATIC VOID_T __alert_timeout_cb(TIMER_ID timer_id, VOID_T *arg)
{
    PR_WARN("alert input timeout");
    tal_mutex_lock(ai_input_ctx.mutex);
    if (ai_input_ctx.alert.state != AI_ALERT_WAIT_RESP) {
        tal_mutex_unlock(ai_input_ctx.mutex);
        return;
    }
    ai_input_ctx.alert.state = AI_ALERT_TIMEOUT;
    AI_ALERT_FB_CB cb = ai_input_ctx.alert.cb;
    AI_CLOUD_ALERT_TYPE_E type = ai_input_ctx.alert.type;
    tal_mutex_unlock(ai_input_ctx.mutex);
    if (cb) {
        cb(type);
    }
}

typedef struct {
    const char *request_id;
    const char *content;
} AI_CLOUD_TRIGGER_T;

VOID __cloud_trigger_wq(VOID *data)
{
    AI_CLOUD_TRIGGER_T *trigger = (AI_CLOUD_TRIGGER_T *)data;
    if (trigger == NULL) {
        return;
    }
    const char *request_id = trigger->request_id;
    const char *content = trigger->content;
    if (request_id && content) {
        tuya_ai_input_stop();
        tuya_ai_output_stop(TRUE);
        tuya_ai_agent_set_scode(AI_AGENT_SCODE_DEFAULT);
        tuya_ai_agent_set_eid((char *)request_id);
        tuya_ai_input_start(FALSE);
        tuya_ai_text_input((uint8_t *)content, strlen(content), strlen(content));
        tuya_ai_input_stop();
    }

    if (trigger->request_id) {
        tal_free((char *)trigger->request_id);
    }
    if (trigger->content) {
        tal_free((char *)trigger->content);
    }
    tal_free(trigger);
}

OPERATE_RET tuya_ai_input_cloud_trigger(const char *request_id, const char *content)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("cloud trigger request_id: %s, content: %s", request_id, content);

    if (request_id == NULL || content == NULL) {
        return OPRT_INVALID_PARM;
    }

    AI_CLOUD_TRIGGER_T *trigger = (AI_CLOUD_TRIGGER_T *)tal_malloc(SIZEOF(AI_CLOUD_TRIGGER_T));
    if (trigger == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(trigger, 0, SIZEOF(AI_CLOUD_TRIGGER_T));
    trigger->request_id = mm_strdup(request_id);
    trigger->content = mm_strdup(content);
    if (trigger->request_id == NULL || trigger->content == NULL) {
        rt = OPRT_MALLOC_FAILED;
        goto err;
    }
    TUYA_CALL_ERR_GOTO(tal_workq_schedule(WORKQ_SYSTEM, __cloud_trigger_wq, trigger), err);
    return rt;
err:
    if (trigger && trigger->request_id) {
        tal_free((char *)trigger->request_id);
    }
    if (trigger && trigger->content) {
        tal_free((char *)trigger->content);
    }
    if (trigger) {
        tal_free(trigger);
    }
    return rt;
}
