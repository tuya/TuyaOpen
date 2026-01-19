/**
 * @file tuya_ai_biz.c
 * @author tuya
 * @brief ai business
 * @version 0.1
 * @date 2025-03-04
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
#include "tuya_ai_biz.h"
#include "base_event.h"
#include "tuya_ai_private.h"

#ifndef AI_SESSION_MAX_NUM
#define AI_SESSION_MAX_NUM 2
#endif
#ifndef AI_BIZ_TASK_DELAY
#define AI_BIZ_TASK_DELAY 10
#endif

typedef struct {
    char id[AI_UUID_V4_LEN];
    AI_SESSION_CFG_T cfg;
} AI_SESSION_T;

typedef struct {
    AI_BIZ_MONITOR_CB recv_cb;
    AI_BIZ_MONITOR_CB send_cb;
    VOID *usr_data;
} AI_BASIC_BIZ_MONITOR_T;

typedef struct {
    THREAD_HANDLE thread;
    BOOL_T terminate;
    MUTEX_HANDLE mutex;
    AI_SESSION_T session[AI_SESSION_MAX_NUM];
    AI_BIZ_RECV_CB cb;
    AI_BASIC_BIZ_MONITOR_T *monitor;
} AI_BASIC_BIZ_T;

AI_BASIC_BIZ_MONITOR_T ai_monitor;
AI_BASIC_BIZ_T *ai_basic_biz;

#if defined(AI_VERSION) && (0x02 == AI_VERSION)
STATIC char __get_varint_len(char *data, size_t max_len)
{
    size_t len = 0;
    while (len < max_len) {
        len++;
        if ((data[len - 1] & 0x80) == 0) {
            return len;
        }
    }
    return 0;
}
#endif

OPERATE_RET tuya_ai_send_biz_pkt(uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_PACKET_PT type, AI_BIZ_HEAD_INFO_T *head, char *payload)
{
    OPERATE_RET rt = OPRT_OK;
    if (ai_basic_biz == NULL) {
        PR_ERR("ai biz is null");
        return OPRT_COM_ERROR;
    }

    if (ai_basic_biz->monitor && ai_basic_biz->monitor->send_cb) {
        if (attr == NULL || !(attr->flag & AI_HAS_ATTR)) {
            AI_BIZ_ATTR_INFO_T attr = {
                .flag = AI_NO_ATTR,
                .type = type,
            };
            rt = ai_basic_biz->monitor->send_cb(id, &attr, head, payload, ai_basic_biz->monitor->usr_data);
        } else {
            rt = ai_basic_biz->monitor->send_cb(id, attr, head, payload, ai_basic_biz->monitor->usr_data);
        }
        if (OPRT_OK != rt) {
            PR_ERR("send cb failed, rt:%d", rt);
            return rt;
        }
    }

    return tuya_ai_send_biz_pkt_custom(id, attr, type, head, payload, NULL);
}

OPERATE_RET tuya_ai_send_biz_pkt_custom(uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_PACKET_PT type,
                                        AI_BIZ_HEAD_INFO_T *head, char *payload, AI_PACKET_WRITER_T *writer)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t payload_len = 0, total_len = 0;
#if defined(AI_VERSION) && (0x02 == AI_VERSION)
    uint32_t seq_len = 0;
    char seq_array[10] = {0};
#endif
    if (ai_basic_biz == NULL) {
        PR_ERR("ai biz is null");
        return OPRT_COM_ERROR;
    }
    if (head->total_len == 0) {
        head->total_len = head->len;
    }
    AI_PROTO_D("biz len:%d, total len:%d", head->len, head->total_len);
    tuya_ai_client_start_ping();
    if (type == AI_PT_VIDEO) {
        int video_head_len = 0;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        video_head_len = SIZEOF(AI_VIDEO_HEAD_T);
#else
        video_head_len = SIZEOF(AI_VIDEO_HEAD_T_V2);
#endif
        payload_len = video_head_len + head->len;
        total_len = video_head_len + head->total_len;
        char *video = OS_MALLOC(payload_len);
        TUYA_CHECK_NULL_RETURN(video, OPRT_MALLOC_FAILED);
        memset(video, 0, payload_len);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        AI_VIDEO_HEAD_T *video_head = (AI_VIDEO_HEAD_T *)video;
#else
        AI_VIDEO_HEAD_T_V2 *video_head = (AI_VIDEO_HEAD_T_V2 *)video;
#endif
        video_head->id = UNI_HTONS(id);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        video_head->stream_flag = head->stream_flag;
        video_head->timestamp = head->value.video.timestamp;
        UNI_HTONLL(video_head->timestamp);
#else
        uint64_t val = ((uint64_t)(head->stream_flag & 0x03) << (4 + 42)) |
                       (0ULL << 42) |
                       (head->value.video.timestamp & ((1ULL << 42) - 1));
        video[2] = (uint8_t)(val >> 40);
        video[3] = (uint8_t)(val >> 32);
        video[4] = (uint8_t)(val >> 24);
        video[5] = (uint8_t)(val >> 16);
        video[6] = (uint8_t)(val >> 8);
        video[7] = (uint8_t)(val);
#endif

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        video_head->pts = head->value.video.pts;
        UNI_HTONLL(video_head->pts);
        video_head->length = UNI_HTONL(head->total_len);
#endif
        if (payload && head->len) {
            memcpy(video + video_head_len, payload, head->len);
        }
        if (attr && (attr->flag == AI_HAS_ATTR)) {
            rt = tuya_ai_basic_video(&(attr->value.video), video, payload_len, total_len, writer);
        } else {
            rt = tuya_ai_basic_video(NULL, video, payload_len, total_len, writer);
        }
        OS_FREE(video);
    } else if (type == AI_PT_AUDIO) {
        int audio_head_len = 0;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        audio_head_len = SIZEOF(AI_AUDIO_HEAD_T);
#else
        audio_head_len = SIZEOF(AI_AUDIO_HEAD_T_V2);
#endif
        payload_len = audio_head_len + head->len;
        total_len = audio_head_len + head->total_len;
        char *audio = OS_MALLOC(payload_len);
        TUYA_CHECK_NULL_RETURN(audio, OPRT_MALLOC_FAILED);
        memset(audio, 0, payload_len);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        AI_AUDIO_HEAD_T *audio_head = (AI_AUDIO_HEAD_T *)audio;
#else
        AI_AUDIO_HEAD_T_V2 *audio_head = (AI_AUDIO_HEAD_T_V2 *)audio;
#endif

        audio_head->id = UNI_HTONS(id);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        audio_head->stream_flag = head->stream_flag;
        audio_head->timestamp = head->value.audio.timestamp;
        UNI_HTONLL(audio_head->timestamp);
#else
        uint64_t val = ((uint64_t)(head->stream_flag & 0x03) << (4 + 42)) |
                       (0ULL << 42) |
                       (head->value.audio.timestamp & ((1ULL << 42) - 1));
        audio[2] = (uint8_t)(val >> 40);
        audio[3] = (uint8_t)(val >> 32);
        audio[4] = (uint8_t)(val >> 24);
        audio[5] = (uint8_t)(val >> 16);
        audio[6] = (uint8_t)(val >> 8);
        audio[7] = (uint8_t)(val);
#endif

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        audio_head->pts = head->value.audio.pts;
        UNI_HTONLL(audio_head->pts);
        audio_head->length = UNI_HTONL(head->total_len);
#endif
        if (payload && head->len) {
            memcpy(audio + audio_head_len, payload, head->len);
        }
        if (attr && (attr->flag == AI_HAS_ATTR)) {
            rt = tuya_ai_basic_audio(&(attr->value.audio), audio, payload_len, total_len, writer);
        } else {
            rt = tuya_ai_basic_audio(NULL, audio, payload_len, total_len, writer);
        }
        OS_FREE(audio);
    } else if (type == AI_PT_IMAGE) {
        int image_head_len = 0;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        image_head_len = SIZEOF(AI_IMAGE_HEAD_T);
#else
        image_head_len = SIZEOF(AI_IMAGE_HEAD_T_V2);
#endif

        payload_len = image_head_len + head->len;
        total_len = image_head_len + head->total_len;
        char *image = OS_MALLOC(payload_len);
        TUYA_CHECK_NULL_RETURN(image, OPRT_MALLOC_FAILED);
        memset(image, 0, payload_len);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        AI_IMAGE_HEAD_T *image_head = (AI_IMAGE_HEAD_T *)image;
#else
        AI_IMAGE_HEAD_T_V2 *image_head = (AI_IMAGE_HEAD_T_V2 *)image;
#endif
        image_head->id = UNI_HTONS(id);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        image_head->stream_flag = head->stream_flag;
        image_head->timestamp = head->value.image.timestamp;
        UNI_HTONLL(image_head->timestamp);
        image_head->length = UNI_HTONL(head->total_len);
#else
        uint64_t val = ((uint64_t)(head->stream_flag & 0x03) << (4 + 42)) |
                       (0ULL << 42) |
                       (head->value.image.timestamp & ((1ULL << 42) - 1));
        image[2] = (uint8_t)(val >> 40);
        image[3] = (uint8_t)(val >> 32);
        image[4] = (uint8_t)(val >> 24);
        image[5] = (uint8_t)(val >> 16);
        image[6] = (uint8_t)(val >> 8);
        image[7] = (uint8_t)(val);
#endif
        if (payload && head->len) {
            memcpy(image + image_head_len, payload, head->len);
        }
        if (attr && (attr->flag == AI_HAS_ATTR)) {
            rt = tuya_ai_basic_image(&(attr->value.image), image, payload_len, total_len, writer);
        } else {
            rt = tuya_ai_basic_image(NULL, image, payload_len, total_len, writer);
        }
        OS_FREE(image);
    } else if (type == AI_PT_FILE) {
        int file_head_len = 0;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        file_head_len = SIZEOF(AI_FILE_HEAD_T);
#else
        seq_len = tuya_ai_basic_get_var_seq(AI_PT_FILE, seq_array);
        file_head_len = SIZEOF(AI_FILE_HEAD_T_V2) + seq_len;
#endif

        payload_len = file_head_len + head->len;
        total_len = file_head_len + head->total_len;
        char *file = OS_MALLOC(payload_len);
        TUYA_CHECK_NULL_RETURN(file, OPRT_MALLOC_FAILED);
        memset(file, 0, payload_len);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        AI_FILE_HEAD_T *file_head = (AI_FILE_HEAD_T *)file;
#else
        AI_FILE_HEAD_T_V2 *file_head = (AI_FILE_HEAD_T_V2 *)file;
#endif
        file_head->id = UNI_HTONS(id);
        file_head->stream_flag = head->stream_flag;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        file_head->length = UNI_HTONL(head->total_len);
#else
        //seq
        memcpy(file + file_head_len - seq_len, seq_array, seq_len);
#endif
        if (payload && head->len) {
            memcpy(file + file_head_len, payload, head->len);
        }
        if (attr && (attr->flag == AI_HAS_ATTR)) {
            rt = tuya_ai_basic_file(&(attr->value.file), file, payload_len, total_len, writer);
        } else {
            rt = tuya_ai_basic_file(NULL, file, payload_len, total_len, writer);
        }
        if (payload_len == total_len) {
            tuya_ai_basic_update_var_seq(AI_PT_FILE);
        }
        OS_FREE(file);
    } else if (type == AI_PT_TEXT) {

        int text_head_len = 0;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        text_head_len = SIZEOF(AI_TEXT_HEAD_T);
#else
        seq_len = tuya_ai_basic_get_var_seq(AI_PT_TEXT, seq_array);
        text_head_len = SIZEOF(AI_TEXT_HEAD_T_V2) + seq_len;
#endif

        payload_len = text_head_len + head->len;
        total_len = text_head_len + head->total_len;
        char *text = OS_MALLOC(payload_len);
        TUYA_CHECK_NULL_RETURN(text, OPRT_MALLOC_FAILED);
        memset(text, 0, payload_len);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        AI_TEXT_HEAD_T *text_head = (AI_TEXT_HEAD_T *)text;
#else
        AI_TEXT_HEAD_T_V2 *text_head = (AI_TEXT_HEAD_T_V2 *)text;
#endif
        text_head->id = UNI_HTONS(id);
        text_head->stream_flag = head->stream_flag;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        text_head->length = UNI_HTONL(head->total_len);
#else
        //seq
        memcpy(text + text_head_len - seq_len, seq_array, seq_len);
#endif
        if (payload && head->len) {
            memcpy(text + text_head_len, payload, head->len);
        }
        if (attr && (attr->flag == AI_HAS_ATTR)) {
            rt = tuya_ai_basic_text(&(attr->value.text), text, payload_len, total_len, writer);
        } else {
            rt = tuya_ai_basic_text(NULL, text, payload_len, total_len, writer);
        }
        if (payload_len == total_len) {
            tuya_ai_basic_update_var_seq(AI_PT_TEXT);
        }
        OS_FREE(text);
    } else if (type == AI_PT_EVENT) {
        payload_len = SIZEOF(AI_EVENT_HEAD_T) + head->len;
        total_len = SIZEOF(AI_EVENT_HEAD_T) + head->total_len;
        char *event = OS_MALLOC(payload_len);
        TUYA_CHECK_NULL_RETURN(event, OPRT_MALLOC_FAILED);
        memset(event, 0, payload_len);
        AI_EVENT_HEAD_T *event_head = (AI_EVENT_HEAD_T *)event;
        event_head->type = UNI_HTONS(id);
        event_head->length = UNI_HTONL(head->total_len);
        if (payload && head->len) {
            memcpy(event + SIZEOF(AI_EVENT_HEAD_T), payload, head->len);
        }
        if (attr && (attr->flag == AI_HAS_ATTR)) {
            rt = tuya_ai_basic_event(&(attr->value.event), event, payload_len, writer);
        } else {
            rt = tuya_ai_basic_event(NULL, event, payload_len, writer);
        }
        OS_FREE(event);
    } else {
        PR_ERR("unknow type:%d", type);
        rt = OPRT_COM_ERROR;
    }

    if (rt != OPRT_OK) {
        PR_ERR("send biz data failed, rt:%d", rt);
    }
    return rt;
}

STATIC VOID __ai_biz_thread_exit()
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
        OS_FREE(ai_basic_biz);
        ai_basic_biz = NULL;
    }
}

STATIC VOID __ai_biz_thread_cb(void* args)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t idx = 0, sidx = 0, kdx = 0;
    while (!ai_basic_biz->terminate && tal_thread_get_state(ai_basic_biz->thread) == THREAD_STATE_RUNNING) {
        if (!tuya_ai_client_is_ready()) {
            tal_system_sleep(200);
            continue;
        }
        tal_mutex_lock(ai_basic_biz->mutex);
        uint16_t sent_ids[AI_BIZ_MAX_NUM * AI_SESSION_MAX_NUM] = {0};
        uint32_t sent_ids_count = 0;
        for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
            if (ai_basic_biz->session[idx].id[0] != 0) {
                AI_SESSION_T *session = &ai_basic_biz->session[idx];
                for (sidx = 0; sidx < session->cfg.send_num; sidx++) {
                    uint16_t send_id = session->cfg.send[sidx].id;
                    BOOL_T already_sent = FALSE;
                    for (kdx = 0; kdx < sent_ids_count; kdx++) {
                        if (sent_ids[kdx] == send_id) {
                            already_sent = TRUE;
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
    __ai_biz_thread_exit();
    PR_NOTICE("ai biz thread exit");
    return;
}

STATIC BOOL_T __ai_biz_need_send_task(VOID)
{
    uint32_t idx = 0, sidx = 0;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_basic_biz->session[idx].id[0] != 0) {
            AI_SESSION_T *session = &ai_basic_biz->session[idx];
            for (sidx = 0; sidx < session->cfg.send_num; sidx++) {
                if (session->cfg.send[sidx].get_cb) {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

STATIC OPERATE_RET __ai_biz_create_task(VOID)
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

OPERATE_RET __ai_parse_video_attr(char *de_buf, uint32_t attr_len, AI_VIDEO_ATTR_T *video)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    AI_ATTRIBUTE_T attr = {0};

    while (offset < attr_len) {
        memset(&attr, 0, SIZEOF(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(de_buf, &offset, &attr);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
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
        }
#else
        if (attr.type == AI_ATTR_VIDEO_PARAMS) {
            AI_PROTO_D("parase vedio params attr value:%s", attr.value.str);
            uint32_t codec_type = 0, sample_rate = 0, width = 0, height = 0, fps = 0;
            char parased = sscanf(attr.value.str, "%d %d %d %d %d", &codec_type, &width, &height, &fps, &sample_rate);
            if (OPRT_COM_ERROR == parased) {
                PR_ERR("parase vedio params attr value failed, rt:%d ", parased);
                return parased;
            }
            video->base.codec_type = (AI_VIDEO_CODEC_TYPE)codec_type;
            video->base.sample_rate = (uint32_t)sample_rate;
            video->base.width = (uint16_t)width;
            video->base.height = (uint16_t)height;
            video->base.fps = (uint16_t)fps;
        }
#endif
        else if (attr.type == AI_ATTR_USER_DATA) {
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
        memset(&attr, 0, SIZEOF(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(de_buf, &offset, &attr);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        if (attr.type == AI_ATTR_AUDIO_CODEC_TYPE) {
            audio->base.codec_type = attr.value.u16;
        } else if (attr.type == AI_ATTR_AUDIO_SAMPLE_RATE) {
            audio->base.sample_rate = attr.value.u32;
        } else if (attr.type == AI_ATTR_AUDIO_CHANNELS) {
            audio->base.channels = attr.value.u16;
        } else if (attr.type == AI_ATTR_AUDIO_DEPTH) {
            audio->base.bit_depth = attr.value.u16;
        }
#else
        if (attr.type == AI_ATTR_AUDIO_PARAMS) {
            AI_PROTO_D("parase audio params attr value:%s", attr.value.str);
            uint32_t codec_type = 0, sample_rate = 0, channels = 0, bit_depth = 0;
            char parased = sscanf(attr.value.str, "%d %d %d %d", &codec_type, &channels, &bit_depth, &sample_rate);
            if (OPRT_COM_ERROR == parased) {
                PR_ERR("parase audio params attr value failed, rt:%d ", parased);
                return parased;
            }
            audio->base.codec_type = (AI_AUDIO_CODEC_TYPE)codec_type;
            audio->base.channels = (AI_AUDIO_CHANNELS)channels;
            audio->base.bit_depth = (uint16_t)bit_depth;
            audio->base.sample_rate = (uint32_t)sample_rate;
        }
#endif
        else if (attr.type == AI_ATTR_USER_DATA) {
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
        memset(&attr, 0, SIZEOF(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(de_buf, &offset, &attr);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        if (attr.type == AI_ATTR_IMAGE_FORMAT) {
            image->base.format = attr.value.u8;
        } else if (attr.type == AI_ATTR_IMAGE_WIDTH) {
            image->base.width = attr.value.u16;
        } else if (attr.type == AI_ATTR_IMAGE_HEIGHT) {
            image->base.height = attr.value.u16;
        }
#else
        if (AI_ATTR_IMAGE_PARAMS == attr.type) {
            AI_PROTO_D("parase image params attr value:%s", attr.value.str);
            uint32_t image_type = 0, image_format = 0, image_width = 0, image_height = 0;
            char parased = sscanf(attr.value.str, "%d %d %d %d", &image_type, &image_format, &image_width, &image_height);
            if (OPRT_COM_ERROR == parased) {
                PR_ERR("parase image params attr value failed, rt:%d ", parased);
                return parased;
            }
            image->base.type = (AI_IMAGE_PAYLOAD_TYPE)image_type;
            image->base.format = (AI_IMAGE_FORMAT)image_format;
            image->base.width = (uint16_t)image_width;
            image->base.height = (uint16_t)image_height;
            if (image_type > IMAGE_PAYLOAD_TYPE_URL) {
                PR_ERR("image payload type invaild %d", image_type);
                return OPRT_INVALID_PARM;
            }
        }
#endif
        else if (attr.type == AI_ATTR_USER_DATA) {
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
        memset(&attr, 0, SIZEOF(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(de_buf, &offset, &attr);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        if (attr.type == AI_ATTR_FILE_FORMAT) {
            file->base.format = attr.value.u8;
        } else if (attr.type == AI_ATTR_FILE_NAME) {
            if (attr.length > SIZEOF(file->base.file_name)) {
                PR_ERR("file name too long %d", attr.length);
                return OPRT_INVALID_PARM;
            }
            memcpy(file->base.file_name, attr.value.str, attr.length);
        }
#else
        if (AI_ATTR_FILE_PARAMS == attr.type) {
            AI_PROTO_D("parase file params attr value:%s", attr.value.str);
            uint32_t file_type = 0, file_format = 0;
            uint8_t file_name[256] = {0};
            char parased = sscanf(attr.value.str, "%d %d %s", &file_type, &file_format, file_name);
            if (OPRT_COM_ERROR == parased) {
                PR_ERR("parase image params attr value failed, rt:%d ", parased);
                return parased;
            }
            file->base.type = (AI_FILE_PAYLOAD_TYPE)file_type;
            file->base.format = (AI_FILE_FORMAT)file_format;

            if ((strlen((char *)file_name) > SIZEOF(file->base.file_name) ||
                 (file_type > FILE_PAYLOAD_TYPE_URL))) {
                PR_ERR("file params invalid %s, %d", file_name, file_type);
                return OPRT_INVALID_PARM;
            }
            memcpy(file->base.file_name, file_name, strlen((char *)file_name));
        }
#endif
        else if (attr.type == AI_ATTR_USER_DATA) {
            file->option.user_data = attr.value.bytes;
            file->option.user_len = attr.length;
        } else if (attr.type == AI_ATTR_SESSION_ID_LIST) {
            file->option.session_id_list = attr.value.str;
        } else {
            PR_ERR("unknow attr type:%d", attr.type);
        }
    }
    if (file->base.file_name[0] == '\0') {
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
        memset(&attr, 0, SIZEOF(AI_ATTRIBUTE_T));
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
        memset(&attr, 0, SIZEOF(AI_ATTRIBUTE_T));
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
        } else if (attr.type == AI_ATTR_CMD_DATA) {
            event->cmd_data = attr.value.str;
        } else if (attr.type == AI_ATTR_ASSIGN_DATAS) {
            event->assign_datas = attr.value.str;
        } else if (attr.type == AI_ATTR_UNASSIGN_DATAS) {
            event->unassign_datas = attr.value.str;
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
        memset(&attr, 0, SIZEOF(AI_ATTRIBUTE_T));
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
            PR_NOTICE("close session err code:%d", close->code);
        } else {
            PR_ERR("unknow attr type:%d", attr.type);
        }
    }
    if (NULL == close->id) {
        PR_ERR("close session id was null");
        return OPRT_INVALID_PARM;
    }

    return rt;
}

OPERATE_RET __ai_parse_session_state_attr(char *de_buf, uint32_t attr_len, AI_SESSION_STATE_ATTR_T *state)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    AI_ATTRIBUTE_T attr = {0};

    while (offset < attr_len) {
        memset(&attr, 0, SIZEOF(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(de_buf, &offset, &attr);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }

        if (attr.type == AI_ATTR_SESSION_ID) {
            state->id = attr.value.str;
            PR_NOTICE("state session id:%s", state->id);
        } else if (attr.type == AI_ATTR_SESSION_STATE_CHANGE_CODE) {
            state->code = attr.value.u16;
            PR_NOTICE("state session code:%d", state->code);
        } else if (attr.type == AI_ATTR_USER_DATA) {
            state->user_data = attr.value.bytes;
            state->user_len = attr.length;
        } else {
            PR_ERR("unknow attr type:%d", attr.type);
        }
    }
    if (NULL == state->id) {
        PR_ERR("session state id was null");
        return OPRT_INVALID_PARM;
    }

    return rt;
}

STATIC OPERATE_RET __ai_parse_biz_attr(AI_PACKET_PT type, char *attr_buf, uint32_t attr_len, AI_BIZ_ATTR_INFO_T *attr)
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
            PR_ERR("parse close attr failed, rt:%d", rt);
            return rt;
        }
    } else if (type == AI_PT_SESSION_STATE_CHANGE) {
        rt = __ai_parse_session_state_attr(attr_buf, attr_len, &attr->value.state);
        if (OPRT_OK != rt) {
            PR_ERR("parse state attr failed, rt:%d", rt);
            return rt;
        }
    } else {
        PR_ERR("unknow type:%d", type);
        return OPRT_INVALID_PARM;
    }
    return rt;
}

STATIC OPERATE_RET __ai_parse_biz_head(AI_PACKET_PT type, char *payload, AI_BIZ_HEAD_INFO_T *biz_head, uint32_t *offset, uint32_t payload_len)
{
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    if (type == AI_PT_VIDEO) {
        AI_VIDEO_HEAD_T *video_head = (AI_VIDEO_HEAD_T *)payload;
        biz_head->stream_flag = video_head->stream_flag;
        biz_head->value.video.timestamp = video_head->timestamp;
        UNI_NTOHLL(biz_head->value.video.timestamp);
        biz_head->value.video.pts = video_head->pts;
        UNI_NTOHLL(biz_head->value.video.pts);
        biz_head->len = UNI_NTOHL(video_head->length);
        *offset = SIZEOF(AI_VIDEO_HEAD_T);
    } else if (type == AI_PT_AUDIO) {
        AI_AUDIO_HEAD_T *audio_head = (AI_AUDIO_HEAD_T *)payload;
        biz_head->stream_flag = audio_head->stream_flag;
        biz_head->value.audio.timestamp = audio_head->timestamp;
        UNI_NTOHLL(biz_head->value.audio.timestamp);
        biz_head->value.audio.pts = audio_head->pts;
        UNI_NTOHLL(biz_head->value.audio.pts);
        biz_head->len = UNI_NTOHL(audio_head->length);
        *offset = SIZEOF(AI_AUDIO_HEAD_T);
    } else if (type == AI_PT_IMAGE) {
        AI_IMAGE_HEAD_T *image_head = (AI_IMAGE_HEAD_T *)payload;
        biz_head->stream_flag = image_head->stream_flag;
        biz_head->value.image.timestamp = image_head->timestamp;
        UNI_NTOHLL(biz_head->value.image.timestamp);
        biz_head->len = UNI_NTOHL(image_head->length);
        *offset = SIZEOF(AI_IMAGE_HEAD_T);
    } else if (type == AI_PT_FILE) {
        AI_FILE_HEAD_T *file_head = (AI_FILE_HEAD_T *)payload;
        biz_head->stream_flag = file_head->stream_flag;
        biz_head->len = UNI_NTOHL(file_head->length);
        *offset = SIZEOF(AI_FILE_HEAD_T);
    } else if (type == AI_PT_TEXT) {
        AI_TEXT_HEAD_T *text_head = (AI_TEXT_HEAD_T *)payload;
        biz_head->stream_flag = text_head->stream_flag;
        biz_head->len = UNI_NTOHL(text_head->length);
        *offset = SIZEOF(AI_TEXT_HEAD_T);
    }
#else
    if ((type == AI_PT_VIDEO) || (type == AI_PT_AUDIO) || (type == AI_PT_IMAGE)) {
        // tuya_debug_hex_dump("image_head:", 64, (uint8_t *)payload, 8);
        biz_head->stream_flag = (payload[2] & 0xc0) >> 6;
        AI_PROTO_D("stream flag:%d", biz_head->stream_flag);
        uint64_t temp_time = 0;
        memcpy(&temp_time, payload, SIZEOF(uint64_t));
        UNI_NTOHLL(temp_time);
        temp_time = temp_time & ((1ULL << 42) - 1);
        biz_head->value.image.timestamp = temp_time;
        AI_PROTO_D("timestamp:%llu", biz_head->value.image.timestamp);
        biz_head->len = payload_len - SIZEOF(AI_VIDEO_HEAD_T_V2);
        *offset = SIZEOF(AI_VIDEO_HEAD_T_V2);
    } else if ((type == AI_PT_FILE) || (type == AI_PT_TEXT)) {
        AI_FILE_HEAD_T_V2 *file_head = (AI_FILE_HEAD_T_V2 *)payload;
        biz_head->stream_flag = file_head->stream_flag;
        AI_PROTO_D("stream flag:%d", biz_head->stream_flag);
        *offset = SIZEOF(AI_FILE_HEAD_T_V2);
        uint8_t var_len = __get_varint_len(payload, payload_len);
        biz_head->len = payload_len - SIZEOF(AI_FILE_HEAD_T_V2) - var_len;
        AI_PROTO_D("var_len : %d ,payload_len : %d ", var_len, biz_head->len);
        *offset += var_len;
    }
#endif
    else if (type == AI_PT_EVENT) {
        AI_EVENT_HEAD_T *event_head = (AI_EVENT_HEAD_T *)payload;
        biz_head->stream_flag = 0;
        biz_head->len = UNI_NTOHL(event_head->length);
        *offset = SIZEOF(AI_EVENT_HEAD_T);
    } else {
        PR_ERR("unknow type:%d", type);
        return OPRT_INVALID_PARM;
    }
    // AI_PROTO_D("biz head len:%d, offset:%d", biz_head->len, *offset);
    return OPRT_OK;
}

STATIC BOOL_T __ai_is_biz_pkt_vaild(AI_PACKET_PT type)
{
    if ((type != AI_PT_AUDIO) && (type != AI_PT_VIDEO) && (type != AI_PT_IMAGE) &&
        (type != AI_PT_FILE) && (type != AI_PT_TEXT) && (type != AI_PT_EVENT) &&
        (type != AI_PT_SESSION_CLOSE) && (type != AI_PT_SESSION_STATE_CHANGE)) {
        PR_ERR("recv data type error %d", type);
        return FALSE;
    }
    return TRUE;
}

STATIC OPERATE_RET __ai_biz_recv_event(AI_EVENT_ATTR_T *event, AI_EVENT_HEAD_T *head, VOID *data)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t idx = 0;

    tal_mutex_lock(ai_basic_biz->mutex);
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if ((ai_basic_biz->session[idx].id[0] != 0) &&
            (!strcmp(ai_basic_biz->session[idx].id, event->session_id))) {
            AI_EVENT_CB cb = ai_basic_biz->session[idx].cfg.event_cb;
            if (cb) {
                AI_PROTO_D("recv event type:%d, call cb: %p", head->type, cb);
                rt = cb(event, head, data);
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

STATIC OPERATE_RET __ai_biz_session_destory(AI_SESSION_ID id, AI_STATUS_CODE code, BOOL_T sync_cloud)
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
        if (ai_basic_biz->session[idx].id[0] != 0 &&
            !strcmp(ai_basic_biz->session[idx].id, id)) {
            memset(&ai_basic_biz->session[idx], 0, SIZEOF(AI_SESSION_T));
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
        ty_publish_event(EVENT_AI_SESSION_CLOSE, id);
    }

    return rt;
}

OPERATE_RET __ai_biz_recv_handle(char *data, uint32_t len, AI_FRAG_FLAG frag)
{
    OPERATE_RET rt = OPRT_OK;
    char *payload = NULL, *attr_buf = NULL;
    VOID *usr_data = NULL;
    AI_BIZ_HEAD_INFO_T biz_head = {0};
    AI_BIZ_RECV_CB cb = NULL;
    AI_PROTO_D("recv data len:%d, frag:%d", len, frag);
    if ((frag == AI_PACKET_NO_FRAG) || (frag == AI_PACKET_FRAG_START)) {
        AI_PAYLOAD_HEAD_T *head = (AI_PAYLOAD_HEAD_T *)data;
        AI_PACKET_PT type = head->type;
        AI_ATTR_FLAG attr_flag = head->attribute_flag;
        uint32_t idx = 0, attr_len = 0;
        uint32_t offset = SIZEOF(AI_PAYLOAD_HEAD_T);
        ai_basic_biz->cb = NULL;
        AI_PROTO_D("recv data type:%d, attr_flag:%d", type, attr_flag);
        if (!__ai_is_biz_pkt_vaild(type)) {
            return OPRT_INVALID_PARM;
        }

        AI_BIZ_ATTR_INFO_T attr_info;
        memset(&attr_info, 0, SIZEOF(AI_BIZ_ATTR_INFO_T));
        attr_info.flag = attr_flag;
        attr_info.type = type;
        if (attr_flag == AI_HAS_ATTR) {
            memcpy(&attr_len, data + offset, SIZEOF(attr_len));
            attr_len = UNI_NTOHL(attr_len);
            offset += SIZEOF(attr_len);
            attr_buf = data + offset;
            rt = __ai_parse_biz_attr(type, attr_buf, attr_len, &attr_info);
            if (OPRT_OK != rt) {
                return rt;
            }
            offset += attr_len;
        }

        if (type == AI_PT_SESSION_CLOSE) {
            AI_SESSION_CLOSE_ATTR_T *close = &attr_info.value.close;
            return __ai_biz_session_destory(close->id, close->code, FALSE);
        } else if (type == AI_PT_SESSION_STATE_CHANGE) {
            AI_SESSION_STATE_ATTR_T *state = &attr_info.value.state;
            if (state->code == AI_CODE_AGENT_TOKEN_EXPIRED) {
                PR_NOTICE("agent token expired");
            }
            return OPRT_OK;
        }

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        offset += SIZEOF(uint32_t);
#endif
        payload = data + offset;

        rt = __ai_parse_biz_head(type, payload, &biz_head, &offset, len - offset);
        if (OPRT_OK != rt) {
            return rt;
        }

        if (type == AI_PT_EVENT) {
            AI_EVENT_HEAD_T head = {0};
            memcpy(&head, payload, SIZEOF(AI_EVENT_HEAD_T));
            head.type = UNI_NTOHS(head.type);
            head.length = UNI_NTOHS(head.length);
            if (ai_basic_biz->monitor && ai_basic_biz->monitor->recv_cb) {
                rt = ai_basic_biz->monitor->recv_cb(head.type, &attr_info, &biz_head, payload + offset, ai_basic_biz->monitor->usr_data);
                if (OPRT_OK != rt) {
                    PR_ERR("recv cb failed, rt:%d", rt);
                }
            }
            rt = __ai_biz_recv_event(&attr_info.value.event, &head, payload + offset);
            return rt;
        }

        if (frag == AI_PACKET_FRAG_START) {
            biz_head.len = len; // not packet actually, need fix later
        }

        uint16_t recv_id = 0;
        memcpy(&recv_id, payload, SIZEOF(uint16_t));
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
        if (ai_basic_biz->monitor && ai_basic_biz->monitor->recv_cb) {
            rt = ai_basic_biz->monitor->recv_cb(recv_id, &attr_info, &biz_head, payload + offset, ai_basic_biz->monitor->usr_data);
            if (OPRT_OK != rt) {
                PR_ERR("recv cb failed, rt:%d", rt);
            }
        }
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
        if (ai_basic_biz->monitor && ai_basic_biz->monitor->recv_cb) {
            rt = ai_basic_biz->monitor->recv_cb(0, NULL, &biz_head, data, ai_basic_biz->monitor->usr_data);
            if (OPRT_OK != rt) {
                PR_ERR("recv cb failed, rt:%d", rt);
            }
        }
        if (ai_basic_biz->cb) {
            rt = ai_basic_biz->cb(NULL, &biz_head, data, usr_data);
            if (rt != OPRT_OK) {
                PR_ERR("recv data handle failed, rt:%d", rt);
            }
        }
    }
    return rt;
}

STATIC OPERATE_RET __ai_clt_close_evt(VOID *data)
{
    uint32_t idx = 0;

    if (ai_basic_biz == NULL) {
        PR_ERR("ai biz is null");
        return OPRT_OK;
    }
    tal_mutex_lock(ai_basic_biz->mutex);
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_basic_biz->session[idx].id[0] != 0) {
            PR_NOTICE("close session id:%s", ai_basic_biz->session[idx].id);
            ty_publish_event(EVENT_AI_SESSION_CLOSE, ai_basic_biz->session[idx].id);
            memset(&ai_basic_biz->session[idx], 0, SIZEOF(AI_SESSION_T));
        }
    }
    tal_mutex_unlock(ai_basic_biz->mutex);
    PR_NOTICE("close all session success");
    return OPRT_OK;
}

STATIC OPERATE_RET __ai_clt_run_evt(VOID *data)
{
    OPERATE_RET rt = OPRT_OK;
    if (NULL == ai_basic_biz) {
        ai_basic_biz = (AI_BASIC_BIZ_T *)OS_MALLOC(SIZEOF(AI_BASIC_BIZ_T));
        TUYA_CHECK_NULL_RETURN(ai_basic_biz, OPRT_MALLOC_FAILED);
        memset(ai_basic_biz, 0, SIZEOF(AI_BASIC_BIZ_T));
        ai_basic_biz->monitor = &ai_monitor;
        TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&ai_basic_biz->mutex), EXIT);
        tuya_ai_client_reg_cb(__ai_biz_recv_handle);
        PR_NOTICE("ai biz init success");
    }
    return rt;

EXIT:
    tuya_ai_biz_deinit();
    return rt;
}

OPERATE_RET tuya_ai_biz_init(VOID)
{
    ty_subscribe_event(EVENT_AI_CLIENT_RUN, "ai.biz", __ai_clt_run_evt, SUBSCRIBE_TYPE_EMERGENCY);
    ty_subscribe_event(EVENT_AI_CLIENT_CLOSE, "ai.biz", __ai_clt_close_evt, SUBSCRIBE_TYPE_NORMAL);
    return OPRT_OK;
}

VOID tuya_ai_biz_deinit(VOID)
{
    if (ai_basic_biz) {
        if (ai_basic_biz->thread) {
            ai_basic_biz->terminate = TRUE;
        } else {
            if (ai_basic_biz->mutex) {
                tal_mutex_release(ai_basic_biz->mutex);
                ai_basic_biz->mutex = NULL;
            }
            OS_FREE(ai_basic_biz);
            ai_basic_biz = NULL;
        }
        ty_unsubscribe_event(EVENT_AI_CLIENT_RUN, "ai.biz", __ai_clt_run_evt);
        ty_unsubscribe_event(EVENT_AI_CLIENT_CLOSE, "ai.biz", __ai_clt_close_evt);
        PR_DEBUG("ai biz deinit");
    }
}

STATIC OPERATE_RET __ai_pack_session_data(AI_SESSION_CFG_T *cfg, AI_SESSION_NEW_ATTR_T *attr)
{
    OPERATE_RET rt = OPRT_OK;
    uint16_t send_ids_len = cfg->send_num * SIZEOF(uint16_t);
    uint16_t recv_ids_len = cfg->recv_num * SIZEOF(uint16_t);
    uint32_t data_len = SIZEOF(send_ids_len) + send_ids_len + SIZEOF(recv_ids_len) + recv_ids_len;
    char *data = OS_MALLOC(data_len);
    TUYA_CHECK_NULL_RETURN(data, OPRT_MALLOC_FAILED);
    memset(data, 0, data_len);

    uint32_t offset = 0, idx = 0;
    uint16_t id = 0;
    AI_PROTO_D("send_ids_len:%d, recv_ids_len:%d", send_ids_len, recv_ids_len);
    send_ids_len = UNI_HTONS(send_ids_len);
    memcpy(data + offset, &send_ids_len, SIZEOF(send_ids_len));
    offset += SIZEOF(send_ids_len);
    for (idx = 0; idx < cfg->send_num; idx++) {
        id = UNI_HTONS(cfg->send[idx].id);
        memcpy(data + offset, &id, SIZEOF(uint16_t));
        offset += SIZEOF(uint16_t);
    }
    recv_ids_len = UNI_HTONS(recv_ids_len);
    memcpy(data + offset, &recv_ids_len, SIZEOF(recv_ids_len));
    offset += SIZEOF(recv_ids_len);
    for (idx = 0; idx < cfg->recv_num; idx++) {
        id = UNI_HTONS(cfg->recv[idx].id);
        memcpy(data + offset, &id, SIZEOF(uint16_t));
        offset += SIZEOF(uint16_t);
    }

    rt = tuya_ai_basic_session_new(attr, data, data_len);
    if (OPRT_OK != rt) {
        PR_ERR("create session failed, rt:%d", rt);
    }
    OS_FREE(data);
    return rt;
}

OPERATE_RET tuya_ai_biz_crt_session(uint32_t bizCode, uint64_t bizTag, AI_SESSION_CFG_T *cfg, uint8_t *attr, uint32_t attr_len, char *token, AI_SESSION_ID id)
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

    AI_PROTO_D("create session id:%s,%d", id, strlen(id));
    AI_SESSION_NEW_ATTR_T session_attr = {
        .biz_code = bizCode,
        .biz_tag = bizTag,
        .token = token,
        .id = id,
        .user_data = attr,
        .user_len = attr_len
    };
    rt = __ai_pack_session_data(cfg, &session_attr);
    if (rt != OPRT_OK) {
        PR_ERR("pack session data failed, rt:%d", rt);
        return rt;
    }

    tal_mutex_lock(ai_basic_biz->mutex);
    uint32_t idx = 0;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_basic_biz->session[idx].id[0] == 0) {
            memcpy(&ai_basic_biz->session[idx].cfg, cfg, SIZEOF(AI_SESSION_CFG_T));
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
    return __ai_biz_session_destory(id, code, TRUE);
}

STATIC uint32_t s_ai_resv_send_id[AI_BIZ_MAX_NUM] = {101, 103, 105, 107, 109};
int tuya_ai_biz_get_send_id(VOID)
{
    STATIC int odd_number = 1;
    int id = odd_number;
    odd_number += 2;
    if (odd_number == s_ai_resv_send_id[0]) {
        odd_number += 10;
    }
    return id;
}

int tuya_ai_biz_get_recv_id(VOID)
{
    STATIC int even_number = 2;
    int id = even_number;
    even_number += 2;
    return id;
}

int tuya_ai_biz_get_reuse_send_id(AI_PACKET_PT type)
{
    if (type < AI_PT_VIDEO || type > AI_PT_TEXT) {
        PR_ERR("type error:%d", type);
        return 0xFFFF;
    }
    return s_ai_resv_send_id[type - AI_PT_VIDEO];
}

AI_SESSION_CFG_T* tuya_ai_biz_get_session_cfg(AI_SESSION_ID id)
{
    uint32_t idx = 0;
    AI_SESSION_CFG_T *cfg = NULL;
    if (ai_basic_biz) {
        tal_mutex_lock(ai_basic_biz->mutex);
        if (NULL == id) {
            cfg = &ai_basic_biz->session[0].cfg;
        } else {
            for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
                if (ai_basic_biz->session[idx].id[0] != 0) {
                    if (!strcmp(ai_basic_biz->session[idx].id, id)) {
                        cfg = &ai_basic_biz->session[idx].cfg;
                        break;
                    }
                }
            }
        }
        tal_mutex_unlock(ai_basic_biz->mutex);
    }
    return cfg;
}

OPERATE_RET tuya_ai_parse_video_attr(char *de_buf, uint32_t attr_len, AI_VIDEO_ATTR_T *video)
{
    return __ai_parse_video_attr(de_buf, attr_len, video);
}

OPERATE_RET tuya_ai_parse_audio_attr(char *de_buf, uint32_t attr_len, AI_AUDIO_ATTR_T *audio)
{
    return __ai_parse_audio_attr(de_buf, attr_len, audio);
}

OPERATE_RET tuya_ai_parse_image_attr(char *de_buf, uint32_t attr_len, AI_IMAGE_ATTR_T *image)
{
    return __ai_parse_image_attr(de_buf, attr_len, image);
}

OPERATE_RET tuya_ai_parse_file_attr(char *de_buf, uint32_t attr_len, AI_FILE_ATTR_T *file)
{
    return __ai_parse_file_attr(de_buf, attr_len, file);
}

OPERATE_RET tuya_ai_parse_text_attr(char *de_buf, uint32_t attr_len, AI_TEXT_ATTR_T *text)
{
    return __ai_parse_text_attr(de_buf, attr_len, text);
}

OPERATE_RET tuya_ai_parse_event_attr(char *de_buf, uint32_t attr_len, AI_EVENT_ATTR_T *event)
{
    return __ai_parse_event_attr(de_buf, attr_len, event);
}

OPERATE_RET tuya_ai_biz_monitor_register(AI_BIZ_MONITOR_CB recv_cb, AI_BIZ_MONITOR_CB send_cb, VOID *usr_data)
{
    ai_monitor.recv_cb = recv_cb;
    ai_monitor.send_cb = send_cb;
    ai_monitor.usr_data = usr_data;
    return OPRT_OK;
}
