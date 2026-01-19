/**
 * @file tuya_ai_mqtt.c
 * @author tuya
 * @brief ai mqtt
 * @version 0.1
 * @date 2025-05-17
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
#include <string.h>
#include <stdio.h>
#include "uni_log.h"
#include "tal_memory.h"
#include "tuya_ai_client.h"
#include "tal_semaphore.h"
#include "tuya_error_code.h"
#include "tal_time_service.h"
#include "mqc_app.h"
#include "tuya_devos_utils.h"
#include "tuya_ai_private.h"
#include "tuya_ai_mqtt.h"
#include "tuya_ai_http.h"
#include "tuya_ai_biz.h"
#include "tuya_ai_agent.h"

#define AI_MQ_PROTO_NUM        9000
#define AI_SEM_ACK_TIMEOUT     6000
#define AI_ATOP_THING_CONFIG_INFO "thing.aigc.basic.server.config.info"

typedef struct {
    AI_SERVER_CFG_INFO_T config;
    AI_AGENT_TOKEN_INFO_T agent;
    SEM_HANDLE sem;
    char biz_id[AI_UUID_V4_LEN + 1];
    AI_MQTT_RECV_CB recv_cb;
} AI_MQTT_CTX_T;
STATIC AI_MQTT_CTX_T *ai_mqtt_ctx = NULL;

STATIC OPERATE_RET __ai_mq_do_interrupt(ty_cJSON *root)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SESSION_CFG_T *cfg = tuya_ai_biz_get_session_cfg(NULL);
    TUYA_CHECK_NULL_RETURN(cfg, OPRT_INVALID_PARM);
    ty_cJSON *eventId = ty_cJSON_GetObjectItem(root, "eventId");
    ty_cJSON *time = ty_cJSON_GetObjectItem(root, "time");
    if (!eventId || !time) {
        PR_ERR("eventId or time is null");
        return OPRT_INVALID_PARM;
    }
    if (cfg->event_cb) {
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        /* pack timestamp into userdata attr */
        char *tmp = ty_cJSON_PrintUnformatted(root);
        AI_ATTRIBUTE_T attr[] = {{
                .type = 1008,
                .payload_type = ATTR_PT_STR,
                .length = strlen(tmp) + 1,
                .value.str = tmp,
            }
        };
        uint8_t *out = NULL;
        uint32_t out_len = 0;
        tuya_pack_user_attrs(attr, CNTSOF(attr), &out, &out_len);
        tal_free(tmp);
        AI_EVENT_ATTR_T event;
        memset(&event, 0, SIZEOF(AI_EVENT_ATTR_T));
        event.event_id = eventId->valuestring;
        event.user_data = out;
        event.user_len = out_len;
        AI_EVENT_HEAD_T head = {0};
        head.type = AI_EVENT_CHAT_BREAK;
        rt = cfg->event_cb(&event, &head, NULL);
        tal_free(out);
#else

        ty_cJSON *root = ty_cJSON_CreateObject();
        if (NULL == root) {
            PR_ERR("creat cjson obj error");
            return OPRT_COM_ERROR;
        }

        ty_cJSON *attr = ty_cJSON_CreateObject();
        if (NULL != attr) {
            ty_cJSON_AddItemToObject(root, "breakAttributes", attr);
            if (time->valuestring) {
                ty_cJSON_AddStringToObject(attr, "time", time->valuestring);
            }
            uint8_t *out = (uint8_t *)ty_cJSON_PrintUnformatted(root);

            AI_EVENT_ATTR_T event;
            memset(&event, 0, SIZEOF(AI_EVENT_ATTR_T));
            event.event_id = eventId->valuestring;
            event.user_data = out;
            event.user_len = strlen((char *)out);
            AI_EVENT_HEAD_T head = {0};
            head.type = AI_EVENT_CHAT_BREAK;
            rt = cfg->event_cb(&event, &head, NULL);

            tal_free(out);

        } else {
            PR_ERR("creat cjson obj error");
            return OPRT_COM_ERROR;
        }

        ty_cJSON_Delete(root);
#endif
    }
    return rt;
}

STATIC OPERATE_RET __ai_mq_do_text(ty_cJSON *root)
{
    if (ai_mqtt_ctx->recv_cb) {
        return ai_mqtt_ctx->recv_cb(root);
    }
    return OPRT_COM_ERROR;
}

OPERATE_RET tuya_ai_mq_ser_cfg_req(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    char *mq_msg = OS_MALLOC(256);
    TUYA_CHECK_NULL_RETURN(mq_msg, OPRT_MALLOC_FAILED);
    memset(mq_msg, 0, 256);
    char biz_id[AI_UUID_V4_LEN + 1] = {0};
    tuya_ai_basic_uuid_v4(biz_id);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    snprintf(mq_msg, 256, "{\"bizType\":\"EVENT\",\"bizId\":\"%s\",\"data\":{\"type\":\"thingGetServerInfo\"}}", biz_id);
#else
    snprintf(mq_msg, 256, "{\"bizType\":\"EVENT\",\"bizId\":\"%s\",\"data\":{\"type\":\"thingGetServerInfo\", \"pv\":%d}}", biz_id, 0x02);
#endif
    rt = mqc_send_custom_mqtt_msg(AI_MQ_PROTO_NUM, (uint8_t *)mq_msg);
    OS_FREE(mq_msg);
    if (OPRT_OK != rt) {
        PR_ERR("send mqtt msg err, rt:%d", rt);
        return rt;
    }
    memset(ai_mqtt_ctx->biz_id, 0, SIZEOF(ai_mqtt_ctx->biz_id));
    memcpy(ai_mqtt_ctx->biz_id, biz_id, AI_UUID_V4_LEN);
    rt = tal_semaphore_wait(ai_mqtt_ctx->sem, AI_SEM_ACK_TIMEOUT);
    if (rt != OPRT_OK) {
        PR_ERR("mq ser cfg ack timeout %d", rt);
    } else {
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        if ((ai_mqtt_ctx->config.host_num == 0) || !ai_mqtt_ctx->config.username ||
            !ai_mqtt_ctx->config.credential || !ai_mqtt_ctx->config.client_id) {
            rt = OPRT_COM_ERROR;
        }
#else
        if ((ai_mqtt_ctx->config.host_num == 0) || !ai_mqtt_ctx->config.derived_client_id) {
            rt = OPRT_COM_ERROR;
        }
#endif
    }
    return rt;
}

AI_SERVER_CFG_INFO_T* tuya_ai_mq_ser_cfg_get(VOID)
{
    return &ai_mqtt_ctx->config;
}

OPERATE_RET tuya_ai_mq_token_req(char *solution_code, AI_AGENT_TOKEN_INFO_T *agent)
{
    OPERATE_RET rt = OPRT_OK;
    char *mq_msg = OS_MALLOC(512);
    TUYA_CHECK_NULL_RETURN(mq_msg, OPRT_MALLOC_FAILED);
    memset(mq_msg, 0, 512);
    char biz_id[AI_UUID_V4_LEN + 1] = {0};
    tuya_ai_basic_uuid_v4(biz_id);
    snprintf(mq_msg, 512, "{\"bizType\":\"EVENT\",\"bizId\":\"%s\",\"devId\":\"%s\",\"data\":{\"type\":\"thingGetAgentToken\",\"data\":{\"aiSolutionCode\":\"%s\"}}}", \
            biz_id, get_gw_dev_id(), (!tuya_ai_agent_is_internal_scode() && solution_code) ? solution_code : "");
    rt = mqc_send_custom_mqtt_msg(AI_MQ_PROTO_NUM, (uint8_t *)mq_msg);
    OS_FREE(mq_msg);
    if (OPRT_OK != rt) {
        PR_ERR("send mqtt msg err, rt:%d", rt);
        return rt;
    }
    memset(ai_mqtt_ctx->biz_id, 0, SIZEOF(ai_mqtt_ctx->biz_id));
    memcpy(ai_mqtt_ctx->biz_id, biz_id, AI_UUID_V4_LEN);
    rt = tal_semaphore_wait(ai_mqtt_ctx->sem, AI_SEM_ACK_TIMEOUT);
    if (rt != OPRT_OK) {
        PR_ERR("mq token ack timeout %d", rt);
    } else {
        if (ai_mqtt_ctx->agent.token[0] == 0) {
            PR_ERR("get agent token failed");
            rt = OPRT_COM_ERROR;
        }
        memcpy(agent, &ai_mqtt_ctx->agent, SIZEOF(AI_AGENT_TOKEN_INFO_T));
    }
    return rt;
}

STATIC VOID __ai_mq_ser_cfg_free(VOID)
{
    uint32_t idx = 0;
    if (ai_mqtt_ctx) {
        if (ai_mqtt_ctx->config.username) {
            OS_FREE(ai_mqtt_ctx->config.username);
        }
        if (ai_mqtt_ctx->config.credential) {
            OS_FREE(ai_mqtt_ctx->config.credential);
        }
        if (ai_mqtt_ctx->config.client_id) {
            OS_FREE(ai_mqtt_ctx->config.client_id);
        }
        if (ai_mqtt_ctx->config.derived_algorithm) {
            OS_FREE(ai_mqtt_ctx->config.derived_algorithm);
        }
        if (ai_mqtt_ctx->config.derived_iv) {
            OS_FREE(ai_mqtt_ctx->config.derived_iv);
        }
        if (ai_mqtt_ctx->config.hosts) {
            for (idx = 0; idx < ai_mqtt_ctx->config.host_num; idx++) {
                OS_FREE(ai_mqtt_ctx->config.hosts[idx]);
            }
            OS_FREE(ai_mqtt_ctx->config.hosts);
        }
#if defined(AI_VERSION) && (0x02 == AI_VERSION)
        if (ai_mqtt_ctx->config.derived_client_id) {
            OS_FREE(ai_mqtt_ctx->config.derived_client_id);
        }
        if (ai_mqtt_ctx->config.rsa_public_key) {
            OS_FREE(ai_mqtt_ctx->config.rsa_public_key);
        }
#endif
        memset(&ai_mqtt_ctx->config, 0, SIZEOF(AI_SERVER_CFG_INFO_T));
    }
}

STATIC OPERATE_RET __ai_mq_parse_ser_cfg(ty_cJSON *root)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t idx = 0;

    __ai_mq_ser_cfg_free();
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    ty_cJSON *tcpport = ty_cJSON_GetObjectItem(root, "tcpport");
    ty_cJSON *username = ty_cJSON_GetObjectItem(root, "username");
    ty_cJSON *credential = ty_cJSON_GetObjectItem(root, "credential");
    ty_cJSON *hosts = ty_cJSON_GetObjectItem(root, "hosts");
    ty_cJSON *udpport = ty_cJSON_GetObjectItem(root, "udpport");
    ty_cJSON *expire = ty_cJSON_GetObjectItem(root, "expire");
    ty_cJSON *clientId = ty_cJSON_GetObjectItem(root, "clientId");
    ty_cJSON *derivedAlgorithm = ty_cJSON_GetObjectItem(root, "derivedAlgorithm");
    ty_cJSON *derivedIv = ty_cJSON_GetObjectItem(root, "derivedIv");
    if (!tcpport || !username || !credential || !hosts || !expire || !clientId ||
        !derivedAlgorithm || !derivedIv) {
        PR_ERR("ai key vaule was null");
        return OPRT_CJSON_PARSE_ERR;
    }

    ai_mqtt_ctx->config.host_num = ty_cJSON_GetArraySize(hosts);
    if (ai_mqtt_ctx->config.host_num <= 0) {
        PR_ERR("host num err %d", ai_mqtt_ctx->config.host_num);
        return OPRT_COM_ERROR;
    }
    ai_mqtt_ctx->config.tcp_port = tcpport->valueint;
    if (udpport) {
        ai_mqtt_ctx->config.udp_port = udpport->valueint;
    }
    ai_mqtt_ctx->config.expire = expire->valuedouble;
    ai_mqtt_ctx->config.username = mm_strdup(username->valuestring);
    ai_mqtt_ctx->config.credential = mm_strdup(credential->valuestring);
    ai_mqtt_ctx->config.client_id = mm_strdup(clientId->valuestring);
    ai_mqtt_ctx->config.derived_algorithm = mm_strdup(derivedAlgorithm->valuestring);
    ai_mqtt_ctx->config.derived_iv = mm_strdup(derivedIv->valuestring);
    ai_mqtt_ctx->config.hosts = OS_MALLOC(ai_mqtt_ctx->config.host_num * SIZEOF(char *));
    if ((!ai_mqtt_ctx->config.hosts) || (!ai_mqtt_ctx->config.username) ||
        (!ai_mqtt_ctx->config.credential) || (!ai_mqtt_ctx->config.client_id) ||
        (!ai_mqtt_ctx->config.derived_algorithm) || (!ai_mqtt_ctx->config.derived_iv)) {
        PR_ERR("malloc err");
        rt = OPRT_MALLOC_FAILED;
        goto EXIT;
    }

    AI_PROTO_D("expire:%lld", ai_mqtt_ctx->config.expire);
    AI_PROTO_D("username: %s, pwd: %s", ai_mqtt_ctx->config.username, ai_mqtt_ctx->config.credential);
    AI_PROTO_D("clinet_id: %s", ai_mqtt_ctx->config.client_id);

    for (idx = 0; idx < ai_mqtt_ctx->config.host_num; idx++) {
        ty_cJSON *host = ty_cJSON_GetArrayItem(hosts, idx);
        ai_mqtt_ctx->config.hosts[idx] = mm_strdup(host->valuestring);
        AI_PROTO_D("host[%d]: %s", idx, ai_mqtt_ctx->config.hosts[idx]);
        if (!ai_mqtt_ctx->config.hosts[idx]) {
            PR_ERR("malloc host err");
            rt = OPRT_MALLOC_FAILED;
            goto EXIT;
        }
    }
#else
    ty_cJSON *tcpport = ty_cJSON_GetObjectItem(root, "tcpport");
    ty_cJSON *hosts = ty_cJSON_GetObjectItem(root, "hosts");
    ty_cJSON *udpport = ty_cJSON_GetObjectItem(root, "udpport");
    ty_cJSON *expire = ty_cJSON_GetObjectItem(root, "expire");
    ty_cJSON *rsa_public_key = ty_cJSON_GetObjectItem(root, "rsa_public_key");
    ty_cJSON *derived_client_id = ty_cJSON_GetObjectItem(root, "derived_client_id");

    if (!tcpport || !hosts || !expire || !derived_client_id || !rsa_public_key) {
        PR_ERR("ai key vaule was null");
        return OPRT_CJSON_PARSE_ERR;
    }

    ai_mqtt_ctx->config.host_num = ty_cJSON_GetArraySize(hosts);
    if (ai_mqtt_ctx->config.host_num <= 0) {
        PR_ERR("host num err %d", ai_mqtt_ctx->config.host_num);
        return OPRT_COM_ERROR;
    }
    ai_mqtt_ctx->config.tcp_port = tcpport->valueint;
    if (udpport) {
        ai_mqtt_ctx->config.udp_port = udpport->valueint;
    }
    ai_mqtt_ctx->config.expire = (uint64_t)expire->valueint;
    ai_mqtt_ctx->config.derived_client_id = mm_strdup(derived_client_id->valuestring);
    ai_mqtt_ctx->config.rsa_public_key = mm_strdup(rsa_public_key->valuestring);
    ai_mqtt_ctx->config.hosts = OS_MALLOC(ai_mqtt_ctx->config.host_num * SIZEOF(char *));
    if ((!ai_mqtt_ctx->config.hosts) || (!ai_mqtt_ctx->config.derived_client_id) ||
        (!ai_mqtt_ctx->config.rsa_public_key)) {
        PR_ERR("malloc err");
        rt = OPRT_MALLOC_FAILED;
        goto EXIT;
    }

    AI_PROTO_D("expire:%lld", ai_mqtt_ctx->config.expire);
    AI_PROTO_D("derived_client_id: %s", ai_mqtt_ctx->config.derived_client_id);
    AI_PROTO_D("rsa_public_key: %s", ai_mqtt_ctx->config.rsa_public_key);

    for (idx = 0; idx < ai_mqtt_ctx->config.host_num; idx++) {
        ty_cJSON *host = ty_cJSON_GetArrayItem(hosts, idx);
        ai_mqtt_ctx->config.hosts[idx] = mm_strdup(host->valuestring);
        AI_PROTO_D("host[%d]: %s", idx, ai_mqtt_ctx->config.hosts[idx]);
        if (!ai_mqtt_ctx->config.hosts[idx]) {
            PR_ERR("malloc host err");
            rt = OPRT_MALLOC_FAILED;
            goto EXIT;
        }
    }
#endif
    return rt;

EXIT:
    __ai_mq_ser_cfg_free();
    return rt;
}

OPERATE_RET tuya_ai_atop_ser_cfg_req(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    uint64_t bizTag = 0;
    ty_cJSON *root = ty_cJSON_CreateObject();
    TUYA_CHECK_NULL_RETURN(root, OPRT_MALLOC_FAILED);
    ty_cJSON_AddNumberToObject(root, "bizTag", bizTag);
    char *post_data = ty_cJSON_PrintUnformatted(root);
    ty_cJSON_Delete(root);
    TUYA_CHECK_NULL_RETURN(post_data, OPRT_MALLOC_FAILED);

    ty_cJSON *result = NULL;
    rt = iot_httpc_common_post(AI_ATOP_THING_CONFIG_INFO, "1.0", NULL, get_gw_dev_id(),
                               post_data, NULL, &result);
    OS_FREE(post_data);
    if (OPRT_OK != rt) {
        PR_ERR("http post err, rt:%d", rt);
        return rt;
    }

    rt = __ai_mq_parse_ser_cfg(result);
    ty_cJSON_Delete(result);
    return rt;
}

STATIC BOOL_T __ai_mq_is_biz_id_vaild(ty_cJSON *bizId)
{
    if (bizId && memcmp(bizId->valuestring, ai_mqtt_ctx->biz_id, AI_UUID_V4_LEN) == 0) {
        memset(ai_mqtt_ctx->biz_id, 0, SIZEOF(ai_mqtt_ctx->biz_id));
        return TRUE;
    }
    PR_ERR("bizId is not match, bizId:%s, ctx bizId:%s", bizId ? bizId->valuestring : "null", ai_mqtt_ctx->biz_id);
    return FALSE;
}

STATIC OPERATE_RET __ai_mq_do_ser_cfg(ty_cJSON *root)
{
    OPERATE_RET rt = OPRT_OK;
    ty_cJSON *data = NULL, *success = NULL, *errorCode = NULL;
    if ((success = ty_cJSON_GetObjectItem(root, "success")) == NULL) {
        PR_ERR("server info success was null");
        return OPRT_CJSON_PARSE_ERR;
    }
    if (success->type == ty_cJSON_False) {
        PR_ERR("success was false");
        errorCode = ty_cJSON_GetObjectItem(root, "errorCode");
        if (errorCode && (strlen(errorCode->valuestring) > 0)) {
            PR_NOTICE("errorCode: %s", errorCode->valuestring);
        }
        return OPRT_CJSON_PARSE_ERR;
    }
    if ((data = ty_cJSON_GetObjectItem(root, "data")) == NULL) {
        PR_ERR("server info data was null");
        return OPRT_CJSON_PARSE_ERR;
    }

    rt = __ai_mq_parse_ser_cfg(data);
    tal_semaphore_post(ai_mqtt_ctx->sem);
    return rt;
}

STATIC OPERATE_RET __ai_mq_parse_biz_cfg(ty_cJSON *root, BOOL_T is_send)
{
    uint32_t size = 0, idx = 0;
    size = ty_cJSON_GetArraySize(root);
    if ((size > AI_BIZ_MAX_NUM) || (size == 0)) {
        PR_ERR("biz size err %d", size);
        return OPRT_CJSON_PARSE_ERR;
    }
    char *value = NULL;
    if (is_send) {
        ai_mqtt_ctx->agent.biz.send_num = size;
        value = ai_mqtt_ctx->agent.biz.send;
    } else {
        ai_mqtt_ctx->agent.biz.recv_num = size;
        value = ai_mqtt_ctx->agent.biz.recv;
    }
    for (idx = 0; idx < size; idx++) {
        ty_cJSON *item = ty_cJSON_GetArrayItem(root, idx);
        if (item) {
            if (strcmp(item->valuestring, "video") == 0) {
                value[idx] = AI_PT_VIDEO;
            } else if (strcmp(item->valuestring, "audio") == 0) {
                value[idx] = AI_PT_AUDIO;
            } else if (strcmp(item->valuestring, "image") == 0) {
                value[idx] = AI_PT_IMAGE;
            } else if (strcmp(item->valuestring, "file") == 0) {
                value[idx] = AI_PT_FILE;
            } else if (strcmp(item->valuestring, "text") == 0) {
                value[idx] = AI_PT_TEXT;
            } else {
                PR_ERR("send data type err");
                return OPRT_CJSON_PARSE_ERR;
            }
        }
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __ai_mq_parse_agent_token(ty_cJSON *root)
{
    OPERATE_RET rt = OPRT_OK;
    ty_cJSON *token = NULL, *biz_cfg = NULL;
    token = ty_cJSON_GetObjectItem(root, "agentToken");
    biz_cfg = ty_cJSON_GetObjectItem(root, "bizConfig");
    if (!token || !biz_cfg) {
        PR_ERR("agent token info was null");
        return OPRT_CJSON_PARSE_ERR;
    }
    ty_cJSON *bizcode = ty_cJSON_GetObjectItem(biz_cfg, "bizCode");
    ty_cJSON *send = ty_cJSON_GetObjectItem(biz_cfg, "sendData");
    ty_cJSON *recv = ty_cJSON_GetObjectItem(biz_cfg, "revData");
    if (!bizcode || !send || !recv) {
        PR_ERR("agent bizcode was null");
        return OPRT_CJSON_PARSE_ERR;
    }
    if (strlen(token->valuestring) >= AI_AGENT_TOKEN_LEN) {
        PR_ERR("agent token too long, len:%d", strlen(token->valuestring));
        return OPRT_INVALID_PARM;
    }
    memset(ai_mqtt_ctx->agent.token, 0, AI_AGENT_TOKEN_LEN);
    memcpy(ai_mqtt_ctx->agent.token, token->valuestring, strlen(token->valuestring));
    ai_mqtt_ctx->agent.biz.code = bizcode->valueint;
    rt = __ai_mq_parse_biz_cfg(send, TRUE);
    rt += __ai_mq_parse_biz_cfg(recv, FALSE);
    return rt;
}

STATIC OPERATE_RET __ai_mq_do_agent_token(ty_cJSON *root)
{
    OPERATE_RET rt = OPRT_OK;
    ty_cJSON *data = NULL, *success = NULL, *errorCode = NULL;
    if ((success = ty_cJSON_GetObjectItem(root, "success")) == NULL) {
        PR_ERR("token success was null");
        return OPRT_CJSON_PARSE_ERR;
    }
    if ((data = ty_cJSON_GetObjectItem(root, "data")) == NULL) {
        PR_ERR("token data was null");
        return OPRT_CJSON_PARSE_ERR;
    }
    AI_PROTO_D("token success: %d", success->type);

    if (success->type == ty_cJSON_False) {
        PR_ERR("success was false");
        errorCode = ty_cJSON_GetObjectItem(root, "errorCode");
        if (errorCode && (strlen(errorCode->valuestring) > 0)) {
            PR_NOTICE("errorCode: %s", errorCode->valuestring);
        }
        ty_cJSON *tts_url = ty_cJSON_GetObjectItem(data, "ttsUrl");
        if ((tts_url) && (strlen(tts_url->valuestring) > 0)) {
            if (strlen(tts_url->valuestring) >= AI_TTS_URL_LEN) {
                PR_ERR("tts url too long, len:%d", strlen(tts_url->valuestring));
                return OPRT_INVALID_PARM;
            }
            PR_NOTICE("ttsUrl: %s", tts_url->valuestring);
            memset(ai_mqtt_ctx->agent.tts_url, 0, AI_TTS_URL_LEN);
            memcpy(ai_mqtt_ctx->agent.tts_url, tts_url->valuestring, strlen(tts_url->valuestring));
        }
        tal_semaphore_post(ai_mqtt_ctx->sem);
        return rt;
    }
    rt = __ai_mq_parse_agent_token(data);
    tal_semaphore_post(ai_mqtt_ctx->sem);
    return rt;
}

STATIC OPERATE_RET __ai_mq_handle(ty_cJSON *root)
{
    OPERATE_RET rt = OPRT_OK;
    ty_cJSON *data = NULL, *biz_type = NULL, *type = NULL, *bizId = NULL;
    AI_PROTO_D("recv ai mqtt msg");
    if ((data = ty_cJSON_GetObjectItem(root, "data")) == NULL) {
        PR_ERR("no 0 data");
        return OPRT_CJSON_PARSE_ERR;
    }

    biz_type = ty_cJSON_GetObjectItem(data, "bizType");
    if ((biz_type == NULL) || strlen(biz_type->valuestring) == 0) {
        PR_ERR("has no bizType");
        return OPRT_CJSON_PARSE_ERR;
    }
    bizId = ty_cJSON_GetObjectItem(data, "bizId");

    if (strcmp(biz_type->valuestring, "EVENT") == 0) {
        if ((data = ty_cJSON_GetObjectItem(data, "data")) == NULL) {
            PR_ERR("no 1 data");
            return OPRT_CJSON_PARSE_ERR;
        }
        type = ty_cJSON_GetObjectItem(data, "type");
        if ((type == NULL) || (strlen(type->valuestring) == 0)) {
            PR_ERR("has no type");
            return OPRT_CJSON_PARSE_ERR;
        }
        if ((data = ty_cJSON_GetObjectItem(data, "data")) == NULL) {
            PR_ERR("no 2 data");
            return OPRT_CJSON_PARSE_ERR;
        }
        if (strcmp(type->valuestring, "asrInterrupt") == 0) {
            rt = __ai_mq_do_interrupt(data);
        } else if (strcmp(type->valuestring, "cloudReturnServerInfo") == 0) {
            if (__ai_mq_is_biz_id_vaild(bizId)) {
                rt = __ai_mq_do_ser_cfg(data);
            }
        } else if (strcmp(type->valuestring, "cloudReturnAgentToken") == 0) {
            if (__ai_mq_is_biz_id_vaild(bizId)) {
                rt = __ai_mq_do_agent_token(data);
            }
        }
    } else {
        rt = __ai_mq_do_text(data);
    }
    return rt;
}

VOID tuya_ai_mq_deinit(VOID)
{
    if (ai_mqtt_ctx) {
        mqc_app_unregister_cb(AI_MQ_PROTO_NUM, __ai_mq_handle);
        __ai_mq_ser_cfg_free();
        if (ai_mqtt_ctx->sem) {
            tal_semaphore_release(ai_mqtt_ctx->sem);
        }
        OS_FREE(ai_mqtt_ctx);
        ai_mqtt_ctx = NULL;
        PR_DEBUG("ai mq deinit");
    }
}

OPERATE_RET tuya_ai_mq_init(AI_MQTT_RECV_CB cb)
{
    OPERATE_RET rt = OPRT_OK;
    ai_mqtt_ctx = OS_MALLOC(SIZEOF(AI_MQTT_CTX_T));
    TUYA_CHECK_NULL_RETURN(ai_mqtt_ctx, OPRT_MALLOC_FAILED);
    memset(ai_mqtt_ctx, 0, SIZEOF(AI_MQTT_CTX_T));
    mqc_app_register_cb(AI_MQ_PROTO_NUM, __ai_mq_handle);
    TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&(ai_mqtt_ctx->sem), 0, 1), EXIT);
    ai_mqtt_ctx->recv_cb = cb;
    return rt;

EXIT:
    PR_ERR("ai mqtt init failed, rt:%d", rt);
    tuya_ai_mq_deinit();
    return rt;
}