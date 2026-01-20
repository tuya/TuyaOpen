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
#include "tuya_ai_http.h"
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

#define INTTERUPT_TIME_MAX  16

typedef struct {
    char scode[AI_SOLUTION_CODE_LEN];
    AI_ATTR_BASE_T up_attr;
    AI_ATTR_BASE_T down_attr;
    AI_INPUT_SEND_T biz_get[AI_BIZ_MAX_NUM];
    AI_AGENT_SESSION_T session[AI_SESSION_MAX_NUM];
    char cfg_eid[AI_UUID_V4_LEN + 1];     // will auto clear after session create
    BOOL_T codec_enable;
    TUYA_AI_ENCODER_T *encoder;             // encoder handle
    TUYA_AI_ENCODER_INFO_T encoder_info;    // encoder info
    AI_AGENT_TTS_CFG_T tts_cfg;
    char last_intr_time[INTTERUPT_TIME_MAX];  // last chat break time
    BOOL_T enable_crt_session_ext; // enable crt session external
    BOOL_T enable_internal_scode; // enable internal solution code
    BOOL_T enable_serv_vad; // enable server vad
    BOOL_T enable_mcp; // enable mcp tools
    BOOL_T enable_joyinside; // enable joyinside cloud
    TY_AI_MCP_CB mcp_cb;
    VOID *mcp_user_data;
} AI_AGENT_CTX_T;
STATIC AI_AGENT_CTX_T ai_agent_ctx;

STATIC OPERATE_RET __mcp_handle(char *data);
STATIC AI_SESSION_ID __ai_agent_get_eid(char *scode);

STATIC OPERATE_RET __parse_attr_time(uint8_t *data, uint32_t len, char *time_str)
{
    OPERATE_RET rt = OPRT_OK;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    AI_ATTRIBUTE_T *attr = NULL;
    uint32_t attr_num = 0, idx;
    ty_cJSON *root, *node;

    rt = tuya_parse_user_attrs((char *)data, len, &attr, &attr_num);
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
    tuya_debug_hex_dump("__parse_attr_time", 64, data, len);

    ty_cJSON *root, *attr, *time;
    root = ty_cJSON_Parse((char *)data);
    if (NULL == root) {
        rt = OPRT_COM_ERROR ;
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
        rt = OPRT_COM_ERROR ;
    }


    ty_cJSON_Delete(root);
#endif
    return rt;
}

STATIC OPERATE_RET __ai_event_cb(AI_EVENT_ATTR_T *event, AI_EVENT_HEAD_T *head, VOID *data)
{
    PR_DEBUG("[====ai event cb====] type:%d, sid:%s, eid:%s", 
        head ? head->type : -1, 
        event && event->session_id ? event->session_id : "(null)", 
        event && event->event_id ? event->event_id : "(null)");
    if ((head->type == AI_EVENT_CHAT_BREAK) || (head->type == AI_EVENT_SERVER_VAD)) {
        if (head->type == AI_EVENT_CHAT_BREAK && event->user_data && event->user_len > 0) {
            /* filter chat break event */
            char intr_time[INTTERUPT_TIME_MAX] = {0};
            OPERATE_RET rt = __parse_attr_time(event->user_data, event->user_len, intr_time);
            if (OPRT_OK == rt) {
                if ((ai_agent_ctx.last_intr_time[0] != '\0') && (strcmp(intr_time, ai_agent_ctx.last_intr_time) <= 0)) {
                    PR_DEBUG("Interrupt event ignored, last:%s, cur:%s", ai_agent_ctx.last_intr_time, intr_time);
                    return OPRT_OK;
                }
                strncpy(ai_agent_ctx.last_intr_time, intr_time, INTTERUPT_TIME_MAX);
            }
        }
        tuya_ai_output_event(head->type, 0, event->event_id);
    } else if (head->type == AI_EVENT_MCP_CMD && data) {
        __mcp_handle(data);
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __ai_audio_recv_cb(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, VOID *data, VOID *usr_data)
{
    OPERATE_RET rt = OPRT_OK;
    char *scode = (char *)usr_data;

    // handle alert input
    if (scode && (strcmp(scode, AI_AGENT_SCODE_ALERT) == 0)) {
        rt = tuya_ai_input_alert_notify(head->stream_flag == AI_STREAM_START);
        if (rt == OPRT_TIMEOUT) {
            PR_WARN("alert input timeout, ignore audio data");
            return OPRT_OK;
        }
    }

    switch (head->stream_flag) {
    case AI_STREAM_START:
        if (attr && attr->flag == AI_HAS_ATTR) {
            tuya_ai_output_attr(scode, attr);
        }
        rt = tuya_ai_output_start();
        rt += tuya_ai_output_write(AI_PT_AUDIO, (uint8_t *)data, head->len);
        break;
    case AI_STREAM_ING:
        rt = tuya_ai_output_write(AI_PT_AUDIO, (uint8_t *)data, head->len);
        break;
    case AI_STREAM_END:
        rt = tuya_ai_output_write(AI_PT_AUDIO, (uint8_t *)data, head->len);
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

STATIC OPERATE_RET __ai_direct_output(char *scode, AI_PACKET_PT type, AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char *data)
{
    OPERATE_RET rt = OPRT_OK;
    if (head->len == 0xFFFFFFFF) {
        PR_ERR("invalid head len");
        return OPRT_INVALID_PARM;
    }
    switch (head->stream_flag) {
    case AI_STREAM_START:
        if (attr && attr->flag == AI_HAS_ATTR) {
            tuya_ai_output_attr(scode, attr);
        }
        tuya_ai_output_event(AI_EVENT_START, type, __ai_agent_get_eid(scode));
        rt = tuya_ai_output_media(scode, type, data, head->len, head->len);
        break;
    case AI_STREAM_ING:
        rt = tuya_ai_output_media(scode, type, data, head->len, head->len);
        break;
    case AI_STREAM_END:
        rt = tuya_ai_output_media(scode, type, data, head->len, head->len);
        tuya_ai_output_event(AI_EVENT_END, type, __ai_agent_get_eid(scode));
        break;
    case AI_STREAM_ONE:
        if (attr && attr->flag == AI_HAS_ATTR) {
            tuya_ai_output_attr(scode, attr);
        }
        tuya_ai_output_event(AI_EVENT_START, type, __ai_agent_get_eid(scode));
        rt = tuya_ai_output_media(scode, type, data, head->len, head->len);
        tuya_ai_output_event(AI_EVENT_END, type, __ai_agent_get_eid(scode));
        break;
    default:
        break;
    }

    return rt;
}

STATIC OPERATE_RET __ai_image_recv_cb(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, VOID *data, VOID *usr_data)
{
    OPERATE_RET rt = OPRT_OK;
    char *decode_base64 = NULL;
    uint32_t decode_len = 0;
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
                return tuya_ai_http_dld_image(data, __ai_image_recv_cb);
            } else if (img_type == IMAGE_PAYLOAD_TYPE_BASE64) {
                memcpy(&ai_agent_ctx.down_attr.image, &attr->value.image, SIZEOF(AI_IMAGE_ATTR_T));
                decode_base64 = Malloc(head->len);
                if (decode_base64 == NULL) {
                    PR_ERR("malloc decode_base64 failed");
                    return OPRT_MALLOC_FAILED;
                }
                memset(decode_base64, 0, head->len);
                decode_len = tuya_base64_decode(data, (uint8_t *)decode_base64);
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
            decode_len = tuya_base64_decode(data, (uint8_t *)decode_base64);
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
            decode_len = tuya_base64_decode(data, (uint8_t *)decode_base64);
        }
        break;
    case AI_STREAM_ONE:
        memset(&ai_agent_ctx.down_attr.image, 0, SIZEOF(AI_IMAGE_ATTR_BASE_T));
        if (attr && attr->flag == AI_HAS_ATTR) {
            img_type = attr->value.image.base.type;
            if (img_type == IMAGE_PAYLOAD_TYPE_URL) {
                memcpy(&ai_agent_ctx.down_attr.image, &attr->value.image, SIZEOF(AI_IMAGE_ATTR_T));
                PR_NOTICE("download image from url:%d, %s", head->len, data);
                return tuya_ai_http_dld_image(data, __ai_image_recv_cb);
            } else if (img_type == IMAGE_PAYLOAD_TYPE_BASE64) {
                memcpy(&ai_agent_ctx.down_attr.image, &attr->value.image, SIZEOF(AI_IMAGE_ATTR_T));
                decode_base64 = Malloc(head->len);
                if (decode_base64 == NULL) {
                    PR_ERR("malloc decode_base64 failed");
                    return OPRT_MALLOC_FAILED;
                }
                memset(decode_base64, 0, head->len);
                decode_len = tuya_base64_decode(data, (uint8_t *)decode_base64);
            }
        }
        break;
    }
#endif
    if ((decode_len > 0) && decode_base64) {
        head->len = decode_len;
        rt = __ai_direct_output((char *)usr_data, AI_PT_IMAGE, attr, head, decode_base64);
        Free(decode_base64);
    } else {
        rt = __ai_direct_output((char *)usr_data, AI_PT_IMAGE, attr, head, (char *)data);
    }
    return rt;
}

STATIC OPERATE_RET __ai_video_recv_cb(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, VOID *data, VOID *usr_data)
{
    return __ai_direct_output((char *)usr_data, AI_PT_VIDEO, attr, head, (char *)data);
}

STATIC OPERATE_RET __ai_file_recv_cb(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, VOID *data, VOID *usr_data)
{
    OPERATE_RET rt = OPRT_OK;
    char *decode_base64 = NULL;
    uint32_t decode_len = 0;
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
                decode_len = tuya_base64_decode(data, (uint8_t *)decode_base64);
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
            decode_len = tuya_base64_decode(data, (uint8_t *)decode_base64);
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
            decode_len = tuya_base64_decode(data, (uint8_t *)decode_base64);
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
                decode_len = tuya_base64_decode(data, (uint8_t *)decode_base64);
            }
        }
        break;
    }
#endif
    if ((decode_len > 0) && decode_base64) {
        head->len = decode_len;
        rt = __ai_direct_output((char *)usr_data, AI_PT_FILE, attr, head, decode_base64);
        Free(decode_base64);
    } else {
        rt = __ai_direct_output((char *)usr_data, AI_PT_FILE, attr, head, (char *)data);
    }
    return rt;
}

STATIC OPERATE_RET __ai_parse_asr(char *scode, ty_cJSON *root, BOOL_T eof)
{
    ty_cJSON *node = ty_cJSON_GetObjectItem(root, "text");
    PR_DEBUG("ASR text: %s", ty_cJSON_GetStringValue(node));
    if (eof && (!node || !node->valuestring || strlen(node->valuestring) == 0)) {
        tuya_ai_output_alert(AT_PLEASE_AGAIN);
    }
    tuya_ai_output_text(scode, AI_TEXT_ASR, node, eof);
    return OPRT_OK;
}

STATIC OPERATE_RET __ai_parse_nlg(char *scode, ty_cJSON *root, BOOL_T eof)
{
    OPERATE_RET rt = OPRT_OK;
    ty_cJSON *node;
    PR_TRACE("NLG text: %s", ty_cJSON_GetStringValue(ty_cJSON_GetObjectItem(root, "content")));
    tuya_ai_output_text(scode, AI_TEXT_NLG, root, eof);
    node = ty_cJSON_GetObjectItem(root, "images");
    if (node) {
        ty_cJSON *url = ty_cJSON_GetObjectItem(node, "url");
        if (url) {
            uint32_t image_num = 0, idx = 0;
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
    ty_cJSON *action = NULL;

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

STATIC OPERATE_RET __ai_parse_skill(char *scode, ty_cJSON *root)
{
    OPERATE_RET rt = OPRT_OK;
    ty_cJSON *node = ty_cJSON_GetObjectItem(root, "code");
    char *code = ty_cJSON_GetStringValue(node);
    if (!code) {
        return OPRT_OK;
    }
    if (strcmp(code, "DeviceControl") == 0) {
        ty_cJSON *dp_json = NULL;
        if ((rt = __ai_parse_dev_ctrl(root, &dp_json)) == OPRT_OK && dp_json) {
            dp_json = ty_cJSON_Duplicate(dp_json, TRUE);
            SF_GW_DEV_CMD_S gd_cmd = {DP_CMD_AI_SKILL, dp_json};
            rt = sf_send_gw_dev_cmd(&gd_cmd);
            if (rt != OPRT_OK) {
                ty_cJSON_Delete(dp_json);
                PR_ERR("send gw dev cmd failed: %d", rt);
            } else {
                PR_DEBUG("send gw dev cmd success");
            }
        } else {
            tuya_ai_output_text(scode, AI_TEXT_SKILL, root, FALSE);
        }
    } else if (strcmp(code, "speechControl") == 0) {
        BOOL_T exit = 0;
        if ((rt = __ai_parse_speech_control(root, &exit)) == OPRT_OK) {
            if (exit) {
                tuya_ai_output_event(AI_EVENT_CHAT_EXIT, 0, __ai_agent_get_eid(scode));
            }
        } else {
            tuya_ai_output_text(scode, AI_TEXT_SKILL, root, FALSE);
        }
    } else {
        tuya_ai_output_text(scode, AI_TEXT_SKILL, root, FALSE);
    }
    return rt;
}

STATIC OPERATE_RET __ai_parse_cloud_event(char *scode, ty_cJSON *root)
{
    OPERATE_RET rt = OPRT_OK;
    ty_cJSON *node = ty_cJSON_GetObjectItem(root, "action");
    char *action = ty_cJSON_GetStringValue(node);

    if (action && strcmp(action, "TriggerAiChat") == 0) {
        ty_cJSON *data = ty_cJSON_GetObjectItem(root, "data");
        char *content = ty_cJSON_GetStringValue(ty_cJSON_GetObjectItem(data, "content"));
        char *request_id = ty_cJSON_GetStringValue(ty_cJSON_GetObjectItem(data, "requestId"));
        if (!content || !request_id) {
            PR_ERR("content or request_id is NULL");
            return OPRT_INVALID_PARM;
        }
        rt = tuya_ai_input_cloud_trigger(request_id, content);
        if (OPRT_OK != rt) {
            PR_ERR("ty_ai_proc_event_send failed");
            return OPRT_COM_ERROR;
        }
        return OPRT_OK;
    } else {
        tuya_ai_output_text(scode, AI_TEXT_CLOUD_EVENT, root, FALSE);
    }

    return rt;
}

STATIC OPERATE_RET __ai_text_recv_cb(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, VOID *data, VOID *usr_data)
{
    if (!data || !head) {
        return OPRT_OK;
    }
    OPERATE_RET rt = OPRT_OK;
    BOOL_T eof = FALSE;

    char *scode = (char *)usr_data;
    if (!scode) {
        scode = ai_agent_ctx.scode;
    }
    ty_cJSON *root = NULL;
    if (head->data_type == AI_BIZ_DATA_TYPE_BYTE) {
        if ((!head->len) || (!data) || (strlen((char *)data) == 0)) {
            return OPRT_OK;
        }
        root = ty_cJSON_Parse((char *)data);
    } else if (head->data_type == AI_BIZ_DATA_TYPE_JSON) {
        root = (ty_cJSON *)data;
    } else {
        PR_ERR("data type err %d", head->data_type);
        return OPRT_INVALID_PARM;
    }
    if (root == NULL) {
        return OPRT_OK;
    }

    ty_cJSON *json_bizType = ty_cJSON_GetObjectItem(root, "bizType");
    char *bizType = ty_cJSON_GetStringValue(json_bizType);

    ty_cJSON *json_eof = ty_cJSON_GetObjectItem(root, "eof");
    if (json_eof) {
        eof = ty_cJSON_GetNumberValue(json_eof);
    }
    ty_cJSON *json_data = ty_cJSON_GetObjectItem(root, "data");

    if (eof && bizType && strcmp(bizType, "ASR") == 0) {
        __ai_parse_asr(scode, json_data, eof);
    } else if (bizType && strcmp(bizType, "NLG") == 0) {
        __ai_parse_nlg(scode, json_data, eof);
    } else if (bizType && strcmp(bizType, "SKILL") == 0) {
        __ai_parse_skill(scode, json_data);
    } else if (bizType && strcmp(bizType, "CloudEvent") == 0) {
        __ai_parse_cloud_event(scode, json_data);
    } else {
        tuya_ai_output_text(scode, AI_TEXT_OTHER, root, eof);
    }
    if (head->data_type == AI_BIZ_DATA_TYPE_BYTE) {
        ty_cJSON_Delete(root);
    }
    return rt;
}

STATIC AI_INPUT_SEND_T* __ai_agent_biz_get(AI_PACKET_PT type)
{
    uint16_t idx = 0;
    for (idx = 0; idx < AI_BIZ_MAX_NUM; idx++) {
        if (ai_agent_ctx.biz_get[idx].type == type) {
            return &ai_agent_ctx.biz_get[idx];
        }
    }
    return NULL;
}

STATIC BOOL_T __ai_agent_update_sid(char *scode)
{
    uint32_t idx = 0;
    BOOL_T found = FALSE;
    if (!scode) {
        return FALSE;
    }

    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (!found && strcmp(ai_agent_ctx.session[idx].scode, scode) == 0 &&
            ai_agent_ctx.session[idx].sid[0] != '\0') {
            // set current session as active
            PR_NOTICE("set session active, scode:%s, sid:%s", scode, ai_agent_ctx.session[idx].sid);
            ai_agent_ctx.session[idx].is_active = TRUE;
            found = TRUE;
        } else {
            // reset other as unactive
            ai_agent_ctx.session[idx].is_active = FALSE;
        }
    }
    return found;
}

// find a free session slot
STATIC uint32_t __ai_agent_find_free_sid(VOID)
{
    uint32_t idx;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_agent_ctx.session[idx].sid[0] == '\0') {
            return idx;
        }
    }
    return AI_SESSION_MAX_NUM;
}

STATIC VOID __ai_agent_set_sid(AI_AGENT_SESSION_T *session, char *scode, AI_SESSION_ID id, AI_SESSION_CFG_T *cfg, char *token)
{
    if (!scode) {
        return;
    }

    uint32_t jdx = 0;
    memset(session->scode, 0, SIZEOF(session->scode));
    strncpy(session->scode, scode, SIZEOF(session->scode) - 1);
    memcpy(session->sid, id, strlen(id));
    memcpy(session->token, token, strlen(token));
    session->is_active = TRUE;
    for (jdx = 0; jdx < cfg->send_num; jdx++) {
        session->send[jdx].id = cfg->send[jdx].id;
        session->send[jdx].type = cfg->send[jdx].type;
        session->send[jdx].first_pkt = FALSE;
    }

    return;
}

STATIC VOID __ai_agent_del_sid(AI_SESSION_ID id)
{
    uint32_t idx = 0;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (memcmp(ai_agent_ctx.session[idx].sid, id, strlen(id)) == 0) {
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

STATIC AI_AGENT_SESSION_T* __ai_agent_get_active_session(VOID)
{
    uint16_t idx = 0;
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_agent_ctx.session[idx].is_active) {
            return &ai_agent_ctx.session[idx];
        }
    }
    PR_TRACE("no active session found");
    return NULL;
}

AI_AGENT_SESSION_T* tuya_ai_agent_get_session(char *scode)
{
    uint16_t idx = 0;
    if (!scode) {
        scode = AI_AGENT_SCODE_DEFAULT;
    }
    for (idx = 0; idx < AI_SESSION_MAX_NUM; idx++) {
        if (ai_agent_ctx.session[idx].sid[0] != '\0' &&
            strcmp(ai_agent_ctx.session[idx].scode, scode) == 0) {
            return &ai_agent_ctx.session[idx];
        }
    }
    PR_TRACE("no session found for scode:%s", scode);
    return NULL;
}

STATIC AI_SESSION_ID __ai_agent_get_sid(VOID)
{
    AI_AGENT_SESSION_T *session = __ai_agent_get_active_session();
    if (session) {
        return session->sid;
    }
    return NULL;
}

STATIC AI_SESSION_ID __ai_agent_get_eid(char *scode)
{
    AI_AGENT_SESSION_T *session = tuya_ai_agent_get_session(scode);
    if (session) {
        return session->eid;
    }
    return "";
}

AI_BIZ_RECV_CB tuya_ai_agent_get_recv_cb(AI_PACKET_PT type)
{
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

AI_EVENT_CB tuya_ai_agent_get_evt_cb(void)
{
    return __ai_event_cb;
}

OPERATE_RET tuya_ai_agent_crt_session(char *scode, uint32_t bizCode, uint64_t bizTag, uint8_t *attr, uint32_t attr_len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_INPUT_SEND_T *input_send = NULL;
    uint32_t idx = 0;

    if (__ai_agent_update_sid(scode)) {
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
    rt = tuya_ai_mq_token_req(scode, &agent);
    if (rt != OPRT_OK) {
        if (agent.tts_url[0] != 0) {
            PR_ERR("get agent token failed, tts url:%s", agent.tts_url);
            tuya_ai_http_dld_audio(agent.tts_url, __ai_audio_recv_cb);
        }
        return rt;
    }

    session_cfg.send_num = agent.biz.send_num;
    for (idx = 0; idx < agent.biz.send_num; idx++) {
        session_cfg.send[idx].type = agent.biz.send[idx];
        input_send = __ai_agent_biz_get(agent.biz.send[idx]);
        if (input_send && input_send->multi_session) {
            session_cfg.send[idx].id = tuya_ai_biz_get_reuse_send_id(agent.biz.send[idx]);
        } else {
            session_cfg.send[idx].id = tuya_ai_biz_get_send_id();
        }
        session_cfg.send[idx].get_cb = input_send ? input_send->get_cb : NULL;
        session_cfg.send[idx].free_cb = input_send ? input_send->free_cb : NULL;
    }

    session_cfg.recv_num = agent.biz.recv_num;
    for (idx = 0; idx < agent.biz.recv_num; idx++) {
        session_cfg.recv[idx].type = agent.biz.recv[idx];
        session_cfg.recv[idx].id = tuya_ai_biz_get_recv_id();
        session_cfg.recv[idx].usr_data = session->scode;
        if (agent.biz.recv[idx] == AI_PT_TEXT) {
            session_cfg.recv[idx].cb = __ai_text_recv_cb;
        } else if (agent.biz.recv[idx] == AI_PT_AUDIO) {
            session_cfg.recv[idx].cb = __ai_audio_recv_cb;
        } else if (agent.biz.recv[idx] == AI_PT_IMAGE) {
            session_cfg.recv[idx].cb = __ai_image_recv_cb;
        } else if (agent.biz.recv[idx] == AI_PT_VIDEO) {
            session_cfg.recv[idx].cb = __ai_video_recv_cb;
        } else if (agent.biz.recv[idx] == AI_PT_FILE) {
            session_cfg.recv[idx].cb = __ai_file_recv_cb;
        }
    }
    session_cfg.event_cb = __ai_event_cb;

    uint8_t *out = NULL;
    uint32_t out_len = 0;
    if (attr && attr_len > 0) {
        out = attr;
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

        char *session_attrs = ty_cJSON_PrintUnformatted(root);
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
        ty_cJSON *attr_info = ty_cJSON_CreateObject();
        ty_cJSON *sessionAttributes = ty_cJSON_CreateObject();
        ty_cJSON_AddItemToObject(attr_info, "sessionAttributes", sessionAttributes);

        ty_cJSON *tts_order_supports = cJSON_CreateArray();
        ty_cJSON_AddItemToObject(sessionAttributes, "tts.order.supports", tts_order_supports);

        cJSON *supportItem = cJSON_CreateObject();
        cJSON_AddItemToArray(tts_order_supports, supportItem);
        //container
        ty_cJSON_AddStringToObject(supportItem, "container", "");
        //channels
        ty_cJSON_AddNumberToObject(supportItem, "channels", 1);
        //bitDepth
        ty_cJSON_AddStringToObject(supportItem, "bitDepth", "16");
        //bitRate
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

        out = (uint8_t *)ty_cJSON_PrintUnformatted(attr_info);
        out_len = strlen((char *)out);

        ty_cJSON_Delete(attr_info);
        AI_PROTO_D("user data out: %s ", out);
#endif
    }

    char sid[AI_UUID_V4_LEN + 1] = {0};
    if (bizCode == 0) {
        bizCode = agent.biz.code;
    }
    rt = tuya_ai_biz_crt_session(bizCode, bizTag, &session_cfg, out, out_len, agent.token, sid);
    if (rt != OPRT_OK) {
        PR_ERR("create session failed");
        tuya_ai_output_alert(AT_NETWORK_FAIL);
    } else {
        PR_DEBUG("create session success, %s", sid);
        __ai_agent_set_sid(session, scode, sid, &session_cfg, agent.token);
        memcpy(ai_agent_ctx.scode, scode, strlen(scode));
    }
    if (attr == NULL) {
        Free(out);
    }
    return rt;
}

OPERATE_RET tuya_ai_agent_del_session(char *scode)
{
    uint32_t idx = 0;
    char sid[AI_UUID_V4_LEN + 1] = {0};
    for (idx = 0; idx < AI_BIZ_MAX_NUM; idx++) {
        if (memcmp(ai_agent_ctx.session[idx].scode, scode, strlen(scode)) == 0) {
            PR_DEBUG("del session idx:%d", idx);
            memcpy(sid, ai_agent_ctx.session[idx].sid, strlen(ai_agent_ctx.session[idx].sid));
            memset(&ai_agent_ctx.session[idx], 0, SIZEOF(AI_AGENT_SESSION_T));
            break;
        }
    }
    if (idx >= AI_BIZ_MAX_NUM) {
        PR_ERR("session not found, scode:%s", scode);
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
    biz_head.data_type = AI_BIZ_DATA_TYPE_JSON;
    return __ai_text_recv_cb(NULL, &biz_head, root, ai_agent_ctx.scode);
}

STATIC OPERATE_RET __ai_client_run_evt(VOID_T *data)
{
    return tuya_ai_output_alert(AT_NETWORK_CONNECTED);
}

STATIC OPERATE_RET  __ai_devos_state_evt(VOID *data)
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
        ai_agent_ctx.encoder = tuya_ai_get_encoder(attr->codec_type);
        ai_agent_ctx.encoder_info.encode_type = attr->codec_type;
        ai_agent_ctx.encoder_info.sample_rate = attr->sample_rate;
        ai_agent_ctx.encoder_info.channels = attr->channels;
        ai_agent_ctx.encoder_info.bits_per_sample = attr->bit_depth;
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
    memset(&ai_agent_ctx, 0, SIZEOF(ai_agent_ctx));

#if defined(ENABLE_JOYINSIDE) && (ENABLE_JOYINSIDE == 1)
    if (cfg->jd_cfg) {
        ai_agent_ctx.enable_joyinside = TRUE;
        TUYA_CALL_ERR_GOTO(joyinside_client_init(cfg->jd_cfg), EXIT);
    } else {
#endif
        memcpy(ai_agent_ctx.scode, cfg->scode, strlen(cfg->scode));
        memcpy(&ai_agent_ctx.up_attr, &cfg->attr, SIZEOF(ai_agent_ctx.up_attr));
        memcpy(ai_agent_ctx.biz_get, cfg->biz_get, AI_BIZ_MAX_NUM * SIZEOF(AI_INPUT_SEND_T));
        memcpy(&ai_agent_ctx.tts_cfg, &cfg->tts_cfg, SIZEOF(ai_agent_ctx.tts_cfg));
        ai_agent_ctx.enable_crt_session_ext = cfg->enable_crt_session_ext;
        ai_agent_ctx.enable_internal_scode = cfg->enable_internal_scode;
        ai_agent_ctx.enable_serv_vad = TRUE;
        TUYA_CALL_ERR_GOTO(tuya_ai_client_init(__ai_agent_mq_handle), EXIT);
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
    // tuya_ai_agent_mcp_unregister_all();
    memset(&ai_agent_ctx, 0, SIZEOF(ai_agent_ctx));
    PR_NOTICE("ai agent deinit");
    return;
}

STATIC uint16_t __ai_agent_get_send_id(AI_PACKET_PT type, AI_AGENT_SESSION_T *session)
{
    uint16_t idx = 0;
    for (idx = 0; idx < AI_BIZ_MAX_NUM; idx++) {
        if (session->send[idx].type == type) {
            return session->send[idx].id;
        }
    }
    return 0;
}

STATIC BOOL_T __ai_agent_is_first_pkt(AI_PACKET_PT type, AI_AGENT_SESSION_T *session)
{
    uint16_t idx = 0;
    for (idx = 0; idx < AI_BIZ_MAX_NUM; idx++) {
        if (session->send[idx].type == type) {
            return session->send[idx].first_pkt;
        }
    }
    return FALSE;
}

STATIC VOID __ai_agent_set_first_pkt(AI_PACKET_PT type, BOOL_T flag, AI_AGENT_SESSION_T *session)
{
    uint16_t idx = 0;
    for (idx = 0; idx < AI_BIZ_MAX_NUM; idx++) {
        if (session->send[idx].type == type) {
            session->send[idx].first_pkt = flag;
            return;
        }
    }
    return;
}

BOOL_T tuya_ai_agent_is_internal_scode(void)
{
    return ai_agent_ctx.enable_internal_scode;
}

void tuya_ai_agent_internal_scode_ctrl(BOOL_T flag)
{
    ai_agent_ctx.enable_internal_scode = flag;
}

OPERATE_RET tuya_ai_agent_start(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    if (ai_agent_ctx.enable_joyinside) {
        return rt;
    } else {
        if (!ai_agent_ctx.enable_crt_session_ext) {
            rt = tuya_ai_agent_crt_session(ai_agent_ctx.scode, 0, 0, NULL, 0);
            if (rt != OPRT_OK) {
                PR_ERR("create ai agent session failed");
                return rt;
            }
        }
        return tuya_ai_agent_event(AI_EVENT_START, 0);
    }
}

STATIC BOOL_T __ai_agent_is_need_stream_flag(AI_PACKET_PT ptype, uint32_t len, uint32_t total_len)
{
    if (AI_PT_IMAGE != ptype && len == total_len) {
        return TRUE;
    }
    return FALSE;
}

OPERATE_RET tuya_ai_agent_end(VOID)
{
    if (ai_agent_ctx.enable_joyinside) {
        tuya_ai_agent_event(AI_EVENT_END, 0);
    } else {
        unsigned short idx = 0;
        AI_AGENT_SESSION_T *session = __ai_agent_get_active_session();
        if (!session) {
            return OPRT_COM_ERROR;
        }
        for (idx = 0; idx < AI_BIZ_MAX_NUM; idx++) {
            AI_PACKET_PT type = session->send[idx].type;
            if (__ai_agent_is_first_pkt(type, session)) {
                tuya_ai_agent_upload_stream(type, NULL, NULL, 0, 0);
    #if defined(AI_VERSION) && (0x01 == AI_VERSION)
                tuya_ai_agent_event(AI_EVENT_PAYLOADS_END, type);
    #endif
                __ai_agent_set_first_pkt(type, FALSE, session);
            }
        }

        tuya_ai_agent_event(AI_EVENT_END, 0);
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

STATIC OPERATE_RET __ai_upload_stream(AI_PACKET_PT ptype, AI_BIZ_HD_T *biz, char *data, uint32_t len, uint32_t total_len)
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
        AI_AGENT_SESSION_T *session = __ai_agent_get_active_session();
        if (!session) {
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
        head.total_len = total_len;
        head.len = len;
        if (biz) {
            if (AI_PT_VIDEO == ptype) {
                memcpy(&head.value.video, &(biz->video), SIZEOF(head.value.video));
            } else if (AI_PT_AUDIO == ptype) {
                memcpy(&head.value.audio, &(biz->audio), SIZEOF(head.value.audio));
            } else if (AI_PT_IMAGE == ptype) {
                memcpy(&head.value.image, &(biz->image), SIZEOF(head.value.image));
            }
        }

    uint16_t id = __ai_agent_get_send_id(ptype, session);

        PR_DEBUG("[ptype %d upload stream] len:%d flag %d, total_len:%d, id:%d", ptype, len, stype, total_len, id);

        return tuya_ai_send_biz_pkt(id, &attr, ptype, &head, data);
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __upload_data_cb(AI_AUDIO_CODEC_TYPE codec_type, uint8_t *data, uint32_t len, void *usr_data)
{
    return __ai_upload_stream(AI_PT_AUDIO, (AI_BIZ_HD_T *)usr_data, (char *)data, len, len);
}

OPERATE_RET tuya_ai_agent_upload_stream(AI_PACKET_PT ptype, AI_BIZ_HD_T *biz, char *data, uint32_t len, uint32_t total_len)
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
            OPERATE_RET rt = ai_agent_ctx.encoder->encode(ai_agent_ctx.encoder->handle, (uint8_t *)data, len, __upload_data_cb, (VOID *)biz);
            if (rt != OPRT_OK) {
                PR_ERR("encoder failed, rt:%d", rt);
                return rt;
            }
            return rt;
        } else {
            return __ai_upload_stream(ptype, biz, data, len, total_len);
        }
    } else {
        return __ai_upload_stream(ptype, biz, data, len, total_len);
    }
}

VOID tuya_ai_agent_server_vad_ctrl(BOOL_T flag)
{
    ai_agent_ctx.enable_serv_vad = flag;
}

STATIC OPERATE_RET __ai_event_start(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    char *eid = tuya_ai_agent_get_eid();
    if (!eid) {
        PR_ERR("no active session or eid");
        return OPRT_COM_ERROR;
    }
    memset(eid, 0, AI_UUID_V4_LEN + 1);
    AI_AGENT_SESSION_T *session = __ai_agent_get_active_session();
    if (!session) {
        return OPRT_COM_ERROR;
    }

    if (ai_agent_ctx.cfg_eid[0] != '\0') {
        memcpy(eid, ai_agent_ctx.cfg_eid, AI_UUID_V4_LEN);
        memset(ai_agent_ctx.cfg_eid, 0, SIZEOF(ai_agent_ctx.cfg_eid));  // clear cfg eid
    }
    uint8_t *out = NULL;
    uint32_t out_len = 0;

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    char start_attr[128] = {0};
    if (ai_agent_ctx.enable_serv_vad) {
        snprintf(start_attr, sizeof(start_attr), "{\"tts.alternate\":\"true\",\"asr.enableVad\":true,\"processing.interrupt\":true}");
    } else {
        snprintf(start_attr, sizeof(start_attr), "{\"tts.alternate\":\"true\",\"asr.enableVad\":false,\"processing.interrupt\":true}");
    }

    AI_ATTRIBUTE_T attr[] = {{
            .type = 1003,
            .payload_type = ATTR_PT_STR,
            .length = strlen(start_attr),
            .value.str = start_attr,
        }
    };

    tuya_pack_user_attrs(attr, CNTSOF(attr), &out, &out_len);
#else
    ty_cJSON *attr_info = ty_cJSON_CreateObject();
    ty_cJSON *chatAttributes = ty_cJSON_CreateObject();
    ty_cJSON_AddItemToObject(attr_info, "chatAttributes", chatAttributes);
    if (ai_agent_ctx.enable_serv_vad) {
        ty_cJSON_AddBoolToObject(chatAttributes, "asr.enableVad", true);
    } else {
        ty_cJSON_AddBoolToObject(chatAttributes, "asr.enableVad", false);
    }
    ty_cJSON_AddBoolToObject(chatAttributes, "processing.interrupt", true);

    out = (uint8_t *)ty_cJSON_PrintUnformatted(attr_info);
    out_len = strlen((char *)out);
    ty_cJSON_Delete(attr_info);
    AI_PROTO_D("event start attr: %s ", out);
#endif
    rt = tuya_ai_event_start(__ai_agent_get_sid(), eid, out, out_len);
    Free(out);
    return rt;
}

STATIC OPERATE_RET __ai_event_payloads_end(AI_PACKET_PT packet_type)
{
    OPERATE_RET rt = OPRT_OK;
    AI_AGENT_SESSION_T *session = __ai_agent_get_active_session();
    if (!session) {
        return OPRT_COM_ERROR;
    }
    AI_ATTRIBUTE_T attr = {
        .type = 1002,
        .payload_type = ATTR_PT_U16,
        .length = 2,
        .value.u16 = __ai_agent_get_send_id(packet_type, session),
    };
    uint8_t *out = NULL;
    uint32_t out_len = 0;
    tuya_pack_user_attrs(&attr, 1, &out, &out_len);
    rt = tuya_ai_event_payloads_end(__ai_agent_get_sid(), tuya_ai_agent_get_eid(), out, out_len);
    Free(out);
    return rt;
}

OPERATE_RET tuya_ai_agent_event(AI_EVENT_TYPE etype, AI_PACKET_PT ptype)
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
        PR_DEBUG("[upload event] type:%d", etype);
        if (AI_EVENT_START == etype) {
            rt = __ai_event_start();
        } else if (AI_EVENT_PAYLOADS_END == etype) {
            rt = __ai_event_payloads_end(ptype);
        } else if (AI_EVENT_END == etype) {
            rt = tuya_ai_event_end(__ai_agent_get_sid(), tuya_ai_agent_get_eid(), NULL, 0);
        } else if (AI_EVENT_ONE_SHOT == etype) {
            rt = tuya_ai_event_one_shot(__ai_agent_get_sid(), tuya_ai_agent_get_eid(), NULL, 0);
        } else if (AI_EVENT_CHAT_BREAK == etype) {
            rt = tuya_ai_event_chat_break(__ai_agent_get_sid(), tuya_ai_agent_get_eid(), NULL, 0);
        }
    }

    return rt;
}

VOID tuya_ai_agent_set_scode(char *scode)
{
    if (ai_agent_ctx.enable_joyinside) {
        return;
    }
    if (scode) {
        memset(ai_agent_ctx.scode, 0, SIZEOF(ai_agent_ctx.scode));
        strncpy(ai_agent_ctx.scode, scode, SIZEOF(ai_agent_ctx.scode) - 1);
        PR_DEBUG("set scode:%s", ai_agent_ctx.scode);
    }
    return;
}

char *tuya_ai_agent_get_active_scode(VOID)
{
    AI_AGENT_SESSION_T *session = __ai_agent_get_active_session();
    if (session) {
        return session->scode;
    }
    return NULL;
}

VOID tuya_ai_agent_set_eid(AI_EVENT_ID eid)
{
    if (eid && strlen(eid) > 0) {
        memset(ai_agent_ctx.cfg_eid, 0, AI_UUID_V4_LEN + 1);
        memcpy(ai_agent_ctx.cfg_eid, eid, AI_UUID_V4_LEN);
        PR_DEBUG("set eid:%s", eid);
    }
}

AI_EVENT_ID tuya_ai_agent_get_eid(VOID)
{
    return __ai_agent_get_eid(ai_agent_ctx.scode);
}

OPERATE_RET tuya_ai_agent_mcp_set_cb(TY_AI_MCP_CB cb, VOID *user_data)
{
    if (!cb) {
        ai_agent_ctx.enable_mcp = FALSE;
        ai_agent_ctx.mcp_cb = NULL;
        ai_agent_ctx.mcp_user_data = NULL;
        PR_DEBUG("mcp cb cleared");
        return OPRT_OK;
    }
    ai_agent_ctx.enable_mcp = TRUE;
    ai_agent_ctx.mcp_cb = cb;
    ai_agent_ctx.mcp_user_data = user_data;
    PR_DEBUG("mcp cb set");
    return OPRT_OK;
}

STATIC OPERATE_RET __mcp_handle(char *data)
{
    PR_DEBUG("mcp data: %s", data);

    if (ai_agent_ctx.mcp_cb) {
        ty_cJSON *root = ty_cJSON_Parse(data);
        OPERATE_RET rt = ai_agent_ctx.mcp_cb(root, ai_agent_ctx.mcp_user_data);
        ty_cJSON_Delete(root);
        if (rt == OPRT_OK) {
            return OPRT_OK;
        }
    }

    PR_ERR("mcp handle not found or handle failed");
    // generate error response
    char *message = "{\"error\":{\"code\":500,\"message\":\"Internal Server Error\"}}";
    return tuya_ai_event_mcp(__ai_agent_get_sid(), tuya_ai_agent_get_eid(), message);
}

OPERATE_RET tuya_ai_agent_mcp_response(char *message)
{
    if (!message) {
        return OPRT_INVALID_PARM;
    }
    return tuya_ai_event_mcp(__ai_agent_get_sid(), tuya_ai_agent_get_eid(), message);
}

BOOL_T tuya_ai_agent_is_ready(void)
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