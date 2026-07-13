/**
 * @file tuya_ai_agent.c
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
#include <stdio.h>
#include "tuya_ai_agent.h"
#include "uni_log.h"
#include "base_event.h"
#include "tuya_ai_biz.h"
#include "tuya_ai_http.h"
#include "tuya_ai_client.h"
#include "tuya_ai_event.h"
#include "tuya_ai_output.h"
#include "tuya_ai_input.h"
#include "tuya_svc_devos.h"
#include "tuya_ai_internal.h"
#include "tuya_list.h"
#include "http_inf.h"
#include "http_manager.h"
#include "tal_workq_service.h"
#include "tuya_ai_mqtt.h"
#include "tal_symmetry.h"
#include "smart_frame.h"
#include "tuya_ai_encoder.h"
#include "uni_base64.h"
#if defined(ENABLE_TUYA_CODEC_OPUS_IPC) && (ENABLE_TUYA_CODEC_OPUS_IPC == 1)
#include "tuya_ai_encoder_opus_ipc.h"
#elif defined(ENABLE_TUYA_CODEC_OPUS) && (ENABLE_TUYA_CODEC_OPUS == 1)
#include "tuya_ai_encoder_opus.h"
#endif
#if defined(ENABLE_TUYA_CODEC_SPEEX_IPC) && (ENABLE_TUYA_CODEC_SPEEX_IPC == 1)
#include "tuya_ai_encoder_speex_ipc.h"
#elif defined(ENABLE_TUYA_CODEC_SPEEX) && (ENABLE_TUYA_CODEC_SPEEX == 1)
#include "tuya_ai_encoder_speex.h"
#endif
#include "tuya_ai_protocol.h"

#define INTTERUPT_TIME_MAX 16

typedef struct {
    CHAR_T      *scode;
    AI_BIZ_HD_T *biz;
    UCHAR_T     *buf;
    UINT_T       buf_size;
    UINT_T       buf_len;
} AUDIO_ENCODE_CTX_T;
typedef struct {
    CHAR_T                 scode[AI_SOLUTION_CODE_LEN]; // global scode
    AI_ATTR_BASE_T         up_attr;
    AI_ATTR_BASE_T         down_attr;
    AI_INPUT_SEND_T        biz_get[AI_BIZ_MAX_NUM];
    AI_AGENT_SESSION_T     session[AI_SESSION_MAX_NUM];
    CHAR_T                 cfg_eid[AI_UUID_V4_LEN + 1]; // will auto clear after session create
    CHAR_T                *event_param;                 // will auto clear after event start
    CHAR_T                *session_param;               // will auto clear after session create
    BOOL_T                 codec_enable;
    TUYA_AI_ENCODER_T     *encoder;      // encoder handle
    TUYA_AI_ENCODER_INFO_T encoder_info; // encoder info
    AI_AGENT_TTS_CFG_T     tts_cfg;
    CHAR_T                 last_intr_time[INTTERUPT_TIME_MAX]; // last chat break time
    BOOL_T                 enable_crt_session_ext;             // enable crt session external
    BOOL_T                 enable_internal_scode;              // enable internal solution code
    BOOL_T                 enable_serv_vad;                    // enable server vad
    BOOL_T                 enable_proc_intr;                   // enable processing interrupt
    BOOL_T                 enable_mcp;                         // enable mcp tools
    BOOL_T                 enable_joyinside;                   // enable joyinside cloud
    TY_AI_MCP_CB           mcp_cb;
    VOID                  *mcp_user_data;
    AI_AGENT_RECV_DATA_T  *recv[AI_SESSION_MAX_NUM];
    AI_VIDEO_STREAM_E      video_stream;
} AI_AGENT_CTX_T;
STATIC AI_AGENT_CTX_T ai_agent_ctx;

STATIC OPERATE_RET __mcp_handle(CHAR_T *sid, CHAR_T *eid, CHAR_T *data);

STATIC OPERATE_RET __parse_attr_time(BYTE_T *data, UINT_T len, CHAR_T *time_str)
{
    OPERATE_RET rt = OPRT_OK;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    AI_ATTRIBUTE_T *attr     = NULL;
    UINT_T          attr_num = 0, idx;
    ty_cJSON       *root, *node;
    rt = tuya_parse_user_attrs((CHAR_T *)data, len, &attr, &attr_num);
    if (OPRT_OK != rt) {
        return rt;
    }
    rt = OPRT_NOT_SUPPORTED;
    for (idx = 0; idx < attr_num; idx++) {
        if (attr[idx].type == 1008) {
            root = ty_cJSON_Parse(attr[idx].value.str);
            node = ty_cJSON_GetObjectItem(root, "time");
            if (node && node->valuestring) {
                strncpy(time_str, node->valuestring, INTTERUPT_TIME_MAX);
                rt = OPRT_OK;
            }
            ty_cJSON_Delete(root);
        }
    }
    Free(attr);
#else
    // tal_log_hex_dump("__parse_attr_time", 64, data, len);
    ty_cJSON *root, *attr, *time;
    root = ty_cJSON_Parse((CHAR_T *)data);
    if (NULL == root) {
        rt = OPRT_COM_ERROR;
        return rt;
    }
    attr = ty_cJSON_GetObjectItem(root, "breakAttributes");
    if (NULL != attr) {
        time = ty_cJSON_GetObjectItem(attr, "time");
        if (time && time->valuestring) {
            strncpy(time_str, time->valuestring, INTTERUPT_TIME_MAX);
            rt = OPRT_OK;
        }
    } else {
        rt = OPRT_COM_ERROR;
    }
    ty_cJSON_Delete(root);
#endif
    return rt;
}

OPERATE_RET tuya_ai_agent_set_scode(CHAR_T *scode)
{
    if (scode) {
        memset(&ai_agent_ctx.scode, 0, SIZEOF(ai_agent_ctx.scode));
        strncpy(ai_agent_ctx.scode, scode, AI_SOLUTION_CODE_LEN);
    }
    return OPRT_OK;
}

CHAR_T *tuya_ai_agent_get_scode(CHAR_T *scode)
{
    return scode ? scode : ai_agent_ctx.scode;
}

STATIC OPERATE_RET __ai_event_cb(AI_EVENT_ATTR_T *event, AI_EVENT_HEAD_T *head, VOID *data)
{
    PR_DEBUG("[====ai event cb====] type:%d, sid:%s, eid:%s", head->type, event->session_id, event->event_id);
    if ((head->type == AI_EVENT_CHAT_BREAK) || (head->type == AI_EVENT_SERVER_VAD)) {
        if (head->type == AI_EVENT_CHAT_BREAK && event->user_data && event->user_len > 0) {
            /* filter chat break event */
            CHAR_T      intr_time[INTTERUPT_TIME_MAX] = {0};
            OPERATE_RET rt                            = __parse_attr_time(event->user_data, event->user_len, intr_time);
            if (OPRT_OK == rt) {
                if ((ai_agent_ctx.last_intr_time[0] != '\0') && (strcmp(intr_time, ai_agent_ctx.last_intr_time) <= 0)) {
                    TAL_PR_DEBUG("Interrupt event ignored, last:%s, cur:%s", ai_agent_ctx.last_intr_time, intr_time);
                    return OPRT_OK;
                }
                strncpy(ai_agent_ctx.last_intr_time, intr_time, INTTERUPT_TIME_MAX);
            }
        }
        tuya_ai_output_event(head->type, 0, event->event_id);
    } else if (head->type == AI_EVENT_MCP_CMD && data) {
        __mcp_handle(event->session_id, event->event_id, data);
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __ai_audio_recv_cb(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, VOID *data, VOID *usr_data)
{
    OPERATE_RET rt = OPRT_OK;

    switch (head->stream_flag) {
    case AI_STREAM_START:
        if (attr && attr->flag == AI_HAS_ATTR) {
            tuya_ai_output_attr(attr);
        }
        rt = tuya_ai_output_start();
        rt += tuya_ai_output_write(AI_PT_AUDIO, (UINT8_T *)data, head->len);
        break;
    case AI_STREAM_ING:
        rt = tuya_ai_output_write(AI_PT_AUDIO, (UINT8_T *)data, head->len);
        break;
    case AI_STREAM_END:
        if (head->len > 0) {
            rt = tuya_ai_output_write(AI_PT_AUDIO, (UINT8_T *)data, head->len);
        }
        rt += tuya_ai_output_stop(FALSE);
        break;
    default:
        break;
    }

    return rt;
}

AI_ATTR_BASE_T *tuya_ai_agent_get_down_attr(VOID)
{
    return &ai_agent_ctx.down_attr;
}

STATIC OPERATE_RET __ai_direct_output(AI_PACKET_PT type, AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, CHAR_T *data)
{
    OPERATE_RET rt = OPRT_OK;
    if (head->len == 0xFFFFFFFF) {
        PR_ERR("invalid head len");
        return OPRT_INVALID_PARM;
    }
    switch (head->stream_flag) {
    case AI_STREAM_START:
        if (attr && attr->flag == AI_HAS_ATTR) {
            tuya_ai_output_attr(attr);
        }
        tuya_ai_output_event(AI_EVENT_START, type, NULL);
        rt = tuya_ai_output_media(type, data, head->len, head->len);
        break;
    case AI_STREAM_ING:
        rt = tuya_ai_output_media(type, data, head->len, head->len);
        break;
    case AI_STREAM_END:
        if (head->len > 0) {
            rt = tuya_ai_output_media(type, data, head->len, head->len);
        }
        tuya_ai_output_event(AI_EVENT_END, type, NULL);
        break;
    case AI_STREAM_ONE:
        if (attr && attr->flag == AI_HAS_ATTR) {
            tuya_ai_output_attr(attr);
        }
        tuya_ai_output_event(AI_EVENT_START, type, NULL);
        rt = tuya_ai_output_media(type, data, head->len, head->len);
        tuya_ai_output_event(AI_EVENT_END, type, NULL);
        break;
    default:
        break;
    }

    return rt;
}

STATIC OPERATE_RET __ai_image_http_recv_cb(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, VOID *data, VOID *usr_data)
{
    return __ai_direct_output(AI_PT_IMAGE, attr, head, (CHAR_T *)data);
}

STATIC OPERATE_RET __ai_image_recv_cb(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, VOID *data, VOID *usr_data)
{
    OPERATE_RET rt            = OPRT_OK;
    CHAR_T     *decode_base64 = NULL;
    UINT_T      decode_len    = 0;
#if defined(AI_SUB_VERSION) && (0x02 == AI_SUB_VERSION)
    CHAR_T *dec_data = NULL;
#endif
#if defined(AI_VERSION) && (0x02 == AI_VERSION)
    AI_IMAGE_PAYLOAD_TYPE img_type;
    switch (head->stream_flag) {
    case AI_STREAM_START:
        memset(&ai_agent_ctx.down_attr.image, 0, SIZEOF(AI_IMAGE_ATTR_BASE_T));
        if (attr && attr->flag == AI_HAS_ATTR) {
            img_type = attr->value.image.base.type;
            PR_DEBUG("recv image type:%d, %d", img_type, head->len);
            if (img_type == IMAGE_PAYLOAD_TYPE_URL) {
                memcpy(&ai_agent_ctx.down_attr.image, &attr->value.image, SIZEOF(AI_IMAGE_ATTR_T));
                PR_NOTICE("download image from url:%d, %s", head->len, data);
                return tuya_ai_http_dld_image(data, __ai_image_http_recv_cb);
            } else if (img_type == IMAGE_PAYLOAD_TYPE_BASE64) {
                memcpy(&ai_agent_ctx.down_attr.image, &attr->value.image, SIZEOF(AI_IMAGE_ATTR_T));
                decode_base64 = Malloc(head->len);
                if (decode_base64 == NULL) {
                    PR_ERR("malloc decode_base64 failed");
                    return OPRT_MALLOC_FAILED;
                }
                memset(decode_base64, 0, head->len);
                decode_len = tuya_base64_decode(data, (UCHAR_T *)decode_base64);
            }
        }
        break;
    case AI_STREAM_ING:
        img_type = ai_agent_ctx.down_attr.image.type;
        if ((img_type == IMAGE_PAYLOAD_TYPE_BASE64) && (data != NULL) && (head->len > 0)) {
            decode_base64 = Malloc(head->len);
            if (decode_base64 == NULL) {
                PR_ERR("malloc decode_base64 failed");
                return OPRT_MALLOC_FAILED;
            }
            memset(decode_base64, 0, head->len);
            decode_len = tuya_base64_decode(data, (UCHAR_T *)decode_base64);
        }
        break;
    case AI_STREAM_END:
        img_type = ai_agent_ctx.down_attr.image.type;
        if ((img_type == IMAGE_PAYLOAD_TYPE_BASE64) && (data != NULL) && (head->len > 0)) {
            decode_base64 = Malloc(head->len);
            if (decode_base64 == NULL) {
                PR_ERR("malloc decode_base64 failed");
                return OPRT_MALLOC_FAILED;
            }
            memset(decode_base64, 0, head->len);
            decode_len = tuya_base64_decode(data, (UCHAR_T *)decode_base64);
        }
        break;
    case AI_STREAM_ONE:
        memset(&ai_agent_ctx.down_attr.image, 0, SIZEOF(AI_IMAGE_ATTR_BASE_T));
        if (attr && attr->flag == AI_HAS_ATTR) {
            img_type = attr->value.image.base.type;
            if (img_type == IMAGE_PAYLOAD_TYPE_URL) {
                memcpy(&ai_agent_ctx.down_attr.image, &attr->value.image, SIZEOF(AI_IMAGE_ATTR_T));
                PR_NOTICE("download image from url:%d, %s", head->len, data);
                return tuya_ai_http_dld_image(data, __ai_image_http_recv_cb);
            } else if (img_type == IMAGE_PAYLOAD_TYPE_BASE64) {
                memcpy(&ai_agent_ctx.down_attr.image, &attr->value.image, SIZEOF(AI_IMAGE_ATTR_T));
                decode_base64 = Malloc(head->len);
                if (decode_base64 == NULL) {
                    PR_ERR("malloc decode_base64 failed");
                    return OPRT_MALLOC_FAILED;
                }
                memset(decode_base64, 0, head->len);
                decode_len = tuya_base64_decode(data, (UCHAR_T *)decode_base64);
            }
        }
        break;
    }
#endif
    CHAR_T *eff_data;
    UINT_T  eff_len;
    if ((decode_len > 0) && decode_base64) {
        eff_data = decode_base64;
        eff_len  = decode_len;
    } else {
        eff_data = (CHAR_T *)data;
        eff_len  = head->len;
    }
#if defined(AI_SUB_VERSION) && (0x02 == AI_SUB_VERSION)
    if (eff_data && eff_len > AI_GCM_TAG_LEN) {
        UCHAR_T *key = tuya_ai_biz_get_key();
        if (key == NULL) {
            PR_ERR("image decrypt key is null");
            rt = OPRT_COM_ERROR;
            goto EXIT;
        }
        UINT_T   cipher_len = eff_len - AI_GCM_TAG_LEN;
        UCHAR_T *tag        = (UCHAR_T *)eff_data + cipher_len;
        dec_data            = Malloc(cipher_len);
        if (dec_data == NULL) {
            PR_ERR("malloc image decrypt buf failed");
            rt = OPRT_MALLOC_FAILED;
            goto EXIT;
        }
        UINT32_T gcm_len = 0;
        rt = tal_aes_gcm_decode(key, TUYA_AI_SECRET_KEY_LEN, head->value.image.iv, AI_IV_LEN,
                                NULL, 0, (UINT8_T *)eff_data, cipher_len,
                                (UINT8_T *)dec_data, &gcm_len,
                                tag, AI_GCM_TAG_LEN);
        if (rt != OPRT_OK) {
            PR_ERR("image gcm decode err:%d", rt);
            goto EXIT;
        }
        eff_data = dec_data;
        eff_len  = gcm_len;
        PR_DEBUG("image gcm decode success, len:%d", gcm_len);
    }
#endif
    head->len = eff_len;
    rt        = __ai_direct_output(AI_PT_IMAGE, attr, head, eff_data);
#if defined(AI_SUB_VERSION) && (0x02 == AI_SUB_VERSION)
EXIT:
    if (dec_data) {
        Free(dec_data);
    }
#endif
    if (decode_base64) {
        Free(decode_base64);
    }
    return rt;
}

STATIC OPERATE_RET __ai_video_recv_cb(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, VOID *data, VOID *usr_data)
{
#if defined(AI_SUB_VERSION) && (0x02 == AI_SUB_VERSION)
    if (data && head->len > AI_GCM_TAG_LEN && head->value.video.frame_type == AI_VIDEO_FRAME_TYPE_I) {
        UCHAR_T *key = tuya_ai_biz_get_key();
        if (key == NULL) {
            PR_ERR("video decrypt key is null");
            return OPRT_COM_ERROR;
        }
        UINT_T   cipher_len = head->len - AI_GCM_TAG_LEN;
        UCHAR_T *tag        = (UCHAR_T *)data + cipher_len;
        CHAR_T  *plaintext  = Malloc(cipher_len);
        if (plaintext == NULL) {
            PR_ERR("malloc video decrypt buf failed");
            return OPRT_MALLOC_FAILED;
        }
        UINT32_T dec_len = 0;
        OPERATE_RET rt = tal_aes_gcm_decode(key, TUYA_AI_SECRET_KEY_LEN, head->value.video.iv, AI_IV_LEN,
                                            NULL, 0, (UINT8_T *)data, cipher_len,
                                            (UINT8_T *)plaintext, &dec_len,
                                            tag, AI_GCM_TAG_LEN);
        if (rt != OPRT_OK) {
            PR_ERR("video gcm decode err:%d", rt);
            Free(plaintext);
            return rt;
        }
        PR_DEBUG("video gcm decode success, len:%d", dec_len);
        head->len = dec_len;
        rt        = __ai_direct_output(AI_PT_VIDEO, attr, head, plaintext);
        Free(plaintext);
        return rt;
    }
#endif
    return __ai_direct_output(AI_PT_VIDEO, attr, head, (CHAR_T *)data);
}

STATIC OPERATE_RET __ai_file_recv_cb(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, VOID *data, VOID *usr_data)
{
    OPERATE_RET rt            = OPRT_OK;
    CHAR_T     *decode_base64 = NULL;
    UINT_T      decode_len    = 0;
#if defined(AI_VERSION) && (0x02 == AI_VERSION)
    AI_FILE_PAYLOAD_TYPE file_type;
    switch (head->stream_flag) {
    case AI_STREAM_START:
        memset(&ai_agent_ctx.down_attr.file, 0, SIZEOF(AI_FILE_ATTR_BASE_T));
        if (attr && attr->flag == AI_HAS_ATTR) {
            file_type = attr->value.file.base.type;
            PR_DEBUG("recv file type:%d, %d, %s", file_type, head->len, attr->value.file.base.file_name);
            if (file_type == FILE_PAYLOAD_TYPE_URL) {
                memcpy(&ai_agent_ctx.down_attr.file, &attr->value.file, SIZEOF(AI_FILE_ATTR_T));
                PR_NOTICE("download file from url:%d, %s", head->len, data);
                return tuya_ai_http_dld_file(data, __ai_file_recv_cb);
            } else if (file_type == FILE_PAYLOAD_TYPE_BASE64) {
                memcpy(&ai_agent_ctx.down_attr.file, &attr->value.file, SIZEOF(AI_FILE_ATTR_T));
                decode_base64 = Malloc(head->len);
                if (decode_base64 == NULL) {
                    PR_ERR("malloc decode_base64 failed");
                    return OPRT_MALLOC_FAILED;
                }
                memset(decode_base64, 0, head->len);
                decode_len = tuya_base64_decode(data, (UCHAR_T *)decode_base64);
            }
        }
        break;
    case AI_STREAM_ING:
        file_type = ai_agent_ctx.down_attr.file.type;
        if ((file_type == FILE_PAYLOAD_TYPE_BASE64) && (data != NULL) && (head->len > 0)) {
            decode_base64 = Malloc(head->len);
            if (decode_base64 == NULL) {
                PR_ERR("malloc decode_base64 failed");
                return OPRT_MALLOC_FAILED;
            }
            memset(decode_base64, 0, head->len);
            decode_len = tuya_base64_decode(data, (UCHAR_T *)decode_base64);
        }
        break;
    case AI_STREAM_END:
        file_type = ai_agent_ctx.down_attr.file.type;
        if ((file_type == FILE_PAYLOAD_TYPE_BASE64) && (data != NULL) && (head->len > 0)) {
            decode_base64 = Malloc(head->len);
            if (decode_base64 == NULL) {
                PR_ERR("malloc decode_base64 failed");
                return OPRT_MALLOC_FAILED;
            }
            memset(decode_base64, 0, head->len);
            decode_len = tuya_base64_decode(data, (UCHAR_T *)decode_base64);
        }
        break;
    case AI_STREAM_ONE:
        memset(&ai_agent_ctx.down_attr.file, 0, SIZEOF(AI_FILE_ATTR_BASE_T));
        if (attr && attr->flag == AI_HAS_ATTR) {
            file_type = attr->value.file.base.type;
            if (file_type == FILE_PAYLOAD_TYPE_URL) {
                memcpy(&ai_agent_ctx.down_attr.file, &attr->value.file, SIZEOF(AI_FILE_ATTR_T));
                PR_NOTICE("download file from url:%d, %s", head->len, data);
                return tuya_ai_http_dld_file(data, __ai_file_recv_cb);
            } else if (file_type == FILE_PAYLOAD_TYPE_BASE64) {
                memcpy(&ai_agent_ctx.down_attr.file, &attr->value.file, SIZEOF(AI_FILE_ATTR_T));
                decode_base64 = Malloc(head->len);
                if (decode_base64 == NULL) {
                    PR_ERR("malloc decode_base64 failed");
                    return OPRT_MALLOC_FAILED;
                }
                memset(decode_base64, 0, head->len);
                decode_len = tuya_base64_decode(data, (UCHAR_T *)decode_base64);
            }
        }
        break;
    }
#endif
    if ((decode_len > 0) && decode_base64) {
        head->len = decode_len;
        rt        = __ai_direct_output(AI_PT_FILE, attr, head, decode_base64);
        Free(decode_base64);
    } else {
        rt = __ai_direct_output(AI_PT_FILE, attr, head, (CHAR_T *)data);
    }
    return rt;
}

STATIC OPERATE_RET __ai_parse_asr(ty_cJSON *root, BOOL_T eof)
{
    ty_cJSON *node = ty_cJSON_GetObjectItem(root, "text");
    PR_DEBUG("ASR text: %s", ty_cJSON_GetStringValue(node));
    if (eof && (!node || !node->valuestring || strlen(node->valuestring) == 0)) {
        tuya_ai_output_alert(AT_PLEASE_AGAIN);
    } else {
        tuya_ai_output_text(AI_TEXT_ASR, node, eof);
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __ai_parse_nlg(ty_cJSON *root, BOOL_T eof)
{
    OPERATE_RET rt = OPRT_OK;
    ty_cJSON   *node;
    PR_TRACE("NLG text: %s", ty_cJSON_GetStringValue(ty_cJSON_GetObjectItem(root, "content")));
    tuya_ai_output_text(AI_TEXT_NLG, root, eof);
    node = ty_cJSON_GetObjectItem(root, "images");
    if (node) {
        ty_cJSON *url = ty_cJSON_GetObjectItem(node, "url");
        if (url) {
            UINT_T image_num = 0, idx = 0;
            image_num = ty_cJSON_GetArraySize(url);
            for (idx = 0; idx < image_num; idx++) {
                ty_cJSON *url_item = ty_cJSON_GetArrayItem(url, idx);
                if ((url_item) && (strlen(url_item->valuestring) > 0)) {
                    rt = tuya_ai_http_dld_image(url_item->valuestring, __ai_image_recv_cb);
                }
            }
        }
    }
    return rt;
}

STATIC OPERATE_RET __ai_parse_dev_ctrl(ty_cJSON *root, ty_cJSON **dp_json)
{
    ty_cJSON *skill_general = ty_cJSON_GetObjectItem(root, "general");
    ty_cJSON *dps = NULL, *action = NULL;

    // only parse general skill with action "set"
    if (skill_general &&
        (dps = ty_cJSON_GetObjectItem(skill_general, "data")) &&
        (action = ty_cJSON_GetObjectItem(skill_general, "action")) &&
        action->valuestring && strcmp(action->valuestring, "set") == 0) {
        *dp_json = dps;
        return OPRT_OK;
    }

    return OPRT_NOT_SUPPORTED;
}

STATIC OPERATE_RET __ai_parse_speech_control(ty_cJSON *root, BOOL_T *exit)
{
    ty_cJSON *skill_general = ty_cJSON_GetObjectItem(root, "general");
    ty_cJSON *action        = NULL;

    if (skill_general &&
        (action = ty_cJSON_GetObjectItem(skill_general, "action"))) {
        if (action->valuestring && strcmp(action->valuestring, "exit") == 0) {
            *exit = TRUE;
            return OPRT_OK;
        }
    }

    *exit = FALSE;
    return OPRT_NOT_SUPPORTED;
}

STATIC OPERATE_RET __ai_parse_skill(ty_cJSON *root)
{
    OPERATE_RET rt   = OPRT_OK;
    ty_cJSON   *node = ty_cJSON_GetObjectItem(root, "code");
    CHAR_T     *code = ty_cJSON_GetStringValue(node);
    if (code && strcmp(code, "DeviceControl") == 0) {
        ty_cJSON *dp_json = NULL;
        if ((rt = __ai_parse_dev_ctrl(root, &dp_json)) == OPRT_OK && dp_json) {
            dp_json                = ty_cJSON_Duplicate(dp_json, TRUE);
            SF_GW_DEV_CMD_S gd_cmd = {DP_CMD_AI_SKILL, dp_json};
            rt                     = sf_send_gw_dev_cmd(&gd_cmd);
            if (rt != OPRT_OK) {
                ty_cJSON_Delete(dp_json);
                PR_ERR("send gw dev cmd failed: %d", rt);
            } else {
                PR_DEBUG("send gw dev cmd success");
            }
        } else {
            tuya_ai_output_text(AI_TEXT_SKILL, root, FALSE);
        }
    } else if (code && strcmp(code, "speechControl") == 0) {
        BOOL_T exit = 0;
        if ((rt = __ai_parse_speech_control(root, &exit)) == OPRT_OK) {
            if (exit) {
                tuya_ai_output_event(AI_EVENT_CHAT_EXIT, 0, NULL);
            }
        } else {
            tuya_ai_output_text(AI_TEXT_SKILL, root, FALSE);
        }
    } else {
        tuya_ai_output_text(AI_TEXT_SKILL, root, FALSE);
    }
    return rt;
}
typedef struct {
    CONST CHAR_T *request_id;
    CONST CHAR_T *content;
} AI_CLOUD_TRIGGER_T;

STATIC VOID __cloud_trigger_wq(VOID *data)
{
    AI_CLOUD_TRIGGER_T *trigger = (AI_CLOUD_TRIGGER_T *)data;
    if (trigger == NULL) {
        return;
    }
    CONST CHAR_T *request_id = trigger->request_id;
    CONST CHAR_T *content    = trigger->content;
    if (request_id && content && tuya_ai_agent_is_ready()) {
        tuya_ai_input_mute_audio(TRUE);
        tuya_ai_agent_event(AI_EVENT_CHAT_BREAK, 0);
        tuya_ai_output_event(AI_EVENT_CHAT_BREAK, AI_PT_AUDIO, 0);
        tuya_ai_input_stop();
        tuya_ai_output_stop(TRUE);
        tuya_ai_agent_set_scode(NULL);
        tuya_ai_agent_set_eid((CHAR_T *)request_id);
        tuya_ai_input_start(FALSE);
        tuya_ai_text_input((BYTE_T *)content, strlen(content), strlen(content));
        tuya_ai_input_stop();
        tuya_ai_input_mute_audio(FALSE);
    }

    if (trigger->request_id) {
        tal_free((CHAR_T *)trigger->request_id);
    }
    if (trigger->content) {
        tal_free((CHAR_T *)trigger->content);
    }
    tal_free(trigger);
}

STATIC OPERATE_RET __ai_agent_cloud_trigger(CONST CHAR_T *request_id, CONST CHAR_T *content)
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
    trigger->content    = mm_strdup(content);
    if (trigger->request_id == NULL || trigger->content == NULL) {
        rt = OPRT_MALLOC_FAILED;
        goto err;
    }
    TUYA_CALL_ERR_GOTO(tal_workq_schedule(WORKQ_SYSTEM, __cloud_trigger_wq, trigger), err);
    return rt;
err:
    if (trigger && trigger->request_id) {
        tal_free((CHAR_T *)trigger->request_id);
    }
    if (trigger && trigger->content) {
        tal_free((CHAR_T *)trigger->content);
    }
    if (trigger) {
        tal_free(trigger);
    }
    return rt;
}

STATIC OPERATE_RET __ai_parse_cloud_event(ty_cJSON *root)
{
    OPERATE_RET rt     = OPRT_OK;
    ty_cJSON   *node   = ty_cJSON_GetObjectItem(root, "action");
    CHAR_T     *action = ty_cJSON_GetStringValue(node);

    if (action && strcmp(action, "TriggerAiChat") == 0) {
        ty_cJSON *data       = ty_cJSON_GetObjectItem(root, "data");
        CHAR_T   *content    = ty_cJSON_GetStringValue(ty_cJSON_GetObjectItem(data, "content"));
        CHAR_T   *request_id = ty_cJSON_GetStringValue(ty_cJSON_GetObjectItem(data, "requestId"));
        if (!content || !request_id) {
            PR_ERR("content or request_id is NULL");
            return OPRT_INVALID_PARM;
        }
        rt = __ai_agent_cloud_trigger(request_id, content);
        if (OPRT_OK != rt) {
            PR_ERR("cloud trigger failed: %d", rt);
            return OPRT_COM_ERROR;
        }
        return OPRT_OK;
    } else {
        tuya_ai_output_text(AI_TEXT_CLOUD_EVENT, root, FALSE);
    }

    return rt;
}

STATIC OPERATE_RET __ai_text_recv_cb(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, VOID *data, VOID *usr_data)
{
    if (!data || !head) {
        PR_ERR("invalid param %p %p", data, head);
        return OPRT_OK;
    }
    OPERATE_RET rt  = OPRT_OK;
    BOOL_T      eof = FALSE;

    ty_cJSON *root = NULL;
    if (head->data_type == AI_BIZ_DATA_TYPE_BYTE) {
        if ((0 == head->len) || (!data)) {
            PR_ERR("invalid text data %p %d", data, head->len);
            return OPRT_OK;
        }
        root = ty_cJSON_ParseWithLengthOpts((CHAR_T *)data, head->len, NULL, 0);
    } else if (head->data_type == AI_BIZ_DATA_TYPE_JSON) {
        root = (ty_cJSON *)data;
    } else {
        PR_ERR("data type err %d", head->data_type);
        return OPRT_INVALID_PARM;
    }
    if (root == NULL) {
        PR_ERR("cjson parse failed");
        return OPRT_OK;
    }

    ty_cJSON *json_bizType = ty_cJSON_GetObjectItem(root, "bizType");
    CHAR_T   *bizType      = ty_cJSON_GetStringValue(json_bizType);

    ty_cJSON *json_eof = ty_cJSON_GetObjectItem(root, "eof");
    if (json_eof) {
        eof = ty_cJSON_GetNumberValue(json_eof);
    }
    ty_cJSON *json_data = ty_cJSON_GetObjectItem(root, "data");

    if (eof && bizType && strcmp(bizType, "ASR") == 0) {
        __ai_parse_asr(json_data, eof);
    } else if (bizType && strcmp(bizType, "NLG") == 0) {
        __ai_parse_nlg(json_data, eof);
    } else if (bizType && strcmp(bizType, "SKILL") == 0) {
        __ai_parse_skill(json_data);
    } else if (bizType && strcmp(bizType, "CloudEvent") == 0) {
        __ai_parse_cloud_event(json_data);
    } else {
        tuya_ai_output_text(AI_TEXT_OTHER, root, eof);
    }
    if (head->data_type == AI_BIZ_DATA_TYPE_BYTE) {
        ty_cJSON_Delete(root);
    }
    return rt;
}

STATIC AI_INPUT_SEND_T *__ai_agent_biz_get(AI_PACKET_PT type)
{
    USHORT_T idx = 0;
    for (idx = 0; idx < AI_BIZ_MAX_NUM; idx++) {
        if (ai_agent_ctx.biz_get[idx].type == type) {
            return &ai_agent_ctx.biz_get[idx];
        }
    }
    return NULL;
}

STATIC BOOL_T __ai_agent_is_session_crt(CHAR_T *scode)
{
    UINT_T idx = 0;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_agent_ctx.session[idx].sid[0] != '\0' &&
            ((!scode && ai_agent_ctx.session[idx].scode[0] == 0) ||
             (scode && strcmp(ai_agent_ctx.session[idx].scode, scode) == 0))) {
            PR_DEBUG("use session %s", ai_agent_ctx.session[idx].sid);
            return TRUE;
        }
    }
    return FALSE;
}

// find a free session slot
STATIC UINT_T __ai_agent_find_free_sid(VOID)
{
    UINT_T idx;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_agent_ctx.session[idx].sid[0] == '\0') {
            return idx;
        }
    }
    return AI_SESSION_MAX_NUM;
}

STATIC VOID __ai_agent_set_sid(AI_AGENT_SESSION_T *session, CHAR_T *scode, AI_SESSION_ID id, AI_SESSION_CFG_T *cfg, CHAR_T *token)
{
    if (!scode) {
        return;
    }

    UINT_T jdx = 0;
    memset(session->scode, 0, SIZEOF(session->scode));
    strncpy(session->scode, scode, SIZEOF(session->scode) - 1);
    memcpy(session->sid, id, strlen(id));
    memcpy(session->token, token, strlen(token));
    for (jdx = 0; jdx < cfg->send_num; jdx++) {
        session->send[jdx].id        = cfg->send[jdx].id;
        session->send[jdx].type      = cfg->send[jdx].type;
        session->send[jdx].first_pkt = FALSE;
    }

    return;
}

STATIC VOID __ai_agent_del_sid(AI_SESSION_ID id)
{
    UINT_T idx = 0;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_agent_ctx.session[idx].sid[0] != '\0' &&
            !strcmp(ai_agent_ctx.session[idx].sid, id)) {
            memset(&ai_agent_ctx.session[idx], 0, SIZEOF(AI_AGENT_SESSION_T));
            PR_DEBUG("del session idx:%d", idx);
            break;
        }
    }
    if (idx >= AI_SESSION_MAX_NUM) {
        PR_ERR("ai agent session not found");
        return;
    }
    return;
}

AI_AGENT_SESSION_T *tuya_ai_agent_get_session(CHAR_T *scode)
{
    USHORT_T idx = 0;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_agent_ctx.session[idx].sid[0] != '\0' &&
            ((!scode && ai_agent_ctx.session[idx].scode[0] == 0) ||
             (scode && strcmp(ai_agent_ctx.session[idx].scode, scode) == 0))) {
            return &ai_agent_ctx.session[idx];
        }
    }
    PR_TRACE("no session found for scode:%s", scode);
    return NULL;
}

VOID __ai_agent_free_recv_cb(VOID)
{
    UINT_T idx;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_agent_ctx.recv[idx]) {
            Free(ai_agent_ctx.recv[idx]);
            ai_agent_ctx.recv[idx] = NULL;
        }
    }
}

OPERATE_RET tuya_ai_agent_set_recv_cb(AI_AGENT_RECV_DATA_T *recv)
{
    UINT_T idx = 0;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_agent_ctx.recv[idx] == NULL) {
            ai_agent_ctx.recv[idx] = Malloc(SIZEOF(AI_AGENT_RECV_DATA_T));
            if (ai_agent_ctx.recv[idx] == NULL) {
                PR_ERR("malloc recv data failed");
                return OPRT_MALLOC_FAILED;
            }
            memset(ai_agent_ctx.recv[idx], 0, SIZEOF(AI_AGENT_RECV_DATA_T));
            memcpy(ai_agent_ctx.recv[idx], recv, SIZEOF(AI_AGENT_RECV_DATA_T));
            PR_DEBUG("set recv cb idx:%d, scode:%s", idx, recv->scode);
            return OPRT_OK;
        }
    }
    PR_ERR("ai agent recv cb full");
    return OPRT_COM_ERROR;
}

AI_BIZ_RECV_CB tuya_ai_agent_get_recv_cb(CHAR_T *scode, AI_PACKET_PT type)
{
    UINT_T idx = 0, jdx = 0;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_agent_ctx.recv[idx] &&
            ((!scode && ai_agent_ctx.recv[idx]->scode[0] == 0) ||
                (scode && strcmp(ai_agent_ctx.recv[idx]->scode, scode) == 0))) {
            for (jdx = 0; jdx < AI_BIZ_MAX_NUM; jdx++) {
                if (ai_agent_ctx.recv[idx]->arc[jdx].type == type) {
                    PR_DEBUG("get recv cb idx:%d, scode:%s, type:%d", idx, ai_agent_ctx.recv[idx]->scode, type);
                    return ai_agent_ctx.recv[idx]->arc[jdx].cb;
                }
            }
        }
    }

    if (type == AI_PT_TEXT) {
        return __ai_text_recv_cb;
    } else if (type == AI_PT_AUDIO) {
        return __ai_audio_recv_cb;
    } else if (type == AI_PT_IMAGE) {
        return __ai_image_recv_cb;
    } else if (type == AI_PT_VIDEO) {
        return __ai_video_recv_cb;
    } else if (type == AI_PT_FILE) {
        return __ai_file_recv_cb;
    }
    return NULL;
}

AI_EVENT_CB tuya_ai_agent_get_evt_cb(CHAR_T *scode)
{
    UINT_T idx = 0;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_agent_ctx.recv[idx] &&
            ((!scode && ai_agent_ctx.recv[idx]->scode[0] == 0) ||
                (scode && strcmp(ai_agent_ctx.recv[idx]->scode, scode) == 0))) {
            return ai_agent_ctx.recv[idx]->event_cb;
        }
    }
    return __ai_event_cb;
}

OPERATE_RET tuya_ai_agent_crt_session(CHAR_T *scode, UINT_T bizCode, UINT64_T bizTag, BYTE_T *attr, UINT_T attr_len)
{
    OPERATE_RET      rt         = OPRT_OK;
    AI_INPUT_SEND_T *input_send = NULL;
    UINT_T           idx        = 0;
    CHAR_T          *token      = NULL;
    if (ai_agent_ctx.enable_joyinside) {
        return OPRT_OK;
    }
    if (__ai_agent_is_session_crt(scode)) {
        return OPRT_OK;
    }
    PR_DEBUG("ai session new");

    idx = __ai_agent_find_free_sid();
    if (idx >= AI_SESSION_MAX_NUM) {
        PR_ERR("ai agent session full %d, %d", idx, AI_SESSION_MAX_NUM);
        return OPRT_COM_ERROR;
    }
    AI_AGENT_SESSION_T *session = &ai_agent_ctx.session[idx];
    memset(session, 0, SIZEOF(AI_AGENT_SESSION_T));
    strncpy(session->scode, scode, SIZEOF(session->scode) - 1);

    AI_SESSION_CFG_T session_cfg = {0};
    memset(&session_cfg, 0, SIZEOF(AI_SESSION_CFG_T));

    AI_AGENT_TOKEN_INFO_T agent = {0};
    if (!ai_agent_ctx.enable_internal_scode) {
        rt = tuya_ai_mq_token_req(scode, &agent);
        if (rt != OPRT_OK) {
            if (agent.tts_url[0] != 0) {
                PR_ERR("get agent token failed, tts url:%s", agent.tts_url);
                tuya_ai_http_dld_audio(agent.tts_url, __ai_audio_recv_cb);
            }
            return rt;
        }
        token   = agent.token;
        bizCode = agent.biz.code;
    } else {
        agent.biz.send_num = AI_BIZ_MAX_NUM;
        agent.biz.recv_num = AI_BIZ_MAX_NUM;
        for (idx = 0; idx < AI_BIZ_MAX_NUM; idx++) {
            agent.biz.send[idx] = AI_PT_VIDEO + idx;
            agent.biz.recv[idx] = AI_PT_VIDEO + idx;
        }
    }

    session_cfg.send_num = agent.biz.send_num;
    for (idx = 0; idx < agent.biz.send_num; idx++) {
        session_cfg.send[idx].type = agent.biz.send[idx];
        input_send                 = __ai_agent_biz_get(agent.biz.send[idx]);
        if (input_send && input_send->multi_session) {
            session_cfg.send[idx].id = tuya_ai_biz_get_reuse_send_id(agent.biz.send[idx]);
            if (agent.biz.send[idx] == AI_PT_VIDEO && ai_agent_ctx.video_stream != AI_VIDEO_STREAM_MAIN) {
                session_cfg.send[idx].id += (AI_PT_TEXT - AI_PT_VIDEO + ai_agent_ctx.video_stream) * 2;
            }
        } else {
            session_cfg.send[idx].id = tuya_ai_biz_get_send_id();
        }
        session_cfg.send[idx].get_cb  = input_send ? input_send->get_cb : NULL;
        session_cfg.send[idx].free_cb = input_send ? input_send->free_cb : NULL;
    }

    session_cfg.recv_num = agent.biz.recv_num;
    for (idx = 0; idx < agent.biz.recv_num; idx++) {
        session_cfg.recv[idx].type     = agent.biz.recv[idx];
        session_cfg.recv[idx].id       = tuya_ai_biz_get_recv_id();
        session_cfg.recv[idx].usr_data = session->scode;
        session_cfg.recv[idx].cb       = tuya_ai_agent_get_recv_cb(scode, agent.biz.recv[idx]);
    }
    session_cfg.event_cb = tuya_ai_agent_get_evt_cb(scode);

    BYTE_T *out     = NULL;
    UINT_T  out_len = 0;
    if (attr && attr_len > 0) {
        out     = attr;
        out_len = attr_len;
    } else {
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        // pack tts attributes: {"tts.order.supports":[{"format":"mp3","container":"","sampleRate":16000,"bitDepth":"16","channels":1}]}
        ty_cJSON *root = ty_cJSON_CreateObject();
        ty_cJSON *tts = ty_cJSON_AddArrayToObject(root, "tts.order.supports");
        ty_cJSON *item = ty_cJSON_CreateObject();
        ty_cJSON_AddStringToObject(item, "format", ai_agent_ctx.tts_cfg.format ? ai_agent_ctx.tts_cfg.format : "mp3");
        ty_cJSON_AddStringToObject(item, "container", "");
        ty_cJSON_AddNumberToObject(item, "sampleRate", ai_agent_ctx.tts_cfg.sample_rate ? ai_agent_ctx.tts_cfg.sample_rate : 16000);
        ty_cJSON_AddNumberToObject(item, "bitDepth", 16);
        ty_cJSON_AddNumberToObject(item, "channels", 1);
        if (ai_agent_ctx.tts_cfg.format && strcmp(ai_agent_ctx.tts_cfg.format, "opus") == 0) {
            ty_cJSON_AddNumberToObject(item, "bitRate", ai_agent_ctx.tts_cfg.bit_rate ? ai_agent_ctx.tts_cfg.bit_rate : 16000);
        } else {
            ty_cJSON_AddNumberToObject(item, "bitRate", ai_agent_ctx.tts_cfg.bit_rate ? ai_agent_ctx.tts_cfg.bit_rate : 64000);
        }
        ty_cJSON_AddItemToArray(tts, item);
        // pack mcp tools attributes: {"supportCustomMCP":true}
        if (ai_agent_ctx.enable_mcp) {
            item = ty_cJSON_CreateObject();
            ty_cJSON_AddBoolToObject(item, "supportCustomMCP", ai_agent_ctx.enable_mcp);
            ty_cJSON_AddItemToObject(root, "deviceMcp", item);
        }

        CHAR_T *session_attrs = ty_cJSON_PrintUnformatted(root);
        PR_DEBUG("session attrs: %s", session_attrs);

        AI_ATTRIBUTE_T def_attr[2] = {{
                .type = 1003,
                .payload_type = ATTR_PT_U8,
                .length = 1,
                .value.u8 = 2
            }, {
                .type = 1004,       /* sessionAttributes */
                .payload_type = ATTR_PT_STR,
                .length = strlen(session_attrs),
                .value.str = session_attrs,
            }
        };
        tuya_pack_user_attrs(def_attr, CNTSOF(def_attr), &out, &out_len);
        Free(session_attrs);
        ty_cJSON_Delete(root);
#else
        ty_cJSON *attr_info         = ty_cJSON_CreateObject();
        ty_cJSON *sessionAttributes = ty_cJSON_CreateObject();
        ty_cJSON_AddItemToObject(attr_info, "sessionAttributes", sessionAttributes);

        ty_cJSON *tts_order_supports = cJSON_CreateArray();
        ty_cJSON_AddItemToObject(sessionAttributes, "tts.order.supports", tts_order_supports);

        cJSON *supportItem = cJSON_CreateObject();
        cJSON_AddItemToArray(tts_order_supports, supportItem);
        // container
        ty_cJSON_AddStringToObject(supportItem, "container", "");
        // channels
        ty_cJSON_AddNumberToObject(supportItem, "channels", 1);
        // bitDepth
        ty_cJSON_AddStringToObject(supportItem, "bitDepth", "16");
        // bitRate
        if (ai_agent_ctx.tts_cfg.format && strcmp(ai_agent_ctx.tts_cfg.format, "opus") == 0) {
            ty_cJSON_AddNumberToObject(supportItem, "bitRate", ai_agent_ctx.tts_cfg.bit_rate == 0 ? 16000 : (ai_agent_ctx.tts_cfg.bit_rate));
        } else {
            ty_cJSON_AddNumberToObject(supportItem, "bitRate", ai_agent_ctx.tts_cfg.bit_rate == 0 ? 64000 : (ai_agent_ctx.tts_cfg.bit_rate));
        }
        //format
        ty_cJSON_AddStringToObject(supportItem, "format", (ai_agent_ctx.tts_cfg.format ? ai_agent_ctx.tts_cfg.format : "mp3"));
        //sampleRate
        ty_cJSON_AddNumberToObject(supportItem, "sampleRate", ai_agent_ctx.tts_cfg.sample_rate == 0 ? 16000 : ai_agent_ctx.tts_cfg.sample_rate);

        // pack mcp tools attributes: {"supportCustomMCP":true}
        if (ai_agent_ctx.enable_mcp) {
            supportItem = ty_cJSON_CreateObject();
            ty_cJSON_AddBoolToObject(supportItem, "supportCustomMCP", ai_agent_ctx.enable_mcp);
            ty_cJSON_AddItemToObject(sessionAttributes, "deviceMcp", supportItem);
        }
        if (ai_agent_ctx.session_param) {
            ty_cJSON *custom = ty_cJSON_Parse(ai_agent_ctx.session_param);
            if (custom) {
                ty_cJSON_AddItemToObject(sessionAttributes, "custom.param", custom);
            }
            Free(ai_agent_ctx.session_param);
            ai_agent_ctx.session_param = NULL;
        }
        out     = (UCHAR_T *)ty_cJSON_PrintUnformatted(attr_info);
        out_len = strlen((CHAR_T *)out);

        ty_cJSON_Delete(attr_info);
        AI_PROTO_D("user data out: %s ", out);
#endif
    }

    CHAR_T sid[AI_UUID_V4_LEN + 1] = {0};
    rt                             = tuya_ai_biz_crt_session(bizCode, bizTag, &session_cfg, out, out_len, token, sid);
    if (rt != OPRT_OK) {
        PR_ERR("create session failed");
        tuya_ai_output_alert(AT_NETWORK_FAIL);
    } else {
        PR_DEBUG("create session:%s success, scode:%s", sid, scode);
        __ai_agent_set_sid(session, scode, sid, &session_cfg, agent.token);
    }
    if (attr == NULL) {
        Free(out);
    }
    return rt;
}

OPERATE_RET tuya_ai_agent_del_session(CHAR_T *scode)
{
    if (ai_agent_ctx.enable_joyinside) {
        return OPRT_OK;
    }
    UINT_T idx                     = 0;
    CHAR_T sid[AI_UUID_V4_LEN + 1] = {0};

    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_agent_ctx.session[idx].sid[0] != '\0' &&
            ((!scode && ai_agent_ctx.session[idx].scode[0] == 0) ||
             (scode && strcmp(ai_agent_ctx.session[idx].scode, scode) == 0))) {
            PR_DEBUG("del session idx:%d", idx);
            strncpy(sid, ai_agent_ctx.session[idx].sid, AI_UUID_V4_LEN);
            memset(&ai_agent_ctx.session[idx], 0, SIZEOF(AI_AGENT_SESSION_T));
            break;
        }
    }
    if (idx >= AI_SESSION_MAX_NUM) {
        PR_ERR("session not found, scode:%s", scode ? scode : "null");
        return OPRT_COM_ERROR;
    }
    return tuya_ai_biz_del_session(sid, AI_CODE_OK);
}

STATIC OPERATE_RET __ai_session_closed_evt(VOID_T *data)
{
    OPERATE_RET rt = OPRT_OK;
    PR_DEBUG("ai session closed");
    AI_SESSION_ID sid = (AI_SESSION_ID)data;
    __ai_agent_del_sid(sid);
    return rt;
}

STATIC OPERATE_RET __ai_agent_mq_handle(ty_cJSON *root)
{
    AI_BIZ_HEAD_INFO_T biz_head = {0};
    biz_head.data_type          = AI_BIZ_DATA_TYPE_JSON;
    return __ai_text_recv_cb(NULL, &biz_head, root, NULL);
}

STATIC OPERATE_RET __ai_client_run_evt(VOID_T *data)
{
    return tuya_ai_output_alert(AT_NETWORK_CONNECTED);
}

STATIC OPERATE_RET __ai_devos_state_evt(VOID *data)
{
    DEVOS_STATE_E state = (DEVOS_STATE_E)data;
    if (state == DEVOS_STATE_UNREGISTERED) {
        tuya_ai_output_alert(AT_NETWORK_CFG);
    }
    return OPRT_OK;
}

STATIC VOID __ai_agent_destroy_encoder(VOID)
{
    if (ai_agent_ctx.encoder && ai_agent_ctx.encoder->handle) {
        ai_agent_ctx.encoder->destroy(ai_agent_ctx.encoder->handle);
        ai_agent_ctx.encoder->handle = NULL;
    }
    return;
}

STATIC VOID __ai_agent_set_audio_encoder(AI_AUDIO_ATTR_BASE_T *attr)
{
    OPERATE_RET rt = OPRT_OK;
    if (ai_agent_ctx.codec_enable) {
        ai_agent_ctx.encoder                      = tuya_ai_get_encoder(attr->codec_type);
        ai_agent_ctx.encoder_info.encode_type     = attr->codec_type;
        ai_agent_ctx.encoder_info.sample_rate     = attr->sample_rate;
        ai_agent_ctx.encoder_info.channels        = attr->channels;
        ai_agent_ctx.encoder_info.bits_per_sample = attr->bit_depth;
        ai_agent_ctx.encoder_info.bitrate         = attr->bitrate;
        ai_agent_ctx.encoder_info.bandwidth       = attr->bandwidth;
        ai_agent_ctx.encoder_info.vbr             = attr->vbr;
        ai_agent_ctx.encoder_info.dtx             = attr->dtx;
        ai_agent_ctx.encoder_info.complexity      = attr->complexity;
        if (ai_agent_ctx.encoder) {
            __ai_agent_destroy_encoder();
            rt = ai_agent_ctx.encoder->create(&ai_agent_ctx.encoder->handle, &ai_agent_ctx.encoder_info);
            if (rt != OPRT_OK) {
                PR_ERR("create audio encoder failed");
                ai_agent_ctx.encoder = NULL;
            } else {
                PR_DEBUG("create audio encoder success %p", ai_agent_ctx.encoder->handle);
            }
        }
    }
}

STATIC VOID __ai_agent_reg_encoder(VOID)
{
#if defined(ENABLE_TUYA_CODEC_OPUS_IPC) && (ENABLE_TUYA_CODEC_OPUS_IPC == 1)
    tuya_ai_register_encoder(&g_tuya_ai_encoder_opus_ipc);
#elif defined(ENABLE_TUYA_CODEC_OPUS) && (ENABLE_TUYA_CODEC_OPUS == 1)
    tuya_ai_register_encoder(&g_tuya_ai_encoder_opus);
#endif
#if defined(ENABLE_TUYA_CODEC_SPEEX_IPC) && (ENABLE_TUYA_CODEC_SPEEX_IPC == 1)
    tuya_ai_register_encoder(&g_tuya_ai_encoder_speex_ipc);
#elif defined(ENABLE_TUYA_CODEC_SPEEX) && (ENABLE_TUYA_CODEC_SPEEX == 1)
    tuya_ai_register_encoder(&g_tuya_ai_encoder_speex);
#endif
}

STATIC VOID __ai_agent_unreg_encoder(VOID)
{
#if defined(ENABLE_TUYA_CODEC_OPUS_IPC) && (ENABLE_TUYA_CODEC_OPUS_IPC == 1)
    tuya_ai_unregister_encoder(&g_tuya_ai_encoder_opus_ipc);
#elif defined(ENABLE_TUYA_CODEC_OPUS) && (ENABLE_TUYA_CODEC_OPUS == 1)
    tuya_ai_unregister_encoder(&g_tuya_ai_encoder_opus);
#endif
#if defined(ENABLE_TUYA_CODEC_SPEEX_IPC) && (ENABLE_TUYA_CODEC_SPEEX_IPC == 1)
    tuya_ai_unregister_encoder(&g_tuya_ai_encoder_speex_ipc);
#elif defined(ENABLE_TUYA_CODEC_SPEEX) && (ENABLE_TUYA_CODEC_SPEEX == 1)
    tuya_ai_unregister_encoder(&g_tuya_ai_encoder_speex);
#endif
}

VOID tuya_ai_agent_send_cb_set(AI_INPUT_SEND_T *in_send)
{
    if (NULL == in_send) {
        PR_ERR("input is null");
        return;
    }
    memcpy(ai_agent_ctx.biz_get, in_send, AI_BIZ_MAX_NUM * SIZEOF(AI_INPUT_SEND_T));
}

OPERATE_RET tuya_ai_agent_init(AI_AGENT_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_CHECK_NULL_RETURN(cfg, OPRT_INVALID_PARM);

#if defined(ENABLE_JOYINSIDE) && (ENABLE_JOYINSIDE == 1)
    if (cfg->jd_cfg) {
        ai_agent_ctx.enable_joyinside = TRUE;
        TUYA_CALL_ERR_GOTO(joyinside_client_init(cfg->jd_cfg), EXIT);
    } else {
#endif
        strncpy(ai_agent_ctx.scode, cfg->scode, SIZEOF(ai_agent_ctx.scode) - 1);
        memcpy(&ai_agent_ctx.up_attr, &cfg->attr, SIZEOF(ai_agent_ctx.up_attr));
        memcpy(ai_agent_ctx.biz_get, cfg->biz_get, AI_BIZ_MAX_NUM * SIZEOF(AI_INPUT_SEND_T));
        memcpy(&ai_agent_ctx.tts_cfg, &cfg->tts_cfg, SIZEOF(ai_agent_ctx.tts_cfg));
        ai_agent_ctx.enable_crt_session_ext = cfg->enable_crt_session_ext;
        ai_agent_ctx.enable_internal_scode  = cfg->enable_internal_scode;
        ai_agent_ctx.enable_serv_vad        = TRUE;
        ai_agent_ctx.enable_proc_intr       = TRUE;
#if defined(AI_SUB_VERSION) && (0x02 == AI_SUB_VERSION)
        cfg->security_cfg.data_sl  = AI_DATA_SL4;
        cfg->security_cfg.video_er = AI_VIDEO_ENCRYPT_I_FR_ALL;
        cfg->security_cfg.image_er = AI_IMAGE_ENCRYPT_ALL;
#endif
        TUYA_CALL_ERR_GOTO(tuya_ai_client_init(__ai_agent_mq_handle, &cfg->security_cfg), EXIT);
        ty_subscribe_event(EVENT_AI_SESSION_CLOSE, "ai.agent", __ai_session_closed_evt, SUBSCRIBE_TYPE_NORMAL);
#if defined(ENABLE_JOYINSIDE) && (ENABLE_JOYINSIDE == 1)
    }
#endif
    ai_agent_ctx.codec_enable = cfg->codec_enable;
    __ai_agent_reg_encoder();
    __ai_agent_set_audio_encoder(&(cfg->attr.audio));
    ty_subscribe_event(EVENT_AI_CLIENT_RUN, "ai.agent", __ai_client_run_evt, SUBSCRIBE_TYPE_NORMAL);
    ty_subscribe_event(EVENT_DEVOS_STATE_CHANGE, "ai.agent", __ai_devos_state_evt, SUBSCRIBE_TYPE_NORMAL);
    TUYA_CALL_ERR_GOTO(tuya_ai_input_init(), EXIT);
    TUYA_CALL_ERR_GOTO(tuya_ai_output_init(&cfg->output), EXIT);
    PR_NOTICE("ai agent init");
    return rt;

EXIT:
    tuya_ai_agent_deinit();
    return rt;
}

VOID tuya_ai_agent_deinit(VOID)
{
    if (ai_agent_ctx.enable_joyinside) {
#if defined(ENABLE_JOYINSIDE) && (ENABLE_JOYINSIDE == 1)
        joyinside_client_deinit();
#endif
    } else {
        tuya_ai_client_deinit();
        ty_unsubscribe_event(EVENT_AI_SESSION_CLOSE, "ai.agent", __ai_session_closed_evt);
    }
    tuya_ai_input_deinit();
    tuya_ai_output_deinit();
    ty_unsubscribe_event(EVENT_AI_CLIENT_RUN, "ai.agent", __ai_client_run_evt);
    ty_unsubscribe_event(EVENT_DEVOS_STATE_CHANGE, "ai.agent", __ai_devos_state_evt);
    __ai_agent_destroy_encoder();
    __ai_agent_unreg_encoder();
    __ai_agent_free_recv_cb();
    if (ai_agent_ctx.event_param) {
        Free(ai_agent_ctx.event_param);
    }
    if (ai_agent_ctx.session_param) {
        Free(ai_agent_ctx.session_param);
    }
    memset(&ai_agent_ctx, 0, SIZEOF(ai_agent_ctx));
    PR_NOTICE("ai agent deinit");
    return;
}

STATIC USHORT_T __ai_agent_get_send_id(AI_PACKET_PT type, AI_AGENT_SESSION_T *session)
{
    USHORT_T idx = 0;
    for (idx = 0; idx < AI_BIZ_MAX_NUM; idx++) {
        if (session->send[idx].type == type) {
            return session->send[idx].id;
        }
    }
    return 0;
}

STATIC BOOL_T __ai_agent_is_first_pkt(AI_PACKET_PT type, AI_AGENT_SESSION_T *session)
{
    USHORT_T idx = 0;
    for (idx = 0; idx < AI_BIZ_MAX_NUM; idx++) {
        if (session->send[idx].type == type) {
            return session->send[idx].first_pkt;
        }
    }
    return FALSE;
}

STATIC VOID __ai_agent_set_first_pkt(AI_PACKET_PT type, BOOL_T flag, AI_AGENT_SESSION_T *session)
{
    USHORT_T idx = 0;
    for (idx = 0; idx < AI_BIZ_MAX_NUM; idx++) {
        if (session->send[idx].type == type) {
            session->send[idx].first_pkt = flag;
            return;
        }
    }
    return;
}

BOOL_T tuya_ai_agent_is_internal_scode(VOID)
{
    return ai_agent_ctx.enable_internal_scode;
}

VOID tuya_ai_agent_internal_scode_ctrl(BOOL_T flag)
{
    ai_agent_ctx.enable_internal_scode = flag;
}

OPERATE_RET tuya_ai_agent_start(CHAR_T *scode)
{
    OPERATE_RET rt = OPRT_OK;
    if (ai_agent_ctx.enable_joyinside) {
        return rt;
    } else {
        if (!ai_agent_ctx.enable_crt_session_ext) {
            rt = tuya_ai_agent_crt_session(scode, 0, 0, NULL, 0);
            if (rt != OPRT_OK) {
                PR_ERR("create ai agent session failed");
                return rt;
            }
        }
        return tuya_ai_agent_event_s(scode, AI_EVENT_START, 0);
    }
}

STATIC BOOL_T __ai_agent_is_need_stream_flag(AI_PACKET_PT ptype, UINT_T len, UINT_T total_len)
{
    if (AI_PT_IMAGE != ptype && len == total_len) {
        return TRUE;
    }
    return FALSE;
}

OPERATE_RET tuya_ai_agent_end(CHAR_T *scode)
{
    if (ai_agent_ctx.enable_joyinside) {
        tuya_ai_agent_event_s(scode, AI_EVENT_END, 0);
    } else {
        AI_AGENT_SESSION_T *session = tuya_ai_agent_get_session(scode);
        if (session == NULL) {
            return OPRT_COM_ERROR;
        }
        USHORT_T idx = 0;
        for (idx = 0; idx < AI_BIZ_MAX_NUM; idx++) {
            AI_PACKET_PT type = session->send[idx].type;
            if (__ai_agent_is_first_pkt(type, session)) {
                tuya_ai_agent_upload_stream(scode, type, NULL, NULL, 0, 0);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
                tuya_ai_agent_event_s(scode, AI_EVENT_PAYLOADS_END, type);
#endif
                __ai_agent_set_first_pkt(type, FALSE, session);
            }
        }

        tuya_ai_agent_event_s(scode, AI_EVENT_END, 0);
    }
    return OPRT_OK;
}

VOID tuya_ai_agent_set_attr(AI_PACKET_PT ptype, AI_ATTR_BIZ_T *attr)
{
    if (AI_PT_VIDEO == ptype) {
        memcpy(&ai_agent_ctx.up_attr.video, &(attr->video), SIZEOF(ai_agent_ctx.up_attr.video));
    } else if (AI_PT_AUDIO == ptype) {
        memcpy(&ai_agent_ctx.up_attr.audio, &(attr->audio), SIZEOF(ai_agent_ctx.up_attr.audio));
        __ai_agent_set_audio_encoder(&(attr->audio));
    } else if (AI_PT_IMAGE == ptype) {
        memcpy(&ai_agent_ctx.up_attr.image, &(attr->image), SIZEOF(ai_agent_ctx.up_attr.image));
    } else if (AI_PT_FILE == ptype) {
        memcpy(&ai_agent_ctx.up_attr.file, &(attr->file), SIZEOF(ai_agent_ctx.up_attr.file));
    }
}

VOID tuya_ai_agent_set_tts_cfg(AI_AGENT_TTS_CFG_T *cfg)
{
    if (cfg) {
        memcpy(&ai_agent_ctx.tts_cfg, cfg, SIZEOF(ai_agent_ctx.tts_cfg));
    }
}

VOID tuya_ai_agent_set_video_stream(AI_VIDEO_STREAM_E stream_type)
{
    ai_agent_ctx.video_stream = stream_type;
}

STATIC OPERATE_RET __ai_upload_stream(CHAR_T *scode, AI_PACKET_PT ptype, AI_BIZ_HD_T *biz, CHAR_T *data, UINT_T len, UINT_T total_len)
{
    if (ai_agent_ctx.enable_joyinside) {
        PR_DEBUG("[ptype %d upload stream] len:%d, total_len:%d", ptype, len, total_len);
#if defined(ENABLE_JOYINSIDE) && (ENABLE_JOYINSIDE == 1)
        if (ptype == AI_PT_TEXT) {
            return joyinside_send_text(data, len);
        } else if (ptype == AI_PT_AUDIO) {
            return joyinside_send_audio(data, len);
        }
#endif
    } else {
        AI_AGENT_SESSION_T *session = tuya_ai_agent_get_session(scode);
        if (session == NULL) {
            return OPRT_COM_ERROR;
        }
        AI_BIZ_ATTR_INFO_T attr;
        memset(&attr, 0, SIZEOF(attr));
        AI_STREAM_TYPE stype = AI_STREAM_ONE;
        if (__ai_agent_is_need_stream_flag(ptype, len, total_len)) {
            if (!__ai_agent_is_first_pkt(ptype, session)) {
                stype = AI_STREAM_START;
                __ai_agent_set_first_pkt(ptype, TRUE, session);
            } else if (data && (len > 0)) {
                stype = AI_STREAM_ING;
            } else {
                stype = AI_STREAM_END;
            }
        }

        if ((AI_STREAM_START == stype) || (AI_STREAM_ONE == stype)) {
            attr.flag = AI_HAS_ATTR;
            attr.type = ptype;
            if (AI_PT_VIDEO == ptype) {
                memcpy(&attr.value.video.base, &ai_agent_ctx.up_attr.video, SIZEOF(attr.value.video.base));
            } else if (AI_PT_AUDIO == ptype) {
                memcpy(&attr.value.audio.base, &ai_agent_ctx.up_attr.audio, SIZEOF(attr.value.audio.base));
            } else if (AI_PT_IMAGE == ptype) {
                memcpy(&attr.value.image.base, &ai_agent_ctx.up_attr.image, SIZEOF(attr.value.image.base));
            } else if (AI_PT_FILE == ptype) {
                memcpy(&attr.value.file.base, &ai_agent_ctx.up_attr.file, SIZEOF(attr.value.file.base));
            }
        }

        AI_BIZ_HEAD_INFO_T head;
        memset(&head, 0, SIZEOF(head));
        head.stream_flag = stype;
        head.total_len   = total_len;
        head.len         = len;
        if (biz) {
            if (AI_PT_VIDEO == ptype) {
                memcpy(&head.value.video, &(biz->video), SIZEOF(head.value.video));
            } else if (AI_PT_AUDIO == ptype) {
                memcpy(&head.value.audio, &(biz->audio), SIZEOF(head.value.audio));
            } else if (AI_PT_IMAGE == ptype) {
                memcpy(&head.value.image, &(biz->image), SIZEOF(head.value.image));
            }
        }

        USHORT_T id = __ai_agent_get_send_id(ptype, session);

        PR_DEBUG("[ptype %d upload stream] len:%d flag %d, total_len:%d, id:%d", ptype, len, stype, total_len, id);

        return tuya_ai_send_biz_pkt(id, &attr, ptype, &head, data);
    }
    return OPRT_OK;
}

// Buffer size (in bytes) for batching encoded audio before upload.
// 800 bytes accommodates up to ~64kbps at 40ms frame duration.
// At 32kbps/40ms: ~160 bytes/frame, fits 5 frames.
// At 16kbps/40ms: ~80 bytes/frame, fits 10 frames.
#ifndef AUDIO_ENCODE_BUF_SIZE
#define AUDIO_ENCODE_BUF_SIZE 800
#endif

STATIC OPERATE_RET __upload_data_cb(AI_AUDIO_CODEC_TYPE codec_type, UCHAR_T *data, UINT_T len, void *usr_data)
{
    AUDIO_ENCODE_CTX_T *encode_ctx = (AUDIO_ENCODE_CTX_T *)usr_data;

    if ((encode_ctx->buf_len + len) > encode_ctx->buf_size) {
        OPERATE_RET rt = __ai_upload_stream(encode_ctx->scode, AI_PT_AUDIO, encode_ctx->biz,
                                            (CHAR_T *)encode_ctx->buf, encode_ctx->buf_len, encode_ctx->buf_len);
        if (rt != OPRT_OK) {
            PR_ERR("upload encoded data failed, rt:%d", rt);
            return rt;
        }
        encode_ctx->buf_len = 0;
    }

    if ((encode_ctx->buf_len + len) > encode_ctx->buf_size) {
        PR_ERR("encoded frame too large: %d > %d", len, encode_ctx->buf_size);
        return OPRT_INVALID_PARM;
    }

    memcpy(encode_ctx->buf + encode_ctx->buf_len, data, len);
    encode_ctx->buf_len += len;
    return OPRT_OK;
}

OPERATE_RET tuya_ai_agent_upload_stream(CHAR_T *scode, AI_PACKET_PT ptype, AI_BIZ_HD_T *biz, CHAR_T *data, UINT_T len, UINT_T total_len)
{
    if (ptype == AI_PT_AUDIO && ai_agent_ctx.encoder) {
        if (ai_agent_ctx.encoder->handle == NULL) {
            PR_ERR("encoder is null");
            return OPRT_COM_ERROR;
        } else if (len != total_len) {
            PR_ERR("audio stream len not match, len:%d, total_len:%d", len, total_len);
            return OPRT_INVALID_PARM;
        }
        if (data && len > 0) {
            // encode data
            UCHAR_T            encode_buf[AUDIO_ENCODE_BUF_SIZE] = {0};
            AUDIO_ENCODE_CTX_T encode_ctx                        = {
                .scode    = scode,
                .biz      = biz,
                .buf      = encode_buf,
                .buf_size = AUDIO_ENCODE_BUF_SIZE,
                .buf_len  = 0,
            };
            OPERATE_RET rt = ai_agent_ctx.encoder->encode(ai_agent_ctx.encoder->handle, (UCHAR_T *)data, len, __upload_data_cb, (VOID *)&encode_ctx);
            if (rt != OPRT_OK) {
                PR_ERR("encoder failed, rt:%d", rt);
                return rt;
            }
            if (encode_ctx.buf_len > 0) {
                rt = __ai_upload_stream(encode_ctx.scode, AI_PT_AUDIO, encode_ctx.biz,
                                        (CHAR_T *)encode_ctx.buf, encode_ctx.buf_len, encode_ctx.buf_len);
            }
            return rt;
        } else {
            return __ai_upload_stream(scode, ptype, biz, data, len, total_len);
        }
    } else {
        return __ai_upload_stream(scode, ptype, biz, data, len, total_len);
    }
}

VOID tuya_ai_agent_server_vad_ctrl(BOOL_T flag)
{
    ai_agent_ctx.enable_serv_vad = flag;
}

VOID tuya_ai_agent_proc_interrupt_ctrl(BOOL_T flag)
{
    ai_agent_ctx.enable_proc_intr = flag;
}

STATIC OPERATE_RET __ai_event_start(AI_SESSION_ID sid, AI_SESSION_ID eid)
{
    OPERATE_RET rt = OPRT_OK;
    if (eid == NULL || sid == NULL) {
        return OPRT_COM_ERROR;
    }

    memset(eid, 0, AI_UUID_V4_LEN + 1);
    if (ai_agent_ctx.cfg_eid[0] != '\0') {
        memcpy(eid, ai_agent_ctx.cfg_eid, AI_UUID_V4_LEN);
        memset(ai_agent_ctx.cfg_eid, 0, SIZEOF(ai_agent_ctx.cfg_eid)); // clear cfg eid
    }
    BYTE_T *out     = NULL;
    UINT_T  out_len = 0;

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    CHAR_T start_attr[128] = {0};
    snprintf(start_attr, sizeof(start_attr),
             "{\"tts.alternate\":\"true\",\"asr.enableVad\":%s,\"processing.interrupt\":%s}",
             ai_agent_ctx.enable_serv_vad ? "true" : "false",
             ai_agent_ctx.enable_proc_intr ? "true" : "false");

    AI_ATTRIBUTE_T attr[] = {{
            .type = 1003,
            .payload_type = ATTR_PT_STR,
            .length = strlen(start_attr),
            .value.str = start_attr,
        }
    };

    tuya_pack_user_attrs(attr, CNTSOF(attr), &out, &out_len);
#else
    ty_cJSON *attr_info      = ty_cJSON_CreateObject();
    ty_cJSON *chatAttributes = ty_cJSON_CreateObject();
    ty_cJSON_AddItemToObject(attr_info, "chatAttributes", chatAttributes);
    ty_cJSON_AddBoolToObject(chatAttributes, "tts.alternate", true);
    ty_cJSON_AddBoolToObject(chatAttributes, "asr.enableVad", ai_agent_ctx.enable_serv_vad ? true : false);
    ty_cJSON_AddBoolToObject(chatAttributes, "processing.interrupt", ai_agent_ctx.enable_proc_intr ? true : false);
    if (ai_agent_ctx.event_param) {
        ty_cJSON *custom = ty_cJSON_Parse(ai_agent_ctx.event_param);
        if (custom) {
            ty_cJSON *sessionAttributes = ty_cJSON_CreateObject();
            ty_cJSON_AddItemToObject(sessionAttributes, "custom.param", custom);
            ty_cJSON_AddItemToObject(attr_info, "sessionAttributes", sessionAttributes);
        }
        Free(ai_agent_ctx.event_param);
        ai_agent_ctx.event_param = NULL;
    }
    out     = (BYTE_T *)ty_cJSON_PrintUnformatted(attr_info);
    out_len = strlen((CHAR_T *)out);
    ty_cJSON_Delete(attr_info);
    AI_PROTO_D("event start attr: %s ", out);
#endif
    rt = tuya_ai_event_start(sid, eid, out, out_len);
    Free(out);
    return rt;
}

STATIC OPERATE_RET __ai_event_payloads_end(AI_SESSION_ID sid, AI_SESSION_ID eid, USHORT_T send_id)
{
    OPERATE_RET rt = OPRT_OK;
    if (eid == NULL || sid == NULL) {
        PR_ERR("no active session or eid");
        return OPRT_COM_ERROR;
    }

    AI_ATTRIBUTE_T attr = {
        .type         = 1002,
        .payload_type = ATTR_PT_U16,
        .length       = 2,
        .value.u16    = send_id,
    };
    BYTE_T *out     = NULL;
    UINT_T  out_len = 0;
    tuya_pack_user_attrs(&attr, 1, &out, &out_len);
    rt = tuya_ai_event_payloads_end(sid, eid, out, out_len);
    Free(out);
    return rt;
}

OPERATE_RET tuya_ai_agent_event_s(CHAR_T *scode, AI_EVENT_TYPE etype, AI_PACKET_PT ptype)
{
    OPERATE_RET rt = OPRT_OK;

    if (ai_agent_ctx.enable_joyinside) {
#if defined(ENABLE_JOYINSIDE) && (ENABLE_JOYINSIDE == 1)
        if (AI_EVENT_CHAT_BREAK == etype) {
            if (tuya_ai_output_is_playing()) {
                return joyinside_send_event(JD_EVENT_INTERRUPT);
            }
        } else if (AI_EVENT_END == etype) {
            return joyinside_send_event(JD_EVENT_FINISH);
        }
#endif
    } else {
        AI_AGENT_SESSION_T *session = tuya_ai_agent_get_session(scode);
        if ((session == NULL) || (session->sid[0] == 0)) {
            return OPRT_COM_ERROR;
        }
        if (AI_EVENT_START == etype) {
            rt = __ai_event_start(session->sid, session->eid);
        } else if (AI_EVENT_PAYLOADS_END == etype) {
            USHORT_T send_id = __ai_agent_get_send_id(ptype, session);
            rt               = __ai_event_payloads_end(session->sid, session->eid, send_id);
        } else if (AI_EVENT_END == etype) {
            rt = tuya_ai_event_end(session->sid, session->eid, NULL, 0);
        } else if (AI_EVENT_ONE_SHOT == etype) {
            rt = tuya_ai_event_one_shot(session->sid, session->eid, NULL, 0);
        } else if (AI_EVENT_CHAT_BREAK == etype) {
            rt = tuya_ai_event_chat_break(session->sid, session->eid, NULL, 0);
        }
    }

    return rt;
}

OPERATE_RET tuya_ai_agent_trigger(CHAR_T *scode, CONST CHAR_T *trigger, CHAR_T *param)
{
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    return OPRT_NOT_SUPPORTED;
#endif
    OPERATE_RET rt      = OPRT_OK;
    BYTE_T     *out     = NULL;
    UINT_T      out_len = 0;

    AI_AGENT_SESSION_T *session = tuya_ai_agent_get_session(scode);
    if ((session == NULL) || (session->sid[0] == 0)) {
        return OPRT_COM_ERROR;
    }

    ty_cJSON *attr_info       = ty_cJSON_CreateObject();
    ty_cJSON *eventAttributes = ty_cJSON_CreateObject();
    ty_cJSON_AddItemToObject(attr_info, "eventAttributes", eventAttributes);
    ty_cJSON *custom_param = ty_cJSON_CreateObject();
    ty_cJSON_AddItemToObject(eventAttributes, "custom.param", custom_param);
    ty_cJSON *sys_trigger = ty_cJSON_CreateObject();
    ty_cJSON_AddItemToObject(custom_param, "sys.trigger", sys_trigger);
    ty_cJSON_AddStringToObject(sys_trigger, "value", trigger);
    if (param && strlen(param) > 0) {
        ty_cJSON *extra = ty_cJSON_Parse(param);
        if (extra) {
            ty_cJSON *child = extra->child;
            while (child) {
                ty_cJSON *next = child->next;
                ty_cJSON_DetachItemViaPointer(extra, child);
                ty_cJSON_AddItemToObject(custom_param, child->string, child);
                child = next;
            }
            ty_cJSON_Delete(extra);
        }
    }
    out = (BYTE_T *)ty_cJSON_PrintUnformatted(attr_info);
    if (out == NULL) {
        ty_cJSON_Delete(attr_info);
        return OPRT_MALLOC_FAILED;
    }
    out_len = strlen((CHAR_T *)out);
    ty_cJSON_Delete(attr_info);
    CHAR_T eid[AI_UUID_V4_LEN + 1] = {0};
    tuya_ai_basic_uuid_short(eid);
    PR_DEBUG("event trigger attr: %s, eid:%s, sid:%s", out, eid, session->sid);
    rt = tuya_ai_event_trigger(session->sid, eid, out, out_len);
    Free(out);
    return rt;
}

VOID tuya_ai_agent_set_eid(AI_EVENT_ID eid)
{
    if (eid && strlen(eid) > 0) {
        memset(ai_agent_ctx.cfg_eid, 0, AI_UUID_V4_LEN + 1);
        memcpy(ai_agent_ctx.cfg_eid, eid, AI_UUID_V4_LEN);
        PR_DEBUG("set eid:%s", eid);
    }
}

VOID tuya_ai_agent_set_event_param(CHAR_T *value)
{
    if (value == NULL || strlen(value) == 0) {
        PR_ERR("event param value is empty");
        return;
    }
    ty_cJSON *json = ty_cJSON_Parse(value);
    if (json == NULL) {
        PR_ERR("event param value is not valid json");
        return;
    }
    ty_cJSON_Delete(json);
    if (ai_agent_ctx.event_param) {
        Free(ai_agent_ctx.event_param);
        ai_agent_ctx.event_param = NULL;
    }
    ai_agent_ctx.event_param = Malloc(strlen(value) + 1);
    if (ai_agent_ctx.event_param == NULL) {
        PR_ERR("malloc event param failed");
        return;
    }
    memset(ai_agent_ctx.event_param, 0, strlen(value) + 1);
    memcpy(ai_agent_ctx.event_param, value, strlen(value));
}

VOID tuya_ai_agent_set_session_param(CHAR_T *value)
{
    if (value == NULL || strlen(value) == 0) {
        PR_ERR("session param value is empty");
        return;
    }
    ty_cJSON *json = ty_cJSON_Parse(value);
    if (json == NULL) {
        PR_ERR("session param value is not valid json");
        return;
    }
    ty_cJSON_Delete(json);
    if (ai_agent_ctx.session_param) {
        Free(ai_agent_ctx.session_param);
        ai_agent_ctx.session_param = NULL;
    }
    ai_agent_ctx.session_param = Malloc(strlen(value) + 1);
    if (ai_agent_ctx.session_param == NULL) {
        PR_ERR("malloc session param failed");
        return;
    }
    memset(ai_agent_ctx.session_param, 0, strlen(value) + 1);
    memcpy(ai_agent_ctx.session_param, value, strlen(value));
}

OPERATE_RET tuya_ai_agent_mcp_set_cb(TY_AI_MCP_CB cb, VOID *user_data)
{
    if (!cb) {
        ai_agent_ctx.enable_mcp    = FALSE;
        ai_agent_ctx.mcp_cb        = NULL;
        ai_agent_ctx.mcp_user_data = NULL;
        PR_DEBUG("mcp cb cleared");
        return OPRT_OK;
    }
    ai_agent_ctx.enable_mcp    = TRUE;
    ai_agent_ctx.mcp_cb        = cb;
    ai_agent_ctx.mcp_user_data = user_data;
    PR_DEBUG("mcp cb set");
    return OPRT_OK;
}

STATIC OPERATE_RET __mcp_handle(CHAR_T *sid, CHAR_T *eid, CHAR_T *data)
{
    TAL_PR_DEBUG("mcp data: %s", data);

    if (ai_agent_ctx.mcp_cb) {
        ty_cJSON   *root = ty_cJSON_Parse(data);
        OPERATE_RET rt   = ai_agent_ctx.mcp_cb(sid, eid, root, ai_agent_ctx.mcp_user_data);
        ty_cJSON_Delete(root);
        if (rt == OPRT_OK) {
            return OPRT_OK;
        }
    }

    TAL_PR_ERR("mcp handle not found or handle failed");
    // generate error response
    CHAR_T *message = "{\"error\":{\"code\":500,\"message\":\"Internal Server Error\"}}";
    return tuya_ai_event_mcp(sid, eid, message);
}

OPERATE_RET tuya_ai_agent_mcp_response(CHAR_T *sid, CHAR_T *eid, CHAR_T *message)
{
    if (!message) {
        return OPRT_INVALID_PARM;
    }
    return tuya_ai_event_mcp(sid, eid, message);
}

BOOL_T tuya_ai_agent_is_ready(VOID)
{
    if (ai_agent_ctx.enable_joyinside) {
#if defined(ENABLE_JOYINSIDE) && (ENABLE_JOYINSIDE == 1)
        return joyinside_client_is_ready();
#endif
        return FALSE;
    } else {
        return tuya_ai_client_is_ready();
    }
}