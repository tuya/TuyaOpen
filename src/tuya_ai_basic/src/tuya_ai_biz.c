/**
 * @file tuya_ai_biz.c
 * @brief This file contains the implementation of Tuya AI business logic,
 * including AI session management, task scheduling and AI protocol processing.
 *
 * The Tuya AI business module provides core functionalities for AI session
 * lifecycle management, including session creation, configuration and resource
 * allocation. It implements the AI protocol handlers and manages concurrent
 * AI sessions with thread-safe operations.
 *
 * Key features include:
 * - AI session management with configurable maximum session limit
 * - Asynchronous task scheduling with configurable delay
 * - Thread-safe operations using mutex and event mechanisms
 * - Integration with Tuya AI client and protocol layers
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include <stdint.h>
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
#include "tuya_ai_biz.h"
#include "tal_event.h"
#include "tuya_ai_private.h"

#ifndef AI_SESSION_MAX_NUM
#define AI_SESSION_MAX_NUM 6
#endif
#ifndef AI_BIZ_TASK_DELAY
#define AI_BIZ_TASK_DELAY 10
#endif

typedef struct {
    char id[AI_UUID_V4_LEN];
    AI_SESSION_CFG_T cfg;
} AI_SESSION_T;

typedef struct {
    THREAD_HANDLE thread;
    MUTEX_HANDLE mutex;
    AI_SESSION_T session[AI_SESSION_MAX_NUM];
    AI_BIZ_RECV_CB cb;
} AI_BASIC_BIZ_T;
AI_BASIC_BIZ_T *ai_basic_biz;

OPERATE_RET tuya_ai_send_biz_pkt(uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_PACKET_PT type, AI_BIZ_HEAD_INFO_T *head,
                                 char *payload)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t payload_len = 0;
    if (ai_basic_biz == NULL) {
        PR_ERR("ai biz is null");
        return OPRT_COM_ERROR;
    }
    AI_PROTO_D("biz len:%d", head->len);
    if (type == AI_PT_VIDEO) {
        payload_len = sizeof(AI_VIDEO_HEAD_T) + head->len;
        char *video = Malloc(payload_len);
        TUYA_CHECK_NULL_RETURN(video, OPRT_MALLOC_FAILED);
        memset(video, 0, payload_len);
        AI_VIDEO_HEAD_T *video_head = (AI_VIDEO_HEAD_T *)video;
        video_head->id = UNI_HTONS(id);
        video_head->stream_flag = head->stream_flag;
        video_head->timestamp = head->value.video.timestamp;
        video_head->pts = head->value.video.pts;
        UNI_HTONLL(video_head->timestamp);
        UNI_HTONLL(video_head->pts);
        video_head->length = UNI_HTONL(head->len);
        if (payload && head->len) {
            memcpy(video + sizeof(AI_VIDEO_HEAD_T), payload, head->len);
        }
        if (attr && (attr->flag == AI_HAS_ATTR)) {
            rt = tuya_ai_basic_video(&(attr->value.video), video, payload_len);
        } else {
            rt = tuya_ai_basic_video(NULL, video, payload_len);
        }
        Free(video);
    } else if (type == AI_PT_AUDIO) {
        payload_len = sizeof(AI_AUDIO_HEAD_T) + head->len;
        char *audio = Malloc(payload_len);
        TUYA_CHECK_NULL_RETURN(audio, OPRT_MALLOC_FAILED);
        memset(audio, 0, payload_len);
        AI_AUDIO_HEAD_T *audio_head = (AI_AUDIO_HEAD_T *)audio;
        audio_head->id = UNI_HTONS(id);
        audio_head->stream_flag = head->stream_flag;
        audio_head->timestamp = head->value.audio.timestamp;
        audio_head->pts = head->value.audio.pts;
        UNI_HTONLL(audio_head->timestamp);
        UNI_HTONLL(audio_head->pts);
        audio_head->length = UNI_HTONL(head->len);
        if (payload && head->len) {
            memcpy(audio + sizeof(AI_AUDIO_HEAD_T), payload, head->len);
        }
        if (attr && (attr->flag == AI_HAS_ATTR)) {
            rt = tuya_ai_basic_audio(&(attr->value.audio), audio, payload_len);
        } else {
            rt = tuya_ai_basic_audio(NULL, audio, payload_len);
        }
        Free(audio);
    } else if (type == AI_PT_IMAGE) {
        payload_len = sizeof(AI_IMAGE_HEAD_T) + head->len;
        char *image = Malloc(payload_len);
        TUYA_CHECK_NULL_RETURN(image, OPRT_MALLOC_FAILED);
        memset(image, 0, payload_len);
        AI_IMAGE_HEAD_T *image_head = (AI_IMAGE_HEAD_T *)image;
        image_head->id = UNI_HTONS(id);
        image_head->stream_flag = head->stream_flag;
        image_head->timestamp = head->value.image.timestamp;
        UNI_HTONLL(image_head->timestamp);
        image_head->length = UNI_HTONL(head->len);
        if (payload && head->len) {
            memcpy(image + sizeof(AI_IMAGE_HEAD_T), payload, head->len);
        }
        rt = tuya_ai_basic_image(&(attr->value.image), image, payload_len);
        Free(image);
    } else if (type == AI_PT_FILE) {
        payload_len = sizeof(AI_FILE_HEAD_T) + head->len;
        char *file = Malloc(payload_len);
        TUYA_CHECK_NULL_RETURN(file, OPRT_MALLOC_FAILED);
        memset(file, 0, payload_len);
        AI_FILE_HEAD_T *file_head = (AI_FILE_HEAD_T *)file;
        file_head->id = UNI_HTONS(id);
        file_head->stream_flag = head->stream_flag;
        file_head->length = UNI_HTONL(head->len);
        if (payload && head->len) {
            memcpy(file + sizeof(AI_FILE_HEAD_T), payload, head->len);
        }
        rt = tuya_ai_basic_file(&(attr->value.file), file, payload_len);
        Free(file);
    } else if (type == AI_PT_TEXT) {
        payload_len = sizeof(AI_TEXT_HEAD_T) + head->len;
        char *text = Malloc(payload_len);
        TUYA_CHECK_NULL_RETURN(text, OPRT_MALLOC_FAILED);
        memset(text, 0, payload_len);
        AI_TEXT_HEAD_T *text_head = (AI_TEXT_HEAD_T *)text;
        text_head->id = UNI_HTONS(id);
        text_head->stream_flag = head->stream_flag;
        text_head->length = UNI_HTONL(head->len);
        if (payload && head->len) {
            memcpy(text + sizeof(AI_TEXT_HEAD_T), payload, head->len);
        }
        if (attr && (attr->flag == AI_HAS_ATTR)) {
            rt = tuya_ai_basic_text(&(attr->value.text), text, payload_len);
        } else {
            rt = tuya_ai_basic_text(NULL, text, payload_len);
        }
        Free(text);
    } else {
        PR_ERR("unknow type:%d", type);
        rt = OPRT_COM_ERROR;
    }

    if (rt != OPRT_OK) {
        PR_ERR("send biz data failed, rt:%d", rt);
    }
    return rt;
}

static void __ai_biz_thread_cb(void *args)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t idx = 0, sidx = 0, kdx = 0;
    while (tal_thread_get_state(ai_basic_biz->thread) == THREAD_STATE_RUNNING) {
        if (!tuya_ai_client_is_ready()) {
            tal_system_sleep(200);
            continue;
        }
        tal_mutex_lock(ai_basic_biz->mutex);
        uint16_t sent_ids[AI_MAX_SESSION_ID_NUM * AI_SESSION_MAX_NUM] = {0};
        uint32_t sent_ids_count = 0;
        for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
            if (ai_basic_biz->session[idx].id[0] != 0) {
                AI_SESSION_T *session = &ai_basic_biz->session[idx];
                for (sidx = 0; sidx < session->cfg.send_num; sidx++) {
                    uint16_t send_id = session->cfg.send[sidx].id;
                    uint8_t already_sent = false;
                    for (kdx = 0; kdx < sent_ids_count; kdx++) {
                        if (sent_ids[kdx] == send_id) {
                            already_sent = true;
                            break;
                        }
                    }
                    if (!already_sent) {
                        sent_ids[sent_ids_count++] = send_id;
                        AI_BIZ_SEND_DATA_T *send = &session->cfg.send[sidx];
                        if (send->get_cb) {
                            AI_BIZ_ATTR_INFO_T attr = {0};
                            AI_BIZ_HEAD_INFO_T head = {0};
                            char *payload = NULL;
                            rt = send->get_cb(&attr, &head, &payload);
                            if (rt != OPRT_OK) {
                                continue;
                            }
                            tuya_ai_send_biz_pkt(send->id, &attr, send->type, &head, payload);
                            if (send->free_cb) {
                                send->free_cb(payload);
                            }
                        }
                    }
                }
            }
        }
        tal_mutex_unlock(ai_basic_biz->mutex);
        tal_system_sleep(AI_BIZ_TASK_DELAY);
    }

    PR_NOTICE("ai biz thread exit");
    return;
}

static uint8_t __ai_biz_need_send_task(void)
{
    uint32_t idx = 0, sidx = 0;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_basic_biz->session[idx].id[0] != 0) {
            AI_SESSION_T *session = &ai_basic_biz->session[idx];
            for (sidx = 0; sidx < session->cfg.send_num; sidx++) {
                if (session->cfg.send[sidx].get_cb) {
                    return true;
                }
            }
        }
    }
    return false;
}

static OPERATE_RET __ai_biz_create_task(void)
{
    OPERATE_RET rt = OPRT_OK;
    if (ai_basic_biz->thread) {
        return rt;
    }
    THREAD_CFG_T thrd_param = {0};
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "ai_biz_thread";
    thrd_param.stackDepth = 4096;
#if defined(AI_STACK_IN_PSRAM) && (AI_STACK_IN_PSRAM == 1)
    thrd_param.psram_mode = 1;
#endif

    rt = tal_thread_create_and_start(&ai_basic_biz->thread, NULL, NULL, __ai_biz_thread_cb, NULL, &thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("ai biz thread create err, rt:%d", rt);
    }
    AI_PROTO_D("create ai biz thread success");
    return rt;
}

static void __ai_biz_deinit(void)
{
    if (ai_basic_biz) {
        if (ai_basic_biz->thread) {
            tal_thread_delete(ai_basic_biz->thread);
            ai_basic_biz->thread = NULL;
        }
        if (ai_basic_biz->mutex) {
            tal_mutex_release(ai_basic_biz->mutex);
            ai_basic_biz->mutex = NULL;
        }
        Free(ai_basic_biz);
        ai_basic_biz = NULL;
    }
}

OPERATE_RET __ai_parse_video_attr(char *de_buf, uint32_t attr_len, AI_VIDEO_ATTR_T *video)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    AI_ATTRIBUTE_T attr = {0};

    while (offset < attr_len) {
        memset(&attr, 0, sizeof(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(de_buf, &offset, &attr);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }

        if (attr.type == AI_ATTR_VIDEO_CODEC_TYPE) {
            video->base.codec_type = attr.value.u16;
        } else if (attr.type == AI_ATTR_VIDEO_SAMPLE_RATE) {
            video->base.sample_rate = attr.value.u32;
        } else if (attr.type == AI_ATTR_VIDEO_WIDTH) {
            video->base.width = attr.value.u16;
        } else if (attr.type == AI_ATTR_VIDEO_HEIGHT) {
            video->base.height = attr.value.u16;
        } else if (attr.type == AI_ATTR_VIDEO_FPS) {
            video->base.fps = attr.value.u16;
        } else if (attr.type == AI_ATTR_USER_DATA) {
            video->option.user_data = attr.value.bytes;
            video->option.user_len = attr.length;
        } else if (attr.type == AI_ATTR_SESSION_ID_LIST) {
            video->option.session_id_list = attr.value.str;
        } else {
            PR_ERR("unknow attr type:%d", attr.type);
        }
    }
    return rt;
}

OPERATE_RET __ai_parse_audio_attr(char *de_buf, uint32_t attr_len, AI_AUDIO_ATTR_T *audio)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    AI_ATTRIBUTE_T attr = {0};

    while (offset < attr_len) {
        memset(&attr, 0, sizeof(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(de_buf, &offset, &attr);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }

        if (attr.type == AI_ATTR_AUDIO_CODEC_TYPE) {
            audio->base.codec_type = attr.value.u16;
        } else if (attr.type == AI_ATTR_AUDIO_SAMPLE_RATE) {
            audio->base.sample_rate = attr.value.u32;
        } else if (attr.type == AI_ATTR_AUDIO_CHANNELS) {
            audio->base.channels = attr.value.u16;
        } else if (attr.type == AI_ATTR_AUDIO_DEPTH) {
            audio->base.bit_depth = attr.value.u16;
        } else if (attr.type == AI_ATTR_USER_DATA) {
            audio->option.user_data = attr.value.bytes;
            audio->option.user_len = attr.length;
        } else if (attr.type == AI_ATTR_SESSION_ID_LIST) {
            audio->option.session_id_list = attr.value.str;
        } else {
            PR_ERR("unknow attr type:%d", attr.type);
        }
    }
    return rt;
}

OPERATE_RET __ai_parse_image_attr(char *de_buf, uint32_t attr_len, AI_IMAGE_ATTR_T *image)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    AI_ATTRIBUTE_T attr = {0};

    while (offset < attr_len) {
        memset(&attr, 0, sizeof(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(de_buf, &offset, &attr);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }

        if (attr.type == AI_ATTR_IMAGE_FORMAT) {
            image->base.format = attr.value.u8;
        } else if (attr.type == AI_ATTR_IMAGE_WIDTH) {
            image->base.width = attr.value.u16;
        } else if (attr.type == AI_ATTR_IMAGE_HEIGHT) {
            image->base.height = attr.value.u16;
        } else if (attr.type == AI_ATTR_USER_DATA) {
            image->option.user_data = attr.value.bytes;
            image->option.user_len = attr.length;
        } else if (attr.type == AI_ATTR_SESSION_ID_LIST) {
            image->option.session_id_list = attr.value.str;
        } else {
            PR_ERR("unknow attr type:%d", attr.type);
        }
    }
    return rt;
}

OPERATE_RET __ai_parse_file_attr(char *de_buf, uint32_t attr_len, AI_FILE_ATTR_T *file)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    AI_ATTRIBUTE_T attr = {0};

    while (offset < attr_len) {
        memset(&attr, 0, sizeof(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(de_buf, &offset, &attr);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }

        if (attr.type == AI_ATTR_FILE_FORMAT) {
            file->base.format = attr.value.u8;
        } else if (attr.type == AI_ATTR_FILE_NAME) {
            if (attr.length > sizeof(file->base.file_name)) {
                PR_ERR("file name too long %d", attr.length);
                return OPRT_INVALID_PARM;
            }
            memcpy(file->base.file_name, attr.value.str, attr.length);
        } else if (attr.type == AI_ATTR_USER_DATA) {
            file->option.user_data = attr.value.bytes;
            file->option.user_len = attr.length;
        } else if (attr.type == AI_ATTR_SESSION_ID_LIST) {
            file->option.session_id_list = attr.value.str;
        } else {
            PR_ERR("unknow attr type:%d", attr.type);
        }
    }
    if (file->base.file_name == NULL) {
        PR_ERR("file name is null");
        return OPRT_INVALID_PARM;
    }
    return rt;
}

OPERATE_RET __ai_parse_text_attr(char *de_buf, uint32_t attr_len, AI_TEXT_ATTR_T *text)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    AI_ATTRIBUTE_T attr = {0};

    while (offset < attr_len) {
        memset(&attr, 0, sizeof(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(de_buf, &offset, &attr);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }

        if (attr.type == AI_ATTR_SESSION_ID_LIST) {
            text->session_id_list = attr.value.str;
        } else {
            PR_ERR("unknow attr type:%d", attr.type);
        }
    }
    return rt;
}

OPERATE_RET __ai_parse_event_attr(char *de_buf, uint32_t attr_len, AI_EVENT_ATTR_T *event)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    AI_ATTRIBUTE_T attr = {0};

    while (offset < attr_len) {
        memset(&attr, 0, sizeof(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(de_buf, &offset, &attr);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }

        if (attr.type == AI_ATTR_SESSION_ID) {
            event->session_id = attr.value.str;
            AI_PROTO_D("recv event session id:%s", event->session_id);
        } else if (attr.type == AI_ATTR_EVENT_ID) {
            event->event_id = attr.value.str;
            AI_PROTO_D("recv event id:%s", event->event_id);
        } else if (attr.type == AI_ATTR_USER_DATA) {
            event->user_data = attr.value.bytes;
            event->user_len = attr.length;
        } else if (attr.type == AI_ATTR_EVENT_TS) {
            event->end_ts = attr.value.u64;
        } else {
            PR_ERR("unknow attr type:%d", attr.type);
        }
    }
    if ((NULL == event->event_id) || (NULL == event->session_id)) {
        PR_ERR("event id or session id is null");
        return OPRT_INVALID_PARM;
    }

    return rt;
}

OPERATE_RET __ai_parse_session_close_attr(char *de_buf, uint32_t attr_len, AI_SESSION_CLOSE_ATTR_T *close)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    AI_ATTRIBUTE_T attr = {0};

    while (offset < attr_len) {
        memset(&attr, 0, sizeof(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(de_buf, &offset, &attr);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }

        if (attr.type == AI_ATTR_SESSION_ID) {
            close->id = attr.value.str;
            PR_NOTICE("close session id:%s", close->id);
        } else if (attr.type == AI_ATTR_SESSION_CLOSE_ERR_CODE) {
            close->code = attr.value.u16;
            PR_NOTICE("close seesion err code:%d", close->code);
        } else {
            PR_ERR("unknow attr type:%d", attr.type);
        }
    }
    if (NULL == close->id) {
        PR_ERR("clsoe session id is null");
        return OPRT_INVALID_PARM;
    }

    return rt;
}

static OPERATE_RET __ai_parse_biz_attr(AI_PACKET_PT type, char *attr_buf, uint32_t attr_len, AI_BIZ_ATTR_INFO_T *attr)
{
    OPERATE_RET rt = OPRT_OK;
    if (type == AI_PT_VIDEO) {
        rt = __ai_parse_video_attr(attr_buf, attr_len, &attr->value.video);
        if (OPRT_OK != rt) {
            PR_ERR("parse video attr failed, rt:%d", rt);
            return rt;
        }
    } else if (type == AI_PT_AUDIO) {
        rt = __ai_parse_audio_attr(attr_buf, attr_len, &attr->value.audio);
        if (OPRT_OK != rt) {
            PR_ERR("parse audio attr failed, rt:%d", rt);
            return rt;
        }
    } else if (type == AI_PT_IMAGE) {
        rt = __ai_parse_image_attr(attr_buf, attr_len, &attr->value.image);
        if (OPRT_OK != rt) {
            PR_ERR("parse image attr failed, rt:%d", rt);
            return rt;
        }
    } else if (type == AI_PT_FILE) {
        rt = __ai_parse_file_attr(attr_buf, attr_len, &attr->value.file);
        if (OPRT_OK != rt) {
            PR_ERR("parse file attr failed, rt:%d", rt);
            return rt;
        }
    } else if (type == AI_PT_TEXT) {
        rt = __ai_parse_text_attr(attr_buf, attr_len, &attr->value.text);
        if (OPRT_OK != rt) {
            PR_ERR("parse text attr failed, rt:%d", rt);
            return rt;
        }
    } else if (type == AI_PT_EVENT) {
        rt = __ai_parse_event_attr(attr_buf, attr_len, &attr->value.event);
        if (OPRT_OK != rt) {
            PR_ERR("parse event attr failed, rt:%d", rt);
            return rt;
        }
    } else if (type == AI_PT_SESSION_CLOSE) {
        rt = __ai_parse_session_close_attr(attr_buf, attr_len, &attr->value.close);
        if (OPRT_OK != rt) {
            PR_ERR("parse event attr failed, rt:%d", rt);
            return rt;
        }
    } else {
        PR_ERR("unknow type:%d", type);
        return OPRT_INVALID_PARM;
    }
    return rt;
}

static OPERATE_RET __ai_parse_biz_head(AI_PACKET_PT type, char *payload, AI_BIZ_HEAD_INFO_T *biz_head, uint32_t *offset)
{
    if (type == AI_PT_VIDEO) {
        AI_VIDEO_HEAD_T *video_head = (AI_VIDEO_HEAD_T *)payload;
        biz_head->stream_flag = video_head->stream_flag;
        biz_head->value.video.timestamp = video_head->timestamp;
        UNI_NTOHLL(biz_head->value.video.timestamp);
        biz_head->value.video.pts = video_head->pts;
        UNI_NTOHLL(biz_head->value.video.pts);
        biz_head->len = UNI_NTOHL(video_head->length);
        *offset = sizeof(AI_VIDEO_HEAD_T);
    } else if (type == AI_PT_AUDIO) {
        AI_AUDIO_HEAD_T *audio_head = (AI_AUDIO_HEAD_T *)payload;
        biz_head->stream_flag = audio_head->stream_flag;
        biz_head->value.audio.timestamp = audio_head->timestamp;
        UNI_NTOHLL(biz_head->value.audio.timestamp);
        biz_head->value.audio.pts = audio_head->pts;
        UNI_NTOHLL(biz_head->value.audio.pts);
        biz_head->len = UNI_NTOHL(audio_head->length);
        *offset = sizeof(AI_AUDIO_HEAD_T);
    } else if (type == AI_PT_IMAGE) {
        AI_IMAGE_HEAD_T *image_head = (AI_IMAGE_HEAD_T *)payload;
        biz_head->stream_flag = image_head->stream_flag;
        biz_head->value.image.timestamp = image_head->timestamp;
        UNI_NTOHLL(biz_head->value.image.timestamp);
        biz_head->len = UNI_NTOHL(image_head->length);
        *offset = sizeof(AI_IMAGE_HEAD_T);
    } else if (type == AI_PT_FILE) {
        AI_FILE_HEAD_T *file_head = (AI_FILE_HEAD_T *)payload;
        biz_head->stream_flag = file_head->stream_flag;
        biz_head->len = UNI_NTOHL(file_head->length);
        *offset = sizeof(AI_FILE_HEAD_T);
    } else if (type == AI_PT_TEXT) {
        AI_TEXT_HEAD_T *text_head = (AI_TEXT_HEAD_T *)payload;
        biz_head->stream_flag = text_head->stream_flag;
        biz_head->len = UNI_NTOHL(text_head->length);
        *offset = sizeof(AI_TEXT_HEAD_T);
    } else {
        PR_ERR("unknow type:%d", type);
        return OPRT_INVALID_PARM;
    }
    // AI_PROTO_D("biz head len:%d, offset:%d", biz_head->len, *offset);
    return OPRT_OK;
}

static uint8_t __ai_is_biz_pkt_vaild(AI_PACKET_PT type)
{
    if ((type != AI_PT_AUDIO) && (type != AI_PT_VIDEO) && (type != AI_PT_IMAGE) && (type != AI_PT_FILE) &&
        (type != AI_PT_TEXT) && (type != AI_PT_EVENT) && (type != AI_PT_SESSION_CLOSE)) {
        PR_ERR("recv data type error %d", type);
        return false;
    }
    return true;
}

static OPERATE_RET __ai_biz_recv_event(AI_EVENT_ATTR_T *event, char *payload)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t idx = 0;
    AI_EVENT_HEAD_T *head = (AI_EVENT_HEAD_T *)payload;
    AI_EVENT_TYPE type = UNI_NTOHS(head->type);

    tal_mutex_lock(ai_basic_biz->mutex);
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if ((ai_basic_biz->session[idx].id[0] != 0) && (!strcmp(ai_basic_biz->session[idx].id, event->session_id))) {
            AI_EVENT_CB cb = ai_basic_biz->session[idx].cfg.event_cb;
            if (cb) {
                AI_PROTO_D("recv event type:%d, call cb: %p", type, cb);
                rt = cb(type, event->session_id, event->event_id, event->user_data, event->user_len);
                if (rt != OPRT_OK) {
                    PR_ERR("recv event handle failed, rt:%d", rt);
                }
                break;
            }
        }
    }
    tal_mutex_unlock(ai_basic_biz->mutex);

    if (idx == AI_SESSION_MAX_NUM) {
        PR_ERR("session not found");
        return OPRT_COM_ERROR;
    }
    return rt;
}

static OPERATE_RET __ai_biz_session_destory(AI_SESSION_ID id, AI_STATUS_CODE code, uint8_t sync_cloud)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t idx = 0;
    if ((id == NULL) || (ai_basic_biz == NULL)) {
        PR_ERR("del session id or biz is null");
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("del sessoion id:%s", id);
    tal_mutex_lock(ai_basic_biz->mutex);
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_basic_biz->session[idx].id[0] != 0 && !strcmp(ai_basic_biz->session[idx].id, id)) {
            memset(&ai_basic_biz->session[idx], 0, sizeof(AI_SESSION_T));
            AI_PROTO_D("del session idx:%d", idx);
            break;
        }
    }
    tal_mutex_unlock(ai_basic_biz->mutex);
    if (idx == AI_SESSION_MAX_NUM) {
        PR_ERR("session not found");
        return OPRT_COM_ERROR;
    }

    if (sync_cloud) {
        rt = tuya_ai_basic_session_close(id, code);
        if (OPRT_OK != rt) {
            PR_ERR("send session to cloud failed, rt:%d", rt);
        }
    } else {
        PR_NOTICE("publish event session close");
        tal_event_publish(EVENT_AI_SESSION_CLOSE, id);
        tal_event_publish(EVENT_AI_SESSION_NEW, NULL);
    }

    return rt;
}

OPERATE_RET __ai_biz_recv_handle(char *data, uint32_t len, AI_FRAG_FLAG frag)
{
    OPERATE_RET rt = OPRT_OK;
    char *payload = NULL, *attr_buf = NULL;
    void *usr_data = NULL;
    AI_BIZ_HEAD_INFO_T biz_head = {0};
    AI_BIZ_RECV_CB cb = NULL;
    AI_PROTO_D("recv data len:%d, frag:%d", len, frag);
    if ((frag == AI_PACKET_NO_FRAG) || (frag == AI_PACKET_FRAG_START)) {
        AI_PAYLOAD_HEAD_T *head = (AI_PAYLOAD_HEAD_T *)data;
        AI_PACKET_PT type = head->type;
        AI_ATTR_FLAG attr_flag = head->attribute_flag;
        uint32_t idx = 0, attr_len = 0;
        uint32_t offset = sizeof(AI_PAYLOAD_HEAD_T);
        ai_basic_biz->cb = NULL;

        if (!__ai_is_biz_pkt_vaild(type)) {
            return OPRT_INVALID_PARM;
        }

        AI_BIZ_ATTR_INFO_T attr_info;
        memset(&attr_info, 0, sizeof(AI_BIZ_ATTR_INFO_T));
        attr_info.flag = attr_flag;
        attr_info.type = type;
        if (attr_flag == AI_HAS_ATTR) {
            memcpy(&attr_len, data + offset, sizeof(attr_len));
            attr_len = UNI_NTOHL(attr_len);
            offset += sizeof(attr_len);
            attr_buf = data + offset;
            rt = __ai_parse_biz_attr(type, attr_buf, attr_len, &attr_info);
            if (OPRT_OK != rt) {
                return rt;
            }
            offset += attr_len;
        }

        if (type == AI_PT_SESSION_CLOSE) {
            AI_SESSION_CLOSE_ATTR_T *close = &attr_info.value.close;
            return __ai_biz_session_destory(close->id, close->code, false);
        }

        offset += sizeof(uint32_t);
        payload = data + offset;

        if (type == AI_PT_EVENT) {
            rt = __ai_biz_recv_event(&attr_info.value.event, payload);
            return rt;
        }

        rt = __ai_parse_biz_head(type, payload, &biz_head, &offset);
        if (OPRT_OK != rt) {
            return rt;
        }

        uint16_t recv_id = 0;
        memcpy(&recv_id, payload, sizeof(uint16_t));
        recv_id = UNI_NTOHS(recv_id);
        AI_PROTO_D("recv data id:%d", recv_id);

        tal_mutex_lock(ai_basic_biz->mutex);
        for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
            if (ai_basic_biz->session[idx].id[0] != 0) {
                AI_SESSION_T *session = &ai_basic_biz->session[idx];
                uint32_t sidx = 0;
                for (sidx = 0; sidx < session->cfg.recv_num; sidx++) {
                    if (session->cfg.recv[sidx].id == recv_id) {
                        usr_data = session->cfg.recv[sidx].usr_data;
                        if (session->cfg.recv[sidx].cb) {
                            cb = session->cfg.recv[sidx].cb;
                            break;
                        }
                    }
                }
                if (cb) {
                    break;
                }
            }
        }
        tal_mutex_unlock(ai_basic_biz->mutex);
        if (cb) {
            AI_PROTO_D("recv data id:%d, call cb: %p", recv_id, cb);
            rt = cb(&attr_info, &biz_head, payload + offset, usr_data);
            if (rt != OPRT_OK) {
                PR_ERR("recv data handle failed, rt:%d", rt);
            }
            ai_basic_biz->cb = cb;
        }
        if (idx == AI_SESSION_MAX_NUM) {
            PR_ERR("session not found");
            return OPRT_COM_ERROR;
        }
    } else {
        biz_head.len = len;
        biz_head.stream_flag = AI_STREAM_ING;
        if (ai_basic_biz->cb) {
            rt = ai_basic_biz->cb(NULL, &biz_head, data, usr_data);
            if (rt != OPRT_OK) {
                PR_ERR("recv data handle failed, rt:%d", rt);
            }
        }
    }
    return rt;
}

static OPERATE_RET __ai_clt_close_evt(void *data)
{
    uint32_t idx = 0;

    if (ai_basic_biz == NULL) {
        return OPRT_OK;
    }
    tal_mutex_lock(ai_basic_biz->mutex);
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_basic_biz->session[idx].id[0] != 0) {
            PR_NOTICE("close session id:%s", ai_basic_biz->session[idx].id);
            tal_event_publish(EVENT_AI_SESSION_CLOSE, ai_basic_biz->session[idx].id);
            memset(&ai_basic_biz->session[idx], 0, sizeof(AI_SESSION_T));
        }
    }
    tal_mutex_unlock(ai_basic_biz->mutex);
    AI_PROTO_D("close all session success");
    return OPRT_OK;
}

static OPERATE_RET __ai_clt_run_evt(void *data)
{
    OPERATE_RET rt = OPRT_OK;
    if (NULL == ai_basic_biz) {
        ai_basic_biz = (AI_BASIC_BIZ_T *)Malloc(sizeof(AI_BASIC_BIZ_T));
        TUYA_CHECK_NULL_RETURN(ai_basic_biz, OPRT_MALLOC_FAILED);
        memset(ai_basic_biz, 0, sizeof(AI_BASIC_BIZ_T));
        TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&ai_basic_biz->mutex), EXIT);
        tuya_ai_client_reg_cb(__ai_biz_recv_handle);
        PR_NOTICE("ai biz init success");
    }
    tal_event_publish(EVENT_AI_SESSION_NEW, NULL);
    PR_NOTICE("ai biz publish session new event");
    return rt;

EXIT:
    __ai_biz_deinit();
    return rt;
}

OPERATE_RET tuya_ai_biz_init(void)
{
    tal_event_subscribe(EVENT_AI_CLIENT_RUN, "ai.biz", __ai_clt_run_evt, SUBSCRIBE_TYPE_NORMAL);
    tal_event_subscribe(EVENT_AI_CLIENT_CLOSE, "ai.biz", __ai_clt_close_evt, SUBSCRIBE_TYPE_NORMAL);
    return OPRT_OK;
}

static OPERATE_RET __ai_pack_session_data(AI_SESSION_CFG_T *cfg, AI_SESSION_NEW_ATTR_T *attr)
{
    OPERATE_RET rt = OPRT_OK;
    uint16_t send_ids_len = cfg->send_num * sizeof(uint16_t);
    uint16_t recv_ids_len = cfg->recv_num * sizeof(uint16_t);
    uint32_t data_len = sizeof(send_ids_len) + send_ids_len + sizeof(recv_ids_len) + recv_ids_len;
    char *data = Malloc(data_len);
    TUYA_CHECK_NULL_RETURN(data, OPRT_MALLOC_FAILED);
    memset(data, 0, data_len);

    uint32_t offset = 0, idx = 0;
    uint16_t id = 0;
    AI_PROTO_D("send_ids_len:%d, recv_ids_len:%d", send_ids_len, recv_ids_len);
    send_ids_len = UNI_HTONS(send_ids_len);
    memcpy(data + offset, &send_ids_len, sizeof(send_ids_len));
    offset += sizeof(send_ids_len);
    for (idx = 0; idx < cfg->send_num; idx++) {
        id = UNI_HTONS(cfg->send[idx].id);
        memcpy(data + offset, &id, sizeof(uint16_t));
        offset += sizeof(uint16_t);
    }
    recv_ids_len = UNI_HTONS(recv_ids_len);
    memcpy(data + offset, &recv_ids_len, sizeof(recv_ids_len));
    offset += sizeof(recv_ids_len);
    for (idx = 0; idx < cfg->recv_num; idx++) {
        id = UNI_HTONS(cfg->recv[idx].id);
        memcpy(data + offset, &id, sizeof(uint16_t));
        offset += sizeof(uint16_t);
    }

    rt = tuya_ai_basic_session_new(attr, data, data_len);
    if (OPRT_OK != rt) {
        PR_ERR("create session failed, rt:%d", rt);
    }
    Free(data);
    return rt;
}

OPERATE_RET tuya_ai_biz_crt_session(uint32_t bizCode, AI_SESSION_CFG_T *cfg, uint8_t *attr, uint32_t attr_len,
                                    AI_SESSION_ID id)
{
    OPERATE_RET rt = OPRT_OK;
    if (ai_basic_biz == NULL) {
        PR_ERR("ai biz is null");
        return OPRT_COM_ERROR;
    }

    rt = tuya_ai_basic_uuid_v4(id);
    if (OPRT_OK != rt) {
        PR_ERR("create session id failed, rt:%d", rt);
        return rt;
    }

    PR_NOTICE("create session id:%s,%d", id, strlen(id));
    AI_SESSION_NEW_ATTR_T session_attr = {.biz_code = bizCode, .id = id, .user_data = attr, .user_len = attr_len};
    rt = __ai_pack_session_data(cfg, &session_attr);
    if (rt != OPRT_OK) {
        PR_ERR("pack session data failed, rt:%d", rt);
        return rt;
    }

    tal_mutex_lock(ai_basic_biz->mutex);
    uint32_t idx = 0;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_basic_biz->session[idx].id[0] == 0) {
            memcpy(&ai_basic_biz->session[idx].cfg, cfg, sizeof(AI_SESSION_CFG_T));
            memcpy(ai_basic_biz->session[idx].id, id, strlen(id));
            AI_PROTO_D("create session idx:%d", idx);
            break;
        }
    }
    if (__ai_biz_need_send_task()) {
        __ai_biz_create_task();
    }
    tal_mutex_unlock(ai_basic_biz->mutex);

    if (idx == AI_SESSION_MAX_NUM) {
        PR_ERR("session num is full");
        return rt;
    }
    AI_PROTO_D("create session success");
    return rt;
}

OPERATE_RET tuya_ai_biz_del_session(AI_SESSION_ID id, AI_STATUS_CODE code)
{
    return __ai_biz_session_destory(id, code, true);
}

int tuya_ai_biz_get_send_id(void)
{
    static int odd_number = 1;
    int id = odd_number;
    odd_number += 2;
    return id;
}

int tuya_ai_biz_get_recv_id(void)
{
    static int even_number = 2;
    int id = even_number;
    even_number += 2;
    return id;
}
