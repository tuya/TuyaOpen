/**
 * @file tuya_ai_output.c
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

#include "uni_log.h"
#include "tuya_ws_db.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "tal_memory.h"
#include "tal_mutex.h"
#include "tuya_ringbuf.h"
#include "tuya_list.h"
#include "tuya_ai_agent.h"
#include "tuya_ai_output.h"
#include "tuya_ai_internal.h"
#include "tal_time_service.h"
#include "mix_method.h"

#ifndef AI_OUTPUT_STACK_SIZE
#define AI_OUTPUT_STACK_SIZE (4*1024)
#endif
#ifndef AI_OUTPUT_RINGBUF_SIZE
#define AI_OUTPUT_RINGBUF_SIZE (20*1024)
#endif
#ifndef AI_OUTPUT_BUF_SIZE
#define AI_OUTPUT_BUF_SIZE (5*1024)
#endif

#define AI_OUTPUT_TASK_DELAY  (100)

typedef enum {
    AI_OUTPUT_STOPPED,
    AI_OUTPUT_PLAYING,
    AI_OUTPUT_STOPPING,
    AI_OUTPUT_IDLE,
    AI_OUTPUT_MAX
} AI_OUTPUT_STATUS_E;

typedef struct {
    char *scode;
    AI_OUTPUT_CBS_T cbs;
    AI_OUTPUT_CBS_MODE_E mode;
    LIST_HEAD node;
} AI_OUTPUT_CBS_NODE_T;

typedef struct {
    THREAD_HANDLE thread;
    AI_OUTPUT_CBS_T cbs;        // default cbs
    LIST_HEAD ext_cbs;          // external cbs list
    AI_OUTPUT_STATUS_E status;
    TUYA_RINGBUFF_T ringbuf;
    MUTEX_HANDLE mutex;
    char *output_buf;
    uint32_t offset;
    bool terminate;
} AUDIO_PLAYER_CTX_T;
STATIC AUDIO_PLAYER_CTX_T ai_output_ctx;

void tuya_ai_output_deinit(void)
{
    ai_output_ctx.terminate = TRUE;
}

#if (AI_OUTPUT_RINGBUF_SIZE > 0)
STATIC void __ai_output_free(void)
{
    if (ai_output_ctx.thread) {
        tal_thread_delete(ai_output_ctx.thread);
        ai_output_ctx.thread = NULL;
    }
    if (ai_output_ctx.mutex) {
        tal_mutex_release(ai_output_ctx.mutex);
        ai_output_ctx.mutex = NULL;
    }
    if (ai_output_ctx.output_buf) {
        Free(ai_output_ctx.output_buf);
        ai_output_ctx.output_buf = NULL;
    }
    if (ai_output_ctx.ringbuf) {
        tuya_ring_buff_free(ai_output_ctx.ringbuf);
        ai_output_ctx.ringbuf = NULL;
    }
    memset(&ai_output_ctx, 0, SIZEOF(ai_output_ctx));
    PR_DEBUG("ai output deinit");
}

STATIC void __ai_output_thread(void* arg)
{
    OPERATE_RET rt = OPRT_OK;
    AI_OUTPUT_STATUS_E cur_state = AI_OUTPUT_STOPPED;
    AI_OUTPUT_STATUS_E last_state = AI_OUTPUT_STOPPED;
    ai_output_ctx.offset = 0;
    uint32_t last_offset = 0;
    TIME_T time_now = 0, time_last = 0;

    while (!ai_output_ctx.terminate && tal_thread_get_state(ai_output_ctx.thread) == THREAD_STATE_RUNNING) {
        cur_state = ai_output_ctx.status;
        switch (cur_state) {
        case AI_OUTPUT_PLAYING:
        case AI_OUTPUT_STOPPING: {
            tal_mutex_lock(ai_output_ctx.mutex);
            rt = tuya_ring_buff_read(ai_output_ctx.ringbuf, ai_output_ctx.output_buf + ai_output_ctx.offset, AI_OUTPUT_BUF_SIZE - ai_output_ctx.offset);
            if ((AI_OUTPUT_PLAYING == cur_state) && (0 == rt)) {
                if (0 == ai_output_ctx.offset) {
                    tal_mutex_unlock(ai_output_ctx.mutex);
                    tal_system_sleep(10);
                    continue;
                } else {
                    if (last_offset == ai_output_ctx.offset) {
                        time_now = tal_time_get_posix();
                        if (time_now - time_last > 10) { // 10s no data
                            PR_DEBUG("[output] play timeout");
                            ai_output_ctx.status = AI_OUTPUT_STOPPED;
                            tuya_ring_buff_reset(ai_output_ctx.ringbuf);
                            last_offset = 0;
                            tal_mutex_unlock(ai_output_ctx.mutex);
                            break;
                        }
                    } else {
                        last_offset = ai_output_ctx.offset;
                        time_last = tal_time_get_posix();
                    }
                }
            } else {
                last_offset = 0;
            }
            ai_output_ctx.offset += rt;
            rt = tuya_ai_output_media(NULL, AI_PT_AUDIO, ai_output_ctx.output_buf, ai_output_ctx.offset, ai_output_ctx.offset);
            if (rt > 0) { // buf is not completely consumed
                if ((AI_OUTPUT_STOPPING == cur_state) && (rt == ai_output_ctx.offset)) {
                    ai_output_ctx.status = AI_OUTPUT_STOPPED;
                    PR_DEBUG("[output] stop force done, rt:%d", rt);
                    tal_mutex_unlock(ai_output_ctx.mutex);
                    break;
                }
                memmove(ai_output_ctx.output_buf, ai_output_ctx.output_buf + (ai_output_ctx.offset - rt), rt);
                ai_output_ctx.offset = rt;
            } else if (rt == 0) {
                ai_output_ctx.offset = 0;
                if ((cur_state == AI_OUTPUT_STOPPING) && (tuya_ring_buff_used_size_get(ai_output_ctx.ringbuf) == 0)) {
                    PR_DEBUG("[output] stop done");
                    ai_output_ctx.status = AI_OUTPUT_STOPPED;
                }
            } else {
                PR_DEBUG("[output] stop error");
                ai_output_ctx.status = AI_OUTPUT_STOPPED;
                tuya_ring_buff_reset(ai_output_ctx.ringbuf);
            }
            tal_mutex_unlock(ai_output_ctx.mutex);
        }
        break;
        case AI_OUTPUT_STOPPED: {
            ai_output_ctx.offset = 0;
            if (last_state != AI_OUTPUT_STOPPED) {
                tal_system_sleep(300); // wait for audio output done

                tal_mutex_lock(ai_output_ctx.mutex);
                tuya_ai_output_event(AI_EVENT_END, AI_PT_AUDIO, tuya_ai_agent_get_eid());
                tal_mutex_unlock(ai_output_ctx.mutex);
            }
        }
        break;
        case AI_OUTPUT_IDLE: {
            tal_system_sleep(AI_OUTPUT_TASK_DELAY);
        }
        break;
        default:
            break;
        }

        last_state = cur_state;

        if (cur_state == AI_OUTPUT_STOPPED) {
            tal_system_sleep(AI_OUTPUT_TASK_DELAY);
        } else {
            tal_system_sleep(1);
        }
    }
    __ai_output_free();
}
#endif

OPERATE_RET tuya_ai_output_init(AI_OUTPUT_CBS_T *cbs)
{
    OPERATE_RET rt = OPRT_OK;
    if (ai_output_ctx.mutex) {
        return OPRT_OK;
    }
    memset(&ai_output_ctx, 0, SIZEOF(ai_output_ctx));
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&ai_output_ctx.mutex));
    memcpy(&ai_output_ctx.cbs, cbs, SIZEOF(ai_output_ctx.cbs));
    INIT_LIST_HEAD(&ai_output_ctx.ext_cbs);

#if (AI_OUTPUT_RINGBUF_SIZE > 0)
    ai_output_ctx.output_buf = Malloc(AI_OUTPUT_BUF_SIZE);
    TUYA_CHECK_NULL_GOTO(ai_output_ctx.output_buf, EXIT);
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(AI_OUTPUT_RINGBUF_SIZE, OVERFLOW_PSRAM_STOP_TYPE, &ai_output_ctx.ringbuf), EXIT);
#else
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(AI_OUTPUT_RINGBUF_SIZE, OVERFLOW_STOP_TYPE, &ai_output_ctx.ringbuf), EXIT);
#endif

    THREAD_CFG_T thrd_param = {0};
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "ai_agent_output";
    thrd_param.stackDepth = AI_OUTPUT_STACK_SIZE;
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    thrd_param.psram_mode = 1;
#endif
    rt = tal_thread_create_and_start(&ai_output_ctx.thread, NULL, NULL, __ai_output_thread, NULL, &thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("ai output thread create err, rt:%d", rt);
        goto EXIT;
    }
    PR_DEBUG("ai output init success");
    return rt;

EXIT:
    __ai_output_free();
#else
    PR_DEBUG("ai output init success");
#endif
    return rt;
}

OPERATE_RET tuya_ai_output_event(AI_EVENT_TYPE etype, AI_PACKET_PT ptype, AI_EVENT_ID eid)
{
    OPERATE_RET rt = OPRT_OK;
    LIST_HEAD *pos;

    tuya_list_for_each(pos, &ai_output_ctx.ext_cbs) {
        AI_OUTPUT_CBS_NODE_T *entry = tuya_list_entry(pos, AI_OUTPUT_CBS_NODE_T, node);
        if (entry->cbs.event_cb) {
            rt = entry->cbs.event_cb(etype, ptype, eid);
            if (entry->mode == AI_OUTPUT_CBS_MODE_FILTER) {
                return rt;
            }
        }
    }
    if (ai_output_ctx.cbs.event_cb) {
        rt = ai_output_ctx.cbs.event_cb(etype, ptype, eid);
    }
    return rt;
}

OPERATE_RET tuya_ai_output_start(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("[output] start. status:%d", ai_output_ctx.status);

    if ((ai_output_ctx.status != AI_OUTPUT_STOPPED) && (ai_output_ctx.status != AI_OUTPUT_IDLE)) {
        tuya_ai_output_stop(TRUE);
    }

    tal_mutex_lock(ai_output_ctx.mutex);
    ai_output_ctx.status = AI_OUTPUT_PLAYING;

#if (AI_OUTPUT_RINGBUF_SIZE > 0)
    ai_output_ctx.offset = 0;
    tuya_ring_buff_reset(ai_output_ctx.ringbuf);
#endif

    tuya_ai_output_event(AI_EVENT_START, AI_PT_AUDIO, tuya_ai_agent_get_eid());
    tal_mutex_unlock(ai_output_ctx.mutex);

    return rt;
}

OPERATE_RET tuya_ai_output_attr(char *scode, AI_BIZ_ATTR_INFO_T *attr)
{
    LIST_HEAD *pos;

    if (scode) {
        tuya_list_for_each(pos, &ai_output_ctx.ext_cbs) {
            AI_OUTPUT_CBS_NODE_T *entry = tuya_list_entry(pos, AI_OUTPUT_CBS_NODE_T, node);
            if (entry->cbs.media_attr_cb && strcmp(entry->scode, scode) == 0) {
                entry->cbs.media_attr_cb(attr);
                if (entry->mode == AI_OUTPUT_CBS_MODE_FILTER) {
                    return OPRT_OK;
                }
            }
        }
    }

    if (ai_output_ctx.cbs.media_attr_cb) {
        ai_output_ctx.cbs.media_attr_cb(attr);
    }
    return OPRT_OK;
}

OPERATE_RET tuya_ai_output_alert(AI_ALERT_TYPE_E type)
{
    LIST_HEAD *pos;
    tuya_list_for_each(pos, &ai_output_ctx.ext_cbs) {
        AI_OUTPUT_CBS_NODE_T *entry = tuya_list_entry(pos, AI_OUTPUT_CBS_NODE_T, node);
        if (entry->cbs.alert_cb) {
            entry->cbs.alert_cb(type);
            if (entry->mode == AI_OUTPUT_CBS_MODE_FILTER) {
                return OPRT_OK;
            }
        }
    }

    if (ai_output_ctx.cbs.alert_cb) {
        return ai_output_ctx.cbs.alert_cb(type);
    }
    return OPRT_OK;
}

OPERATE_RET tuya_ai_output_text(char *scode, AI_TEXT_TYPE_E type, cJSON *root, bool eof)
{
    LIST_HEAD *pos;

    if (scode) {
        tuya_list_for_each(pos, &ai_output_ctx.ext_cbs) {
            AI_OUTPUT_CBS_NODE_T *entry = tuya_list_entry(pos, AI_OUTPUT_CBS_NODE_T, node);
            if (entry->cbs.text_cb && strcmp(entry->scode, scode) == 0) {
                entry->cbs.text_cb(type, root, eof);
                if (entry->mode == AI_OUTPUT_CBS_MODE_FILTER) {
                    return OPRT_OK;
                }
            }
        }
    }

    if (ai_output_ctx.cbs.text_cb) {
        return ai_output_ctx.cbs.text_cb(type, root, eof);
    }
    return OPRT_OK;
}

OPERATE_RET tuya_ai_output_media(char *scode, AI_PACKET_PT type, char *data, uint32_t len, uint32_t total_len)
{
    LIST_HEAD *pos;

    if (scode) {
        tuya_list_for_each(pos, &ai_output_ctx.ext_cbs) {
            AI_OUTPUT_CBS_NODE_T *entry = tuya_list_entry(pos, AI_OUTPUT_CBS_NODE_T, node);
            if (entry->cbs.media_data_cb && strcmp(entry->scode, scode) == 0) {
                entry->cbs.media_data_cb(type, data, len, total_len);
                if (entry->mode == AI_OUTPUT_CBS_MODE_FILTER) {
                    return OPRT_OK;
                }
            }
        }
    }

    if (ai_output_ctx.cbs.media_data_cb) {
        return ai_output_ctx.cbs.media_data_cb(type, data, len, total_len);
    }
    return OPRT_OK;
}

OPERATE_RET tuya_ai_output_stop(bool force)
{
    OPERATE_RET rt = OPRT_OK;
    PR_DEBUG("[output] stop. force:%d status:%d", force, ai_output_ctx.status);

    if ((ai_output_ctx.status == AI_OUTPUT_STOPPED) || (ai_output_ctx.status == AI_OUTPUT_IDLE)) {
        return rt;
    }

    tal_mutex_lock(ai_output_ctx.mutex);
#if (AI_OUTPUT_RINGBUF_SIZE > 0)
    if (force) {
        ai_output_ctx.status = AI_OUTPUT_IDLE;
        ai_output_ctx.offset = 0;
        tuya_ring_buff_reset(ai_output_ctx.ringbuf);
        rt = tuya_ai_output_event(AI_EVENT_END, AI_PT_AUDIO, tuya_ai_agent_get_eid());
    } else {
        ai_output_ctx.status = AI_OUTPUT_STOPPING;
    }
#else
    ai_output_ctx.status = AI_OUTPUT_STOPPED;
    tuya_ai_output_event(AI_EVENT_END, AI_PT_AUDIO, tuya_ai_agent_get_eid());
#endif
    tal_mutex_unlock(ai_output_ctx.mutex);
    return rt;
}

OPERATE_RET tuya_ai_output_write(AI_PACKET_PT type, uint8_t *data, uint32_t len)
{
    if (ai_output_ctx.status != AI_OUTPUT_PLAYING) {
        return OPRT_OK;
    }

    OPERATE_RET rt = OPRT_OK;
#if (AI_OUTPUT_RINGBUF_SIZE > 0)
    int cnt = 0;
    tal_mutex_lock(ai_output_ctx.mutex);
    rt = tuya_ring_buff_write(ai_output_ctx.ringbuf, data, len);
    tal_mutex_unlock(ai_output_ctx.mutex);

    while (rt != len) {
        tal_system_sleep(10);

        data += rt;
        len -= rt;
        rt = 0;

        tal_mutex_lock(ai_output_ctx.mutex);
        rt = tuya_ring_buff_write(ai_output_ctx.ringbuf, data, len);
        tal_mutex_unlock(ai_output_ctx.mutex);

        if (cnt ++ > 500) {
            PR_ERR("output ring buf write failed");
            break;
        }
    }
    return OPRT_OK;
#else
    rt = tuya_ai_output_media(NULL, type, (char *)data, len, len);
    return rt;
#endif
}

bool tuya_ai_output_is_playing(void)
{
    return (ai_output_ctx.status == AI_OUTPUT_PLAYING) || (ai_output_ctx.status == AI_OUTPUT_STOPPING);
}

/* register external cbs with scode */
OPERATE_RET tuya_ai_output_register_cbs(char *scode, AI_OUTPUT_CBS_T *cbs, AI_OUTPUT_CBS_MODE_E mode)
{
    OPERATE_RET rt = OPRT_OK;
    AI_OUTPUT_CBS_NODE_T *node = NULL, *new_node = NULL;
    LIST_HEAD *pos = NULL;

    if (!scode || !cbs || (mode >= AI_OUTPUT_CBS_MODE_MAX)) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ai_output_ctx.mutex);
    tuya_list_for_each(pos, &ai_output_ctx.ext_cbs) {
        node = tuya_list_entry(pos, AI_OUTPUT_CBS_NODE_T, node);
        if (strcmp(node->scode, scode) == 0) {
            PR_ERR("output cbs already registered: %s", scode);
            rt = OPRT_COM_ERROR;
            goto error;
        }
    }
    NEW_LIST_NODE(AI_OUTPUT_CBS_NODE_T, new_node);
    TUYA_CHECK_NULL_GOTO(new_node, error);
    new_node->scode = mm_strdup(scode);
    TUYA_CHECK_NULL_GOTO(new_node->scode, error);
    memcpy(&new_node->cbs, cbs, SIZEOF(AI_OUTPUT_CBS_T));
    new_node->mode = mode;
    tuya_list_add_tail(&new_node->node, &ai_output_ctx.ext_cbs);
    tal_mutex_unlock(ai_output_ctx.mutex);
    return OPRT_OK;
error:
    if (new_node) {
        if (new_node->scode) {
            tal_free(new_node->scode);
        }
        tal_free(new_node);
    }
    tal_mutex_unlock(ai_output_ctx.mutex);
    return rt;
}

/* unregister external cbs with scode */
OPERATE_RET tuya_ai_output_unregister_cbs(char *scode)
{
    AI_OUTPUT_CBS_NODE_T *node = NULL;
    LIST_HEAD *pos = NULL, *n = NULL;

    if (!scode) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ai_output_ctx.mutex);
    tuya_list_for_each_safe(pos, n, &ai_output_ctx.ext_cbs) {
        node = tuya_list_entry(pos, AI_OUTPUT_CBS_NODE_T, node);
        if (strcmp(node->scode, scode) == 0) {
            tuya_list_del(&node->node);
            if (node->scode) {
                tal_free(node->scode);
            }
            tal_free(node);
            tal_mutex_unlock(ai_output_ctx.mutex);
            return OPRT_OK;
        }
    }
    tal_mutex_unlock(ai_output_ctx.mutex);
    PR_ERR("output cbs not found: %s", scode);
    return OPRT_COM_ERROR;
}
