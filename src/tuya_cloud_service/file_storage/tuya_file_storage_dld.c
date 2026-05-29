/**
 * @file tuya_file_storage_dld.c
 * @brief file storage download
 * @version 0.1
 * @date 2024-07-02
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
#include <stdlib.h>
#include <string.h>
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_fs.h"
#include "tal_kv.h"
#include "tal_symmetry.h"
#include "tal_workq_service.h"
#include "tal_event.h"
#include "tal_event_info.h"
#include "tal_network.h"
#include "mix_method.h"
#include "cJSON.h"
#include "tuya_iot.h"
#include "mqtt_service.h"
#include "atop_service.h"
#include "tuya_protocol.h"
#include "tuya_file_storage_dld.h"
#include "tuya_file_storage_com.h"
#include "tal_time_service.h"

#define ATOP_THING_TC_INIT       "thing.tc.init"
#define ATOP_THING_TC_CONFIG_GET "thing.tc.config.get"
#define MQ_FILE_STORAGE_DL       "fileStorageDl"
#define KV_FILE_DL_TC_KEY        "kv.file.dl.tc.key"
#define TC_MAX_FILE_COUNT        10
#define TC_MAX_PER_FILE_SIZE     (10 * 1024 * 1024) // 10M
#define DL_UNIT_LEN              (32 * 1024)

typedef OPERATE_RET (*FILE_DL_INIT)(void);
typedef struct {
    FILE_STORAGE_CONFIG_T config;
    FILE_DL_CONFIG_CB_T   app;
    uint8_t               tc_key[16];
    http_session_t        http_session;
    uint32_t              upd_fc;
    uint32_t              dld_fc;
    uint32_t              first_packet;
    uint8_t               iv[16];
    DELAYED_WORK_HANDLE   run_work;
} TY_FILE_DL_CONTEX;
static TY_FILE_DL_CONTEX s_file_dld_ctx;

static char **config_key_store[] = {&s_file_dld_ctx.config.provider, &s_file_dld_ctx.config.ak,
                                    &s_file_dld_ctx.config.sk,       &s_file_dld_ctx.config.bucket,
                                    &s_file_dld_ctx.config.endpoint, &s_file_dld_ctx.config.token,
                                    &s_file_dld_ctx.config.region,   &s_file_dld_ctx.config.pathconfig};

static int __dl_post_activate_event(void *data);

static OPERATE_RET __dl_mq_rept(cJSON *req_root, char *event, uint32_t code, char *name, char *errmsg)
{
    OPERATE_RET rt        = OPRT_OK;
    cJSON      *rept_root = NULL;

    if (NULL == req_root || NULL == event) {
        PR_ERR("invalid param: req_root=%p, event=%p", req_root, event);
        if (errmsg) {
            Free(errmsg);
        }
        return OPRT_INVALID_PARM;
    }

    rept_root = cJSON_CreateObject();
    if (NULL == rept_root) {
        if (errmsg) {
            Free(errmsg);
        }
        return OPRT_MALLOC_FAILED;
    }

    cJSON_AddStringToObject(rept_root, "reqType", MQ_FILE_STORAGE_DL);

    cJSON *sid = cJSON_GetObjectItem(req_root, "sid");
    if (cJSON_IsString(sid) && sid->valuestring) {
        cJSON_AddStringToObject(rept_root, "sid", sid->valuestring);
    }

    cJSON_AddStringToObject(rept_root, "event", event);

    cJSON *prefix = cJSON_GetObjectItem(req_root, "prefix");
    if (!cJSON_IsString(prefix) || NULL == prefix->valuestring) {
        PR_ERR("prefix invalid");
        cJSON_Delete(rept_root);
        if (errmsg) {
            Free(errmsg);
        }
        return OPRT_CJSON_GET_ERR;
    }
    cJSON_AddStringToObject(rept_root, "prefix", prefix->valuestring);

    cJSON_AddNumberToObject(rept_root, "code", code);
    if (errmsg) {
        cJSON_AddStringToObject(rept_root, "errmsg", errmsg);
    }
    if (name) {
        cJSON_AddStringToObject(rept_root, "realFn", name);
    }

    cJSON *meta = cJSON_GetObjectItem(req_root, "meta");
    if (cJSON_IsString(meta) && meta->valuestring && strlen(meta->valuestring)) {
        cJSON_AddStringToObject(rept_root, "meta", meta->valuestring);
    }

    char *body = cJSON_PrintUnformatted(rept_root);
    if (errmsg) {
        Free(errmsg);
    }
    cJSON_Delete(rept_root);
    TUYA_CHECK_NULL_RETURN(body, OPRT_MALLOC_FAILED);

    tuya_iot_client_t *client = tuya_iot_client_get();
    rt = tuya_mqtt_protocol_data_publish(&client->mqctx, PRO_DEV_DA_RESP, (const uint8_t *)body, strlen(body));
    PR_DEBUG("mq rept rt:%d", rt);
    Free(body);
    return rt;
}

static OPERATE_RET __dl_event_start(cJSON *root)
{
    OPERATE_RET rt          = OPRT_OK;
    cJSON      *prefix      = cJSON_GetObjectItem(root, "prefix");
    cJSON      *files_count = cJSON_GetObjectItem(root, "fc");
    TUYA_CHECK_NULL_RETURN(files_count, OPRT_CJSON_GET_ERR);
    FILE_DL_START_INFO_T start_data;
    memset(&start_data, 0, sizeof(FILE_DL_START_INFO_T));
    start_data.files_count = files_count->valueint;
    memcpy(start_data.prefix, prefix->valuestring, strlen(prefix->valuestring));

    char *errmsg = NULL;
    rt           = s_file_dld_ctx.app.notify_cb(FILE_DL_TYPE_START, &start_data, &errmsg);
    PR_DEBUG("app handle start rt:%d", rt);
    PR_DEBUG("start fc:%d", files_count->valueint);
    s_file_dld_ctx.dld_fc = 0;
    s_file_dld_ctx.upd_fc = 0;
    rt                    = __dl_mq_rept(root, "startAnswer", rt, NULL, errmsg);
    return rt;
}

static void __dl_storage_free_config(void)
{
    int idx = 0;
    for (idx = 0; idx < CNTSOF(config_key_store); idx++) {
        if (*config_key_store[idx]) {
            Free(*config_key_store[idx]);
        }
    }
    memset(&s_file_dld_ctx.config, 0, sizeof(s_file_dld_ctx.config));
}

OPERATE_RET __dl_get_storage_config(void)
{
    OPERATE_RET rt     = OPRT_OK;
    cJSON      *result = NULL, *child = NULL;
    int         idx = 0, vaule_len = 0;

    cJSON *root = cJSON_CreateObject();
    TUYA_CHECK_NULL_RETURN(root, OPRT_MALLOC_FAILED);
    cJSON_AddNumberToObject(root, "action", 1);
    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    TUYA_CHECK_NULL_RETURN(post_data, OPRT_MALLOC_FAILED);
    rt = atop_service_comm_post_simple(ATOP_THING_TC_CONFIG_GET, "2.0", post_data, NULL, &result);
    Free(post_data);
    if (OPRT_OK != rt) {
        PR_ERR("get file dl config err, rt:%d", rt);
        return rt;
    }

    char *key[] = {"provider", "ak", "sk", "bucket", "endpoint", "token", "region", "path"};
    for (idx = 0; idx < CNTSOF(config_key_store); idx++) {
        child = cJSON_GetObjectItem(result, key[idx]);
        if (!cJSON_IsString(child) || NULL == child->valuestring) {
            PR_ERR("fail to get value for key %s", key[idx]);
            rt = OPRT_CJSON_GET_ERR;
            goto EXIT;
        }

        vaule_len              = strlen(child->valuestring);
        *config_key_store[idx] = Malloc(vaule_len + 1);
        if (NULL == *config_key_store[idx]) {
            PR_ERR("Malloc err");
            rt = OPRT_MALLOC_FAILED;
            goto EXIT;
        }
        memset(*config_key_store[idx], 0, vaule_len + 1);
        memcpy(*config_key_store[idx], child->valuestring, vaule_len);

        PR_DEBUG("idx:%d,key:%s,len:%d,value:%s", idx, key[idx], vaule_len, *config_key_store[idx]);
    }

    cJSON_Delete(result);
    return rt;

EXIT:
    cJSON_Delete(result);
    __dl_storage_free_config();
    return rt;
}

static OPERATE_RET __dl_decode_and_send_to_app(uint8_t *data, uint32_t len, bool last)
{
    OPERATE_RET rt = OPRT_OK;

    if (0 == s_file_dld_ctx.first_packet) {
        // aes header 64 byte: ver[4] + iv[16] + resv[44]
        memcpy(s_file_dld_ctx.iv, data + 4, 16);
        s_file_dld_ctx.first_packet = 1;
        data += 64;
        len -= 64;
    }

    uint8_t *dec_data = NULL;
    uint32_t dec_len  = 0;
    rt                = tal_aes128_cbc_decode(data, len, s_file_dld_ctx.tc_key, s_file_dld_ctx.iv, &dec_data, &dec_len);
    if (OPRT_OK != rt) {
        PR_ERR("decode err,rt:%d", rt);
        return rt;
    }

    int actual_length = dec_len;
    if (last) {
        actual_length = tal_aes_get_actual_length(dec_data, dec_len);
        if (actual_length < 0) {
            PR_ERR("get actual length fail. %d", actual_length);
            Free(dec_data);
            return OPRT_COM_ERROR;
        }
    }

    FILE_DL_TRANS_INFO_T trans;
    memset(&trans, 0, sizeof(FILE_DL_TRANS_INFO_T));
    trans.len  = actual_length;
    trans.data = dec_data;
    rt         = s_file_dld_ctx.app.notify_cb(FILE_DL_TYPE_TRANS, &trans, NULL);

    Free(dec_data);
    return rt;
}

static OPERATE_RET __dl_http_receive_data(http_session_t session, http_resp_t *pResp)
{
    OPERATE_RET rt       = OPRT_OK;
    int         unit_len = s_file_dld_ctx.app.unit_size ? s_file_dld_ctx.app.unit_size : DL_UNIT_LEN;
    uint8_t    *buf      = Malloc(unit_len);
    TUYA_CHECK_NULL_RETURN(buf, OPRT_MALLOC_FAILED);
    memset(buf, 0, unit_len);

    int      read_len = 0, have_read_len = 0, sum_read_len = 0;
    uint32_t total_len = pResp->content_length;
    PR_DEBUG("file total len %d", total_len);
    while (1) {
        read_len = http_read_content(session, &buf[have_read_len], unit_len - have_read_len);
        if (read_len <= 0) {
            rt = OPRT_COM_ERROR;
            break;
        }

        have_read_len += read_len;
        if (sum_read_len + have_read_len >= (int)total_len) {
            rt = __dl_decode_and_send_to_app(buf, have_read_len, true);
            if (OPRT_OK != rt) {
                PR_ERR("decode and send to app err,rt:%d", rt);
            }
            break;
        } else {
            if (have_read_len < unit_len) {
                continue;
            }
        }

        rt = __dl_decode_and_send_to_app(buf, unit_len, false);
        if (OPRT_OK != rt) {
            PR_ERR("decode and send to app err,rt:%d", rt);
            break;
        }
        memset(buf, 0, unit_len);
        sum_read_len += have_read_len;
        have_read_len = 0;
    }

    Free(buf);
    s_file_dld_ctx.first_packet = 0;
    memset(s_file_dld_ctx.iv, 0, sizeof(s_file_dld_ctx.iv));
    return rt;
}

static OPERATE_RET __dl_file_storage_http_get(char *url)
{
    OPERATE_RET rt = OPRT_OK;

    rt = http_open_session(&s_file_dld_ctx.http_session, url, 10000);
    if (OPRT_OK != rt) {
        PR_ERR("http open session failed, rt:%d", rt);
        return OPRT_COM_ERROR;
    }

    FILE_STORAGE_HEADER_PARAM_T header_param;
    memset(&header_param, 0, sizeof(header_param));
    header_param.config = &s_file_dld_ctx.config;
    header_param.method = HTTP_GET;
    tuya_file_storage_build_headers(&header_param);

    http_req_t req = {
        .type                 = HTTP_GET,
        .version              = HTTP_VER_1_1,
        .custom_headers       = header_param.headers,
        .custom_headers_count = header_param.headers_count,
    };

    rt = http_send_request(s_file_dld_ctx.http_session, &req, 0);
    if (OPRT_OK != rt) {
        PR_ERR("http send request failed, rt:%d", rt);
        http_close_session(&s_file_dld_ctx.http_session);
        return OPRT_MID_HTTP_SD_REQ_ERROR;
    }

    http_resp_t *resp = NULL;
    rt                = http_get_response_hdr(s_file_dld_ctx.http_session, &resp);
    if ((OPRT_OK != rt) || (!resp) || (resp->status_code != 200 && resp->status_code != 201)) {
        PR_ERR("get fail %d,code %d", rt, resp ? resp->status_code : 0xff);
        if (resp) {
            http_free_response_hdr(&resp);
        }
        http_close_session(&s_file_dld_ctx.http_session);
        return OPRT_MID_HTTP_GET_RESP_ERROR;
    }

    // cbc encode should align 16
    if (resp->content_length % 16) {
        PR_ERR("content length %d not align 16", resp->content_length);
        http_free_response_hdr(&resp);
        http_close_session(&s_file_dld_ctx.http_session);
        return OPRT_INVALID_PARM;
    }

    if (0 == resp->content_length && !(resp->chunked)) {
        PR_ERR("http head err length %d, chunked %d", resp->content_length, resp->chunked);
        http_free_response_hdr(&resp);
        http_close_session(&s_file_dld_ctx.http_session);
        return OPRT_INVALID_PARM;
    }

    rt = __dl_http_receive_data(s_file_dld_ctx.http_session, resp);
    http_free_response_hdr(&resp);
    return rt;
}

static OPERATE_RET __dl_get_file_from_cloud(char *filename)
{
    OPERATE_RET rt      = OPRT_OK;
    char       *url     = NULL;
    int         url_len = 0;

    FILE_STORAGE_CONFIG_T *config = &s_file_dld_ctx.config;

    if ((0 == tuya_strncasecmp(config->provider, VENDOR_TENCENT_COS, strlen(VENDOR_TENCENT_COS))) ||
        (0 == tuya_strncasecmp(config->provider, VENDOR_AMAZON_S3, strlen(VENDOR_AMAZON_S3)))) {
        url_len =
            strlen(config->bucket) + strlen(config->endpoint) + strlen(config->pathconfig) + strlen(filename) + 64;
        url = Malloc(url_len);
        if (NULL == url) {
            PR_ERR("malloc url err %d", url_len);
            return OPRT_MALLOC_FAILED;
        }
        memset(url, 0, url_len);
        snprintf(url, url_len, "https://%s.%s%s/%s", config->bucket, config->endpoint, config->pathconfig, filename);
    } else if (0 == tuya_strncasecmp(config->provider, VENDOR_AZURE_BLOB, strlen(VENDOR_AZURE_BLOB))) {
        url_len = strlen(config->ak) + strlen(config->endpoint) + strlen(config->bucket) + strlen(config->pathconfig) +
                  strlen(filename) + strlen(config->token) + 64;
        url     = Malloc(url_len);
        if (NULL == url) {
            PR_ERR("malloc url err %d", url_len);
            return OPRT_MALLOC_FAILED;
        }
        memset(url, 0, url_len);
        snprintf(url, url_len, "https://%s%s/%s%s/%s%s", config->ak, config->endpoint, config->bucket,
                 config->pathconfig, filename, config->token);
    } else {
        PR_ERR("provider invaild :%s", config->provider);
        return OPRT_INVALID_PARM;
    }

    int object_len = strlen(config->pathconfig) + strlen(filename) + 8;
    config->object = Malloc(object_len);
    if (NULL == config->object) {
        PR_ERR("object malloc err");
        Free(url);
        return OPRT_MALLOC_FAILED;
    }
    memset(config->object, 0, object_len);
    snprintf(config->object, object_len, "%s/%s", config->pathconfig, filename);

    PR_DEBUG("url:%s", url);
    rt = __dl_file_storage_http_get(url);
    Free(url);
    Free(config->object);
    config->object = NULL;
    PR_DEBUG("http dl from cloud rt:%d %d", rt, tal_net_get_errno());

    return rt;
}

OPERATE_RET __dl_storage_action(char *filename)
{
    OPERATE_RET rt = OPRT_OK;

    rt             = __dl_get_storage_config();
    if (OPRT_OK != rt) {
        PR_ERR("get storage config err,rt:%d", rt);
        return rt;
    }

    rt = __dl_get_file_from_cloud(filename);
    if (OPRT_OK != rt) {
        PR_ERR("get file from cloud err,rt:%d", rt);
    }

    if (s_file_dld_ctx.http_session != NULL) {
        http_close_session(&s_file_dld_ctx.http_session);
    }

    __dl_storage_free_config();

    return rt;
}

static OPERATE_RET __dl_event_single_uploaded(cJSON *root)
{
    OPERATE_RET rt  = OPRT_OK;
    uint32_t    idx = 0;

    FILE_DL_TRANS_START_INFO_T start_info;
    memset(&start_info, 0, sizeof(FILE_DL_TRANS_START_INFO_T));

    cJSON *file_name = cJSON_GetObjectItem(root, "realFn");
    cJSON *frag_fn   = cJSON_GetObjectItem(root, "fragFn");
    TUYA_CHECK_NULL_RETURN(frag_fn, OPRT_CJSON_GET_ERR);
    cJSON *meta = cJSON_GetObjectItem(root, "meta");
    TUYA_CHECK_NULL_RETURN(meta, OPRT_CJSON_GET_ERR);
    cJSON *file_type = cJSON_GetObjectItem(root, "fileType");
    TUYA_CHECK_NULL_RETURN(file_type, OPRT_CJSON_GET_ERR);
    start_info.file_type = file_type->valueint;
    cJSON *title         = cJSON_GetObjectItem(root, "title");

    if (start_info.file_type == 0) {
        cJSON *center_x = cJSON_GetObjectItem(root, "centerX");
        cJSON *center_y = cJSON_GetObjectItem(root, "centerY");
        if (center_x) {
            start_info.center_x = center_x->valueint;
        }
        if (center_y) {
            start_info.center_y = center_y->valueint;
        }
    }

    if (cJSON_IsString(title) && title->valuestring && strlen(title->valuestring)) {
        size_t title_len = strlen(title->valuestring);
        start_info.title = Malloc(title_len + 1);
        TUYA_CHECK_NULL_GOTO(start_info.title, EXIT);
        memset(start_info.title, 0, title_len + 1);
        memcpy(start_info.title, title->valuestring, title_len);
    }
    if (cJSON_IsString(file_name) && file_name->valuestring && strlen(file_name->valuestring)) {
        size_t fn_len = strlen(file_name->valuestring);
        start_info.file_name = Malloc(fn_len + 1);
        TUYA_CHECK_NULL_GOTO(start_info.file_name, EXIT);
        memset(start_info.file_name, 0, fn_len + 1);
        memcpy(start_info.file_name, file_name->valuestring, fn_len);
    }
    if (cJSON_IsString(meta) && meta->valuestring) {
        snprintf(start_info.meta, sizeof(start_info.meta), "%s", meta->valuestring);
    } else {
        start_info.meta[0] = '\0';
    }
    rt = s_file_dld_ctx.app.notify_cb(FILE_DL_TYPE_TRANS_START, &start_info, NULL);

    uint32_t frag_num = cJSON_GetArraySize(frag_fn);
    for (idx = 0; idx < frag_num; idx++) {
        cJSON *cloud_file_name = cJSON_GetArrayItem(frag_fn, idx);
        if (!cJSON_IsString(cloud_file_name) || NULL == cloud_file_name->valuestring) {
            PR_ERR("frag_fn[%u] invalid", idx);
            rt = OPRT_CJSON_GET_ERR;
            break;
        }
        rt = __dl_storage_action(cloud_file_name->valuestring);
        if (rt != OPRT_OK) {
            PR_ERR("__dl_storage_action rt:%d", rt);
            break;
        }
    }

    PR_DEBUG("notify tans end rt:%d", rt);
    __dl_mq_rept(root, "downloaded", rt, NULL, NULL);

    FILE_DL_TRANS_END_INFO_T trans_end = {0};
    trans_end.ret                      = rt;
    s_file_dld_ctx.app.notify_cb(FILE_DL_TYPE_TRANS_END, &trans_end, NULL);

    s_file_dld_ctx.dld_fc++;
    PR_DEBUG("dl fc:%d, upd fc:%d", s_file_dld_ctx.dld_fc, s_file_dld_ctx.upd_fc);

EXIT:
    if (start_info.title) {
        Free(start_info.title);
    }
    if (start_info.file_name) {
        Free(start_info.file_name);
    }
    return rt;
}

// app all files upload finish
static OPERATE_RET __dl_event_uploaded_all_finished(cJSON *root)
{
    OPERATE_RET rt = OPRT_OK;
    if (0 == s_file_dld_ctx.dld_fc) {
        PR_ERR("no file download");
        return rt;
    }
    cJSON *meta = cJSON_GetObjectItem(root, "meta");
    TUYA_CHECK_NULL_RETURN(meta, OPRT_CJSON_GET_ERR);
    cJSON *succFc = cJSON_GetObjectItem(root, "succFc");
    TUYA_CHECK_NULL_RETURN(succFc, OPRT_CJSON_GET_ERR);
    s_file_dld_ctx.upd_fc = succFc->valueint;
    PR_DEBUG("app upd fc:%d, dl fc:%d", s_file_dld_ctx.upd_fc, s_file_dld_ctx.dld_fc);

    __dl_mq_rept(root, "downloadFinish", 0, NULL, NULL);
    FILE_DL_END_INFO_T end_info = {0};
    end_info.upd_fc             = s_file_dld_ctx.upd_fc;
    end_info.dld_fc             = s_file_dld_ctx.dld_fc;
    s_file_dld_ctx.app.notify_cb(FILE_DL_TYPE_END, &end_info, NULL);
    return rt;
}

static OPERATE_RET __dl_event_heart_beat(cJSON *root)
{
    OPERATE_RET rt = OPRT_OK;
    rt             = __dl_mq_rept(root, "heartBeat", rt, NULL, NULL);
    return rt;
}

static OPERATE_RET __dl_event_query(cJSON *root)
{
    OPERATE_RET rt      = OPRT_OK;
    uint32_t    idx     = 0;
    cJSON      *real_fn = cJSON_GetObjectItem(root, "realFn");
    TUYA_CHECK_NULL_RETURN(real_fn, OPRT_CJSON_GET_ERR);

    uint32_t fn_num = cJSON_GetArraySize(real_fn);
    for (idx = 0; idx < fn_num; idx++) {
        cJSON                   *file_name = cJSON_GetArrayItem(real_fn, idx);
        FILE_DL_QUERY_DEL_INFO_T query     = {0};
        query.file_name                    = Malloc(strlen(file_name->valuestring) + 1);
        TUYA_CHECK_NULL_RETURN(query.file_name, OPRT_MALLOC_FAILED);
        memset(query.file_name, 0, strlen(file_name->valuestring) + 1);
        memcpy(query.file_name, file_name->valuestring, strlen(file_name->valuestring));
        rt = s_file_dld_ctx.app.notify_cb(FILE_DL_TYPE_QUERY, &query, NULL);
        Free(query.file_name);
        __dl_mq_rept(root, "query", rt, file_name->valuestring, NULL);
    }

    return rt;
}

static OPERATE_RET __dl_event_delete(cJSON *root)
{
    OPERATE_RET rt      = OPRT_OK;
    uint32_t    idx     = 0;
    cJSON      *real_fn = cJSON_GetObjectItem(root, "realFn");
    TUYA_CHECK_NULL_RETURN(real_fn, OPRT_CJSON_GET_ERR);

    uint32_t fn_num = cJSON_GetArraySize(real_fn);
    for (idx = 0; idx < fn_num; idx++) {
        cJSON                   *file_name = cJSON_GetArrayItem(real_fn, idx);
        FILE_DL_QUERY_DEL_INFO_T del_info  = {0};
        del_info.file_name                 = Malloc(strlen(file_name->valuestring) + 1);
        TUYA_CHECK_NULL_RETURN(del_info.file_name, OPRT_MALLOC_FAILED);
        memset(del_info.file_name, 0, strlen(file_name->valuestring) + 1);
        memcpy(del_info.file_name, file_name->valuestring, strlen(file_name->valuestring));
        rt = s_file_dld_ctx.app.notify_cb(FILE_DL_TYPE_DELETE, &del_info, NULL);
        Free(del_info.file_name);
        __dl_mq_rept(root, "delete", rt, file_name->valuestring, NULL);
    }

    return rt;
}

static void __dl_mq_work_cb(void *msg)
{
    char  *root_str = (char *)msg;
    PR_NOTICE("root_str:%s", root_str);
    cJSON *root     = cJSON_Parse(root_str);
    Free(root_str);
    if (NULL == root) {
        PR_ERR("cjson parse err");
        return;
    }

    cJSON *event = cJSON_GetObjectItem(root, "event");
    if (!cJSON_IsString(event) || NULL == event->valuestring) {
        PR_ERR("event invalid");
        cJSON_Delete(root);
        return;
    }
    PR_DEBUG("dl mq app handle event %s", event->valuestring);
    if (!strcmp(event->valuestring, "start")) {
        __dl_event_start(root);
    } else if (!strcmp(event->valuestring, "uploaded")) {
        __dl_event_single_uploaded(root);
    } else if (!strcmp(event->valuestring, "uploadFinished")) {
        __dl_event_uploaded_all_finished(root);
    } else if (!strcmp(event->valuestring, "heartBeat")) {
        __dl_event_heart_beat(root);
    } else if (!strcmp(event->valuestring, "query")) {
        __dl_event_query(root);
    } else if (!strcmp(event->valuestring, "delete")) {
        __dl_event_delete(root);
    }

    cJSON_Delete(root);
    return;
}

static void __dl_mq_protocol_cb(tuya_protocol_event_t *ev)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == ev || NULL == ev->root_json) {
        return;
    }

    cJSON *data     = ev->data ? ev->data : ev->root_json;
    cJSON *req_type = cJSON_GetObjectItem(data, "reqType");
    if (NULL == req_type || strcmp(req_type->valuestring, MQ_FILE_STORAGE_DL) != 0) {
        return;
    }

    cJSON *sid    = cJSON_GetObjectItem(data, "sid");
    cJSON *prefix = cJSON_GetObjectItem(data, "prefix");
    cJSON *event  = cJSON_GetObjectItem(data, "event");
    if (NULL == sid || NULL == prefix || NULL == event) {
        PR_ERR("get cjson error");
        return;
    }

    if (strlen(prefix->valuestring) > 16) {
        PR_ERR("prefix too long %d", (int)strlen(prefix->valuestring));
        return;
    }
    char *root_str = cJSON_PrintUnformatted(data);
    if (NULL == root_str) {
        return;
    }

    rt = tal_workq_schedule(WORKQ_SYSTEM, __dl_mq_work_cb, root_str);
    if (OPRT_OK != rt) {
        Free(root_str);
        PR_ERR("schedule workq err,rt:%d", rt);
    }
}

static int __dl_post_activate_event(void *data)
{
    OPERATE_RET rt    = OPRT_OK;

    cJSON      *skill = cJSON_CreateObject();
    TUYA_CHECK_NULL_RETURN(skill, OPRT_MALLOC_FAILED);
    cJSON_AddStringToObject(skill, "sdkVer", TUYA_BSV);
    cJSON_AddStringToObject(skill, "resolution", s_file_dld_ctx.app.resolution);
    cJSON_AddNumberToObject(skill, "maxFs",
                            s_file_dld_ctx.app.max_per_file_size ? s_file_dld_ctx.app.max_per_file_size
                                                                 : TC_MAX_PER_FILE_SIZE);
    cJSON_AddNumberToObject(skill, "maxFc",
                            s_file_dld_ctx.app.max_file_cnt ? s_file_dld_ctx.app.max_file_cnt : TC_MAX_FILE_COUNT);
    cJSON_AddStringToObject(skill, "checksumAlg", "SHA-1");

    cJSON   *allow_formats = cJSON_CreateArray();
    uint32_t idx           = 0;
    for (idx = 0; idx < 8; idx++) {
        if (strlen(s_file_dld_ctx.app.allow_formats[idx])) {
            cJSON_AddItemToArray(allow_formats, cJSON_CreateString(s_file_dld_ctx.app.allow_formats[idx]));
        }
    }
    cJSON_AddItemToObject(skill, "allowFormats", allow_formats);
    char *skill_value = cJSON_PrintUnformatted(skill);
    cJSON_Delete(skill);
    TUYA_CHECK_NULL_RETURN(skill_value, OPRT_MALLOC_FAILED);

    cJSON *root = cJSON_CreateObject();
    TUYA_CHECK_NULL_RETURN(root, OPRT_MALLOC_FAILED);
    cJSON_AddStringToObject(root, "skill", skill_value);
    Free(skill_value);
    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    TUYA_CHECK_NULL_RETURN(post_data, OPRT_MALLOC_FAILED);
    cJSON *result = NULL;
    rt            = atop_service_comm_post_simple(ATOP_THING_TC_INIT, "2.0", post_data, NULL, &result);
    Free(post_data);
    if (OPRT_OK != rt) {
        PR_ERR("get tc key err,rt:%d", rt);
        return rt;
    }

    cJSON *tc_key = cJSON_GetObjectItem(result, "tckey");
    TUYA_CHECK_NULL_RETURN(tc_key, OPRT_CJSON_GET_ERR);
    PR_DEBUG("tckey:%s", tc_key->valuestring);
    tuya_base64_decode(tc_key->valuestring, s_file_dld_ctx.tc_key);
    rt = tal_kv_set(KV_FILE_DL_TC_KEY, (const uint8_t *)s_file_dld_ctx.tc_key, sizeof(s_file_dld_ctx.tc_key));
    cJSON_Delete(result);
    PR_DEBUG("save tc key rt:%d", rt);
    return rt;
}

static void __dl_run_work_cb(void *data)
{
    OPERATE_RET        rt     = OPRT_OK;
    tuya_iot_client_t *client = tuya_iot_client_get();
    if (!tuya_iot_activated(client)) {
        goto EXIT;
    }

    TUYA_CALL_ERR_LOG(tuya_mqtt_protocol_register(&client->mqctx,
                                                  PRO_MQ_APP_PROTOCOL_RX,
                                                  __dl_mq_protocol_cb,
                                                  NULL));

    uint8_t *out     = NULL;
    size_t   out_len = 0;
    rt               = tal_kv_get(KV_FILE_DL_TC_KEY, &out, &out_len);
    if (OPRT_OK != rt) {
        PR_ERR("tal_kv_get rt %s %d", KV_FILE_DL_TC_KEY, rt);
        if (OPRT_OK == tal_time_check_time_sync()) {
            TUYA_CALL_ERR_LOG(__dl_post_activate_event(NULL));
        } else {
            TUYA_CALL_ERR_LOG(tal_event_subscribe(EVENT_TIME_SYNC, "file_dl_tc",
                                                  __dl_post_activate_event,
                                                  SUBSCRIBE_TYPE_ONETIME));
        }
        goto EXIT;
    }
    memcpy(s_file_dld_ctx.tc_key, out, out_len);
    tal_kv_free(out);
    PR_DEBUG("restore tc success");

EXIT:
    tal_workq_cancel_delayed(s_file_dld_ctx.run_work);
    s_file_dld_ctx.run_work = NULL;
}

static int __dl_run_event(void *data)
{
    if (s_file_dld_ctx.run_work) {
        return OPRT_OK;
    }

    OPERATE_RET rt = tal_workq_init_delayed(WORKQ_SYSTEM, __dl_run_work_cb, NULL, &s_file_dld_ctx.run_work);
    if (OPRT_OK != rt) {
        PR_ERR("init run work err, rt:%d", rt);
        return rt;
    }

    rt = tal_workq_start_delayed(s_file_dld_ctx.run_work, 0, LOOP_ONCE);
    if (OPRT_OK != rt) {
        PR_ERR("start run work err, rt:%d", rt);
        tal_workq_cancel_delayed(s_file_dld_ctx.run_work);
        s_file_dld_ctx.run_work = NULL;
    }
    return rt;
}

static int __dl_reset_event(void *data)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_file_dld_ctx.run_work) {
        tal_workq_cancel_delayed(s_file_dld_ctx.run_work);
        s_file_dld_ctx.run_work = NULL;
    }

    rt = tal_kv_del(KV_FILE_DL_TC_KEY);
    PR_DEBUG("delete tc key rt:%d", rt);
    return rt;
}

static OPERATE_RET __dl_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(tal_event_subscribe(EVENT_LINK_ACTIVATE, "file_dl", __dl_post_activate_event, SUBSCRIBE_TYPE_NORMAL));
    TUYA_CALL_ERR_LOG(tal_event_subscribe(EVENT_MQTT_CONNECTED, "file_dl", __dl_run_event, SUBSCRIBE_TYPE_NORMAL));
    TUYA_CALL_ERR_LOG(tal_event_subscribe(EVENT_RESET, "file_dl", __dl_reset_event, SUBSCRIBE_TYPE_NORMAL));
    PR_DEBUG("file storage dl init success");

    return OPRT_OK;
}

uint8_t *tuya_file_storage_get_tc_key(void)
{
    return s_file_dld_ctx.tc_key;
}

OPERATE_RET tuya_file_storage_dl_set_config(FILE_DL_CONFIG_CB_T app)
{
    if (!strlen(app.resolution) || !strlen(app.allow_formats[0]) || !app.notify_cb) {
        PR_ERR("paras empty, err");
        return OPRT_INVALID_PARM;
    }
    memcpy(&s_file_dld_ctx.app, &app, sizeof(FILE_DL_CONFIG_CB_T));
    return __dl_init();
}

OPERATE_RET tuya_file_storage_dl_mq_rept(char *prefix, char *name, char *meta, char *event)
{
    OPERATE_RET rt       = OPRT_OK;
    cJSON      *req_root = cJSON_CreateObject();
    TUYA_CHECK_NULL_RETURN(req_root, OPRT_MALLOC_FAILED);
    cJSON_AddStringToObject(req_root, "prefix", prefix);
    cJSON_AddStringToObject(req_root, "meta", meta);
    rt = __dl_mq_rept(req_root, event, 0, name, NULL);
    cJSON_Delete(req_root);
    return rt;
}
