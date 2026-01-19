/**
 * @file tuya_ai_http.c
 * @author tuya
 * @brief ai http
 * @version 0.1
 * @date 2025-05-18
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
#include <stdio.h>
#include "uni_log.h"
#include "http_inf.h"
#include "http_manager.h"
#include "mix_method.h"
#include "tal_workq_service.h"
#include "tuya_ai_private.h"
#include "tuya_ai_biz.h"
#include "tuya_ai_http.h"
#include "tuya_ai_agent.h"
#include "tuya_ai_input.h"

#ifndef AI_DL_IMAGE_UNIT_SIZE
#define AI_DL_IMAGE_UNIT_SIZE (6144)
#endif

typedef struct {
    char *url;
    AI_PACKET_PT type;
    AI_BIZ_RECV_CB cb;
} AI_HTTP_DLD_T;

STATIC OPERATE_RET __ai_http_output_media(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char *data, AI_BIZ_RECV_CB cb)
{
    OPERATE_RET rt = OPRT_OK;
    AI_PACKET_PT type = attr->type;
    AI_PROTO_D("output media type:%d, len:%d", type, head->len);
    if (cb) {
        rt = cb(attr, head, data, NULL);
    }
    return rt;
}

STATIC VOID __ai_http_dld_work(VOID *data)
{
    OPERATE_RET rt = OPRT_OK;
    SESSION_ID http_sesion = NULL;
    char *buf = NULL;
    AI_HTTP_DLD_T *dld = (AI_HTTP_DLD_T *)data;
    AI_PACKET_PT type = dld->type;
    char *url = dld->url;
    S_HTTP_MANAGER *http_manager = NULL;
    STATIC BOOL_T first_pkt = TRUE;
    AI_BIZ_HEAD_INFO_T head = {0};
    AI_BIZ_ATTR_INFO_T attr = {0};
    attr.type = type;
    attr.flag = AI_HAS_ATTR;
#if defined(AI_VERSION) && (0x02 == AI_VERSION)
    AI_ATTR_BASE_T *down_attr = tuya_ai_agent_get_down_attr();
#endif
    if (type == AI_PT_IMAGE) {
#if defined(AI_VERSION) && (0x02 == AI_VERSION)
        if (down_attr && (down_attr->image.type == IMAGE_PAYLOAD_TYPE_URL)) {
            attr.value.image.base.type = IMAGE_PAYLOAD_TYPE_RAW;
            attr.value.image.base.format = down_attr->image.format;
            attr.value.image.base.width = down_attr->image.width;
            attr.value.image.base.height = down_attr->image.height;
        } else {
            attr.value.image.base.format = IMAGE_FORMAT_JPEG;
            attr.value.image.base.width = 480;
            attr.value.image.base.height = 480;
        }
#else
        attr.value.image.base.format = IMAGE_FORMAT_JPEG;
        attr.value.image.base.width = 480;
        attr.value.image.base.height = 480;
#endif
    } else if (type == AI_PT_AUDIO) {
        attr.value.audio.base.codec_type = AUDIO_CODEC_MP3;
    } else if (type == AI_PT_VIDEO) {
        attr.value.video.base.codec_type = VIDEO_CODEC_H264;
        attr.value.video.base.fps = 30;
        attr.value.video.base.width = 480;
        attr.value.video.base.height = 480;
        attr.value.video.base.sample_rate = 90000;
    } else if (type == AI_PT_FILE) {
#if defined(AI_VERSION) && (0x02 == AI_VERSION)
        if (down_attr && (down_attr->file.type == FILE_PAYLOAD_TYPE_URL)) {
            attr.value.file.base.type = FILE_PAYLOAD_TYPE_RAW;
            attr.value.file.base.format = down_attr->file.format;
            memcpy(attr.value.file.base.file_name, down_attr->file.file_name, SIZEOF(down_attr->file.file_name));
        } else {
            attr.value.file.base.format = FILE_FORMAT_MP4;
        }
#else
        attr.value.file.base.format = FILE_FORMAT_MP4;
#endif
    }

    HTTP_URL_H_S *hu_h = create_http_url_h(0, 10);
    TUYA_CHECK_NULL_GOTO(hu_h, EXIT);
    fill_url_head(hu_h, url);

    http_manager = get_http_manager_instance();
    TUYA_CHECK_NULL_GOTO(http_manager, EXIT);

    http_sesion = http_manager->create_http_session(url, TRUE);
    TUYA_CHECK_NULL_GOTO(http_sesion, EXIT);

    http_req_t req = {
        .type = HTTP_GET,
        .resource = hu_h->buf,
        .version = HTTP_VER_1_1,
        .add_head_cb = NULL,
        .add_head_data = NULL
    };

    rt = http_manager->send_http_request(http_sesion, &req, 0);
    if (OPRT_OK != rt) {
        PR_ERR("http send request failed, rt:%d", rt);
        goto EXIT;
    }

    http_resp_t *resp = NULL;
    rt = http_manager->receive_http_response(http_sesion, &resp);
    if ((OPRT_OK != rt) || (!resp) || (resp->status_code != 200 && resp->status_code != 201)) {
        PR_ERR("put fail %d,code %d", rt, resp ? resp->status_code : 0xff);
        // PR_ERR("http max header size:%d,security level:%d", HTTP_MAX_REQ_RESP_HDR_SIZE, TUYA_SECURITY_LEVEL);
        goto EXIT;
    }

    if (0 == resp->content_length && !(resp->chunked)) {
        PR_ERR("http head err length %d, chunked %d", resp->content_length, resp->chunked);
        goto EXIT;
    }

    int read_len = 0, have_read_len = 0, sum_read_len = 0;
    uint32_t total_len = resp->content_length;
    uint32_t unit_len = AI_DL_IMAGE_UNIT_SIZE;
    buf = (char *)OS_MALLOC(unit_len);
    TUYA_CHECK_NULL_GOTO(buf, EXIT);
    memset(buf, 0, unit_len);

    while (1) {
        read_len = http_read_content(http_sesion->s, &buf[have_read_len], unit_len - have_read_len);
        if (read_len <= 0) {
            rt = OPRT_COM_ERROR;
            break;
        }

        have_read_len += read_len;
        AI_PROTO_D("sum_read_len:%d,have_read_len:%d,read_len:%d,total_len:%d", sum_read_len, have_read_len, read_len, total_len);
        if (sum_read_len + have_read_len >= total_len) {
            if (first_pkt) {
                first_pkt = TRUE;
                head.stream_flag = AI_STREAM_ONE;
                head.total_len = total_len;
                head.len = have_read_len;
                rt = __ai_http_output_media(&attr, &head, buf, dld->cb);
            } else {
                first_pkt = TRUE;
                head.stream_flag = AI_STREAM_END;
                head.total_len = total_len;
                head.len = have_read_len;
                rt = __ai_http_output_media(&attr, &head, buf, dld->cb);
            }
            if (OPRT_OK != rt) {
                PR_ERR("send to app err,rt:%d", rt);
            }
            break;
        } else {
            if (have_read_len < unit_len) {
                continue;
            }
        }
        if (first_pkt) {
            first_pkt = FALSE;
            head.stream_flag = AI_STREAM_START;
            head.total_len = total_len;
            head.len = unit_len;
            rt = __ai_http_output_media(&attr, &head, buf, dld->cb);
        } else {
            head.stream_flag = AI_STREAM_ING;
            head.total_len = total_len;
            head.len = unit_len;
            rt = __ai_http_output_media(&attr, &head, buf, dld->cb);
        }
        if (OPRT_OK != rt) {
            PR_ERR("send to app err,rt:%d", rt);
            break;
        }
        memset(buf, 0, unit_len);
        sum_read_len += have_read_len;
        have_read_len = 0;
    }

EXIT:
    if (dld) {
        if (dld->url) {
            OS_FREE(dld->url);
        }
        OS_FREE(dld);
    }
    if (buf) {
        OS_FREE(buf);
    }
    if (http_sesion) {
        http_manager->destory_http_session(http_sesion);
    }
    if (hu_h) {
        del_http_url_h(hu_h);
    }
    return;
}

STATIC OPERATE_RET __ai_http_dld_media(char *url, AI_PACKET_PT type, AI_BIZ_RECV_CB cb)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_CHECK_NULL_RETURN(url, OPRT_INVALID_PARM);
    AI_HTTP_DLD_T *dld = (AI_HTTP_DLD_T *)OS_MALLOC(SIZEOF(AI_HTTP_DLD_T));
    TUYA_CHECK_NULL_RETURN(dld, OPRT_MALLOC_FAILED);
    memset(dld, 0, SIZEOF(AI_HTTP_DLD_T));
    dld->type = type;
    dld->cb = cb;
    dld->url = mm_strdup(url);
    if (dld->url == NULL) {
        PR_ERR("malloc url failed");
        Free(dld);
        return OPRT_MALLOC_FAILED;
    }
    PR_DEBUG("do http dld work");
    rt = tal_workq_schedule(WORKQ_SYSTEM, __ai_http_dld_work, dld);
    if (OPRT_OK != rt) {
        PR_ERR("schedule workq err,rt:%d", rt);
        Free(dld->url);
        Free(dld);
    }
    return rt;
}

OPERATE_RET tuya_ai_http_dld_audio(char *url, AI_BIZ_RECV_CB cb)
{
    return __ai_http_dld_media(url, AI_PT_AUDIO, cb);
}

OPERATE_RET tuya_ai_http_dld_image(char *url, AI_BIZ_RECV_CB cb)
{
    return __ai_http_dld_media(url, AI_PT_IMAGE, cb);
}

OPERATE_RET tuya_ai_http_dld_video(char *url, AI_BIZ_RECV_CB cb)
{
    return __ai_http_dld_media(url, AI_PT_VIDEO, cb);
}

OPERATE_RET tuya_ai_http_dld_file(char *url, AI_BIZ_RECV_CB cb)
{
    return __ai_http_dld_media(url, AI_PT_FILE, cb);
}

OPERATE_RET tuya_ai_http_dld_text(char *url, AI_BIZ_RECV_CB cb)
{
    return __ai_http_dld_media(url, AI_PT_TEXT, cb);
}