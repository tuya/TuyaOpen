/**
 * @file tuya_file_storage_upd.c
 * @brief file storage upload
 * @version 0.1
 * @date 2024-04-02
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
#include "tal_mutex.h"
#include "tal_fs.h"
#include "tal_hash.h"
#include "tal_symmetry.h"
#include "tal_sw_timer.h"
#include "tal_system.h"
#include "tal_event.h"
#include "tal_event_info.h"
#include "tal_network.h"
#include "uni_random.h"
#include "cJSON.h"
#include "tuya_iot.h"
#include "mqtt_service.h"
#include "atop_service.h"
#include "tuya_protocol.h"
#include "tuya_file_storage_upd.h"
#include "tuya_file_storage_com.h"

#define TI_LOG_STORAGE_CONFIG_GET "thing.device.log.storage.config.get"
#define TI_TC_STORAGE_CONFIG_GET  "thing.tc.storage.config.get"
#define TI_TC_UPLOAD_URL_SAVE     "thing.tc.upload.url.save"

#ifndef HTTP_SEND_INTERVAL
#define HTTP_SEND_INTERVAL (100)
#endif
#ifndef PACKET_SIZE_PER_HTTP_SEND
#define PACKET_SIZE_PER_HTTP_SEND (300 * 1024)
#endif
#ifndef MAX_UPD_FILE_SIZE
#define MAX_UPD_FILE_SIZE (5 * 1024 * 1024)
#endif

typedef struct {
    bool                  stop;
    http_session_t        http_session;
    FILE_STORAGE_CONFIG_T config;
    FILE_STORAGE_BIZ_TYPE biz_type;
    MUTEX_HANDLE          mutex;
    uint32_t              total_len;
    uint32_t              offset;
    http_session_t        raw_session; // session for streaming raw upload
    uint8_t               encrypt_iv[16];
    TIMER_ID              tid;
    bool                  expired;
} FILE_STORAGE_CTRL_T;

static FILE_STORAGE_CTRL_T file_storage_ctrl;

static char **config_key_store[] = {&file_storage_ctrl.config.provider, &file_storage_ctrl.config.ak,
                                    &file_storage_ctrl.config.sk,       &file_storage_ctrl.config.bucket,
                                    &file_storage_ctrl.config.endpoint, &file_storage_ctrl.config.token,
                                    &file_storage_ctrl.config.region,   &file_storage_ctrl.config.pathconfig};

static void __file_storage_free_config(void)
{
    int idx = 0;
    for (idx = 0; idx < CNTSOF(config_key_store); idx++) {
        if (*config_key_store[idx]) {
            tal_free(*config_key_store[idx]);
        }
    }
    memset(&file_storage_ctrl.config, 0, sizeof(file_storage_ctrl.config));
}

static char *__file_storage_get_biz_type(void)
{
    if (BIZ_TYPE_LOG == file_storage_ctrl.biz_type) {
        return "log";
    } else if (BIZ_TYPE_NILM == file_storage_ctrl.biz_type) {
        return "nilm";
    } else if (BIZ_TYPE_AI_CAMERA == file_storage_ctrl.biz_type) {
        return "AIKidsCamera";
    }
    return NULL;
}

static OPERATE_RET __file_storage_get_config(void)
{
    OPERATE_RET rt     = OPRT_OK;
    cJSON      *result = NULL, *child = NULL;
    int         idx = 0, vaule_len = 0;
    int         post_data_len = 64;
    char       *post_data     = NULL;

    if (file_storage_ctrl.config.token && file_storage_ctrl.config.bucket && !file_storage_ctrl.expired) {
        return rt;
    }

    __file_storage_free_config();
    file_storage_ctrl.expired = false;
    if (tal_sw_timer_is_running(file_storage_ctrl.tid)) {
        tal_sw_timer_stop(file_storage_ctrl.tid);
    }

    if (0 == strcmp(__file_storage_get_biz_type(), "log")) {
        rt = atop_service_comm_post_simple(TI_LOG_STORAGE_CONFIG_GET, "1.0", NULL, NULL, &result);
    } else if (0 == strcmp(__file_storage_get_biz_type(), "nilm")) {
        post_data = tal_malloc(post_data_len);
        if (post_data == NULL) {
            PR_ERR("malloc err");
            return OPRT_MALLOC_FAILED;
        }
        memset(post_data, 0, post_data_len);
        snprintf(post_data, post_data_len, "{\"bizType\":\"%s\"}", __file_storage_get_biz_type());
        rt = atop_service_comm_post_simple(TI_LOG_STORAGE_CONFIG_GET, "1.1", post_data, NULL, &result);
        tal_free(post_data);
    } else if (0 == strcmp(__file_storage_get_biz_type(), "AIKidsCamera")) {
        post_data = tal_malloc(post_data_len);
        if (post_data == NULL) {
            PR_ERR("malloc err");
            return OPRT_MALLOC_FAILED;
        }
        memset(post_data, 0, post_data_len);
        snprintf(post_data, post_data_len, "{\"type\":\"%s\"}", __file_storage_get_biz_type());
        rt = atop_service_comm_post_simple(TI_TC_STORAGE_CONFIG_GET, "1.0", post_data, NULL, &result);
        tal_free(post_data);
    } else {
        PR_ERR("biz type err");
        return OPRT_COM_ERROR;
    }
    if (OPRT_OK != rt) {
        PR_ERR("get stroage config err,rt:%d", rt);
        return rt;
    }

    char *key[] = {"provider", "ak", "sk", "bucket", "endpoint", "token", "region", "pathConfig"};
    for (idx = 0; idx < CNTSOF(config_key_store); idx++) {
        child = cJSON_GetObjectItem(result, key[idx]);
        if (NULL == child) {
            PR_ERR("fail to get value for key %s", key[idx]);
            rt = OPRT_CJSON_GET_ERR;
            goto EXIT;
        }
        vaule_len              = strlen(child->valuestring);
        *config_key_store[idx] = tal_malloc(vaule_len + 1);
        if (NULL == *config_key_store[idx]) {
            PR_ERR("tal_malloc err");
            goto EXIT;
        }
        memset(*config_key_store[idx], 0, vaule_len + 1);
        strncpy(*config_key_store[idx], child->valuestring, vaule_len);
        PR_DEBUG("idx:%d,key:%s,len:%d,value:%s", idx, key[idx], vaule_len, *config_key_store[idx]);
    }

    child = cJSON_GetObjectItem(result, "expire");
    if (child) {
        uint32_t time_diff = child->valueint - tal_time_get_posix() - 60;
        PR_DEBUG("key:expire,value:%u, diff:%d", child->valueint, time_diff);
        tal_sw_timer_start(file_storage_ctrl.tid, time_diff * 1000, TAL_TIMER_ONCE);
    }

    cJSON_Delete(result);
    return rt;

EXIT:
    cJSON_Delete(result);
    __file_storage_free_config();
    return rt;
}

static OPERATE_RET __file_storage_http_put(char *url, char *data, int len)
{
    OPERATE_RET rt = OPRT_OK;

    rt = http_open_session(&file_storage_ctrl.http_session, url, 10000);
    if (OPRT_OK != rt) {
        PR_ERR("http open session failed, rt:%d", rt);
        return OPRT_COM_ERROR;
    }

    FILE_STORAGE_HEADER_PARAM_T header_param;
    memset(&header_param, 0, sizeof(header_param));
    header_param.config = &file_storage_ctrl.config;
    header_param.method = HTTP_PUT;
    tuya_file_storage_build_headers(&header_param);

    http_req_t req = {.type                 = HTTP_PUT,
                      .version              = HTTP_VER_1_1,
                      .custom_headers       = header_param.headers,
                      .custom_headers_count = header_param.headers_count,
                      .content              = data,
                      .content_len          = len};

    rt = http_send_request(file_storage_ctrl.http_session, &req, 0);
    if (OPRT_OK != rt) {
        PR_ERR("http send request failed, rt:%d", rt);
        http_close_session(&file_storage_ctrl.http_session);
        return OPRT_MID_HTTP_SD_REQ_ERROR;
    }

    http_resp_t *resp = NULL;
    rt                = http_get_response_hdr(file_storage_ctrl.http_session, &resp);
    if ((OPRT_OK != rt) || (!resp) || (resp->status_code != 200 && resp->status_code != 201)) {
        PR_ERR("put fail %d,code %d", rt, resp ? resp->status_code : 0xff);
        rt = OPRT_MID_HTTP_GET_RESP_ERROR;
    }
    if (resp) {
        http_free_response_hdr(&resp);
    }
    return rt;
}

static OPERATE_RET __file_storage_make_url(char *file, int pack_num, int sque, char **out_url)
{
    OPERATE_RET rt                              = OPRT_OK;
    char        fileTmp[UPD_FILE_NAME_LEN + 16] = {0};
    char       *url                             = NULL;
    int         url_len                         = 0;
    PR_DEBUG("sque:%d", sque);

    if (pack_num == 1) {
        snprintf(fileTmp, UPD_FILE_NAME_LEN + 16, "%s", file);
    } else {
        snprintf(fileTmp, UPD_FILE_NAME_LEN + 16, "%s%d", file, sque);
    }

    FILE_STORAGE_CONFIG_T *config = &file_storage_ctrl.config;
    if ((0 == strncasecmp(config->provider, VENDOR_TENCENT_COS, strlen(VENDOR_TENCENT_COS))) ||
        (0 == strncasecmp(config->provider, VENDOR_AMAZON_S3, strlen(VENDOR_AMAZON_S3)))) {
        url_len = strlen(config->bucket) + strlen(config->endpoint) + strlen(config->pathconfig) + strlen(fileTmp) + 64;
        url     = tal_malloc(url_len);
        if (NULL == url) {
            PR_ERR("malloc url err %d", url_len);
            return OPRT_MALLOC_FAILED;
        }
        memset(url, 0, url_len);
        snprintf(url, url_len, "https://%s.%s%s/%s", config->bucket, config->endpoint, config->pathconfig, fileTmp);
    } else if (0 == strncasecmp(config->provider, VENDOR_AZURE_BLOB, strlen(VENDOR_AZURE_BLOB))) {
        url_len = strlen(config->ak) + strlen(config->endpoint) + strlen(config->bucket) + strlen(config->pathconfig) +
                  strlen(fileTmp) + strlen(config->token) + 64;
        url     = tal_malloc(url_len);
        if (NULL == url) {
            PR_ERR("malloc url err %d", url_len);
            return OPRT_MALLOC_FAILED;
        }
        memset(url, 0, url_len);
        snprintf(url, url_len, "https://%s%s/%s%s/%s%s", config->ak, config->endpoint, config->bucket,
                 config->pathconfig, fileTmp, config->token);
    } else {
        PR_ERR("provider invaild :%s", config->provider);
        return OPRT_INVALID_PARM;
    }

    if (config->object) {
        tal_free(config->object);
        config->object = NULL;
    }
    int object_len = strlen(config->pathconfig) + strlen(fileTmp) + 8;
    config->object = tal_malloc(object_len);
    if (NULL == config->object) {
        PR_ERR("object malloc err");
        tal_free(url);
        return OPRT_MALLOC_FAILED;
    }
    memset(config->object, 0, object_len);
    snprintf(config->object, object_len, "%s/%s", config->pathconfig, fileTmp);
    *out_url = url;
    return rt;
}

static OPERATE_RET __file_storage_upd_to_cloud(char *file, char *data, int len, int pack_num, int sque)
{
    OPERATE_RET rt  = OPRT_OK;
    char       *url = NULL;
    rt              = __file_storage_make_url(file, pack_num, sque, &url);
    if ((OPRT_OK != rt) || (NULL == url)) {
        PR_ERR("make url err, %d", rt);
        return rt;
    }

    PR_DEBUG("url:%s", url);
    rt = __file_storage_http_put(url, data, len);
    tal_free(url);
    PR_DEBUG("http upd to cloud rt:%d %d", rt, tal_net_get_errno());

    return rt;
}

static int __file_storage_upd_stop(void *data)
{
    file_storage_ctrl.stop = true;
    return OPRT_OK;
}

static void __file_storage_expire_timer(TIMER_ID timerID, void *pTimerArg)
{
    PR_NOTICE("storage token expired");
    file_storage_ctrl.expired = true;
}

static void __file_storage_init(void)
{
    if (NULL != file_storage_ctrl.mutex) {
        return;
    }
    tal_mutex_create_init(&file_storage_ctrl.mutex);
    tal_sw_timer_create(__file_storage_expire_timer, NULL, &file_storage_ctrl.tid);
    // Note: EVENT_FILE_UPD_STOP not defined in current project; use a custom event name
    tal_event_subscribe("file.upd.stop", "file_upd", __file_storage_upd_stop, SUBSCRIBE_TYPE_NORMAL);
}

static OPERATE_RET __file_storage_open_file(char *local_file, int *size, TUYA_FILE **fp)
{
    int        file_size = 0;
    TUYA_FILE *file_fp   = NULL;

    if (local_file[0] == 0) {
        PR_ERR("file name invaild");
        return OPRT_COM_ERROR;
    }

    if (tal_faccess(local_file, TUYA_R_OK) < 0) {
        PR_ERR("file not exist:%s", local_file);
        return OPRT_NOT_EXIST;
    }

    file_size = tal_fgetsize(local_file);
    if (file_size > MAX_UPD_FILE_SIZE) {
        PR_ERR("%s size too large %d", local_file, file_size);
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    file_fp = tal_fopen(local_file, "rb");
    if (NULL == file_fp) {
        PR_ERR("open failed");
        return OPRT_FILE_OPEN_FAILED;
    }

    *fp   = file_fp;
    *size = file_size;
    return OPRT_OK;
}

static void __file_storage_mqc_get_log_suf(int pack_num, int sque, char *log_suf)
{
    int   idx = 0, suf_len = pack_num * 3;
    char *log_suf_backup = NULL;
    log_suf_backup       = tal_malloc(suf_len);
    if (NULL == log_suf_backup) {
        PR_ERR("malloc suf backup err");
        return;
    }
    memset(log_suf_backup, 0, suf_len);
    for (idx = 1; idx <= sque; idx++) {
        if (1 == idx) {
            sprintf(log_suf, "%d", idx);
        } else {
            strncpy(log_suf_backup, log_suf, strlen(log_suf));
            snprintf(log_suf, suf_len, "%s,%d", log_suf_backup, idx);
        }
    }
    tal_free(log_suf_backup);
}

static OPERATE_RET __file_storage_mqc_notify(int pack_num, int sque_num, char *remote_file)
{
    FILE_STORAGE_CONFIG_T *config = &file_storage_ctrl.config;

    int   suf_len = pack_num * 3, resp_len = 0;
    char *log_suf                             = NULL;
    char *p_resp                              = NULL;
    char  url[ST_URL_LEN + UPD_FILE_NAME_LEN] = {0};

    sque_num -= 1;
    log_suf = tal_malloc(suf_len);
    TUYA_CHECK_NULL_GOTO(log_suf, EXIT);
    memset(log_suf, 0, suf_len);
    __file_storage_mqc_get_log_suf(pack_num, sque_num, log_suf);

    snprintf(url, ST_URL_LEN + UPD_FILE_NAME_LEN, "%s%s", config->pathconfig, remote_file);

    tuya_iot_client_t *client = tuya_iot_client_get();
    const char        *dev_id = tuya_iot_devid_get(client);

    resp_len = suf_len + strlen(config->bucket) + strlen(url) + 512;
    p_resp   = tal_malloc(resp_len);
    TUYA_CHECK_NULL_GOTO(p_resp, EXIT);
    memset(p_resp, 0, resp_len);
    snprintf(p_resp, resp_len,
             "{\"success\":%s,\"devId\":\"%s\",\"logUrl\":\"%s\",\"logSuf\":\"%s\",\"bucket\":\"%s\", \"source\":%d, "
             "\"bizType\":\"%s\"}",
             "true", dev_id, url, log_suf, config->bucket, 1, __file_storage_get_biz_type());
    tuya_mqtt_protocol_data_publish(&client->mqctx, PRO_GW_UPLOAD_LOG, (const uint8_t *)p_resp, strlen(p_resp));

EXIT:
    if (log_suf) {
        tal_free(log_suf);
    }
    if (p_resp) {
        tal_free(p_resp);
    }
    return OPRT_OK;
}

static OPERATE_RET __file_storage_action(char *local_file)
{
    OPERATE_RET rt                             = OPRT_COM_ERROR;
    char        remote_file[UPD_FILE_NAME_LEN] = {0};
    char       *read_buffer                    = NULL;
    TUYA_FILE  *fp                             = NULL;
    char       *remote_name_suf                = NULL;
    POSIX_TM_S  tm;
    int         pack_num = 0, sque_num = 1, one_pack_len = 0, pos = 0, size = 0;

    rt = __file_storage_open_file(local_file, &size, &fp);
    if (OPRT_OK != rt) {
        goto EXIT;
    }

    rt = __file_storage_get_config();
    if (OPRT_OK != rt) {
        goto EXIT;
    }

    rt          = OPRT_COM_ERROR;
    read_buffer = (char *)tal_malloc(PACKET_SIZE_PER_HTTP_SEND);
    TUYA_CHECK_NULL_GOTO(read_buffer, EXIT);

    pack_num = size / PACKET_SIZE_PER_HTTP_SEND + 1;
    PR_DEBUG("total size:%d,pack_num:%d", size, pack_num);
    memset(&tm, 0, sizeof(tm));
    tal_time_get_local_time_custom(0, &tm);
    remote_name_suf = strrchr(local_file, '/');
    if (NULL != remote_name_suf) {
        remote_name_suf++;
    } else {
        remote_name_suf = local_file;
    }

    tuya_iot_client_t *client = tuya_iot_client_get();
    const char        *dev_id = tuya_iot_devid_get(client);
    snprintf(remote_file, UPD_FILE_NAME_LEN, "/%s_%d%02d%02d%02d%02d%02d.%s", dev_id, tm.tm_year + 1900, tm.tm_mon + 1,
             tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, remote_name_suf);

    while (pos < size) {
        memset(read_buffer, 0, PACKET_SIZE_PER_HTTP_SEND);
        one_pack_len = tal_fread(read_buffer, PACKET_SIZE_PER_HTTP_SEND, fp);
        if ((one_pack_len > PACKET_SIZE_PER_HTTP_SEND) || (one_pack_len < 0)) {
            PR_ERR("read err %d %d", one_pack_len, tal_net_get_errno());
            rt = OPRT_FILE_READ_FAILED;
            break;
        }
        PR_DEBUG("len:%d,pack_num:%d,sque_num:%d", one_pack_len, pack_num, sque_num);

        rt = __file_storage_upd_to_cloud(&remote_file[1], read_buffer, one_pack_len, pack_num, sque_num);
        if (OPRT_OK != rt) {
            goto EXIT;
        }
        sque_num++;
        pos += one_pack_len;
        tal_system_sleep(HTTP_SEND_INTERVAL);
        if (file_storage_ctrl.stop) {
            PR_DEBUG("recv stop cmd");
            rt = OPRT_COM_ERROR;
            goto EXIT;
        }
    }

    __file_storage_mqc_notify(pack_num, sque_num, remote_file);

EXIT:
    if (fp) {
        tal_fclose(fp);
    }
    if (read_buffer) {
        tal_free(read_buffer);
    }
    if (OPRT_OK != rt) {
        tuya_file_storage_free();
    }
    return rt;
}

void tuya_file_storage_free(void)
{
    if (file_storage_ctrl.http_session) {
        http_close_session(&file_storage_ctrl.http_session);
    }
    file_storage_ctrl.stop = false;
    __file_storage_free_config();
    return;
}

OPERATE_RET tuya_file_storage_upd(char *local_file, FILE_STORAGE_BIZ_TYPE type)
{
    OPERATE_RET        rt     = OPRT_OK;
    tuya_iot_client_t *client = tuya_iot_client_get();
    if (!tuya_mqtt_connected(&client->mqctx)) {
        PR_ERR("mqtt was offline");
        return OPRT_SVC_MQTT_GW_MQ_OFFLILNE;
    }

    __file_storage_init();
    tal_mutex_lock(file_storage_ctrl.mutex);
    if (type != file_storage_ctrl.biz_type) {
        tuya_file_storage_free();
    }
    file_storage_ctrl.biz_type = type;
    rt                         = __file_storage_action(local_file);
    if ((OPRT_MID_HTTP_GET_RESP_ERROR == rt) || (OPRT_MID_HTTP_SD_REQ_ERROR == rt)) {
        PR_NOTICE("http upload failed, do it again");
        rt = __file_storage_action(local_file);
    }
    tal_mutex_unlock(file_storage_ctrl.mutex);
    PR_NOTICE("upload finished, rt:%d", rt);

    return rt;
}

static void __file_storage_upd_raw_close(void)
{
    file_storage_ctrl.offset    = 0;
    file_storage_ctrl.total_len = 0;
    memset(file_storage_ctrl.encrypt_iv, 0, sizeof(file_storage_ctrl.encrypt_iv));
    if (file_storage_ctrl.raw_session) {
        http_close_session(&file_storage_ctrl.raw_session);
    }
}

OPERATE_RET tuya_file_storage_upd_raw_end(char *remote_name, uint32_t *file_id)
{
    OPERATE_RET rt            = OPRT_OK;
    cJSON      *result        = NULL;
    int         post_data_len = 1024;
    char       *post_data     = NULL;

    __file_storage_upd_raw_close();

    post_data = tal_malloc(post_data_len);
    if (post_data == NULL) {
        PR_ERR("malloc err");
        return OPRT_MALLOC_FAILED;
    }
    FILE_STORAGE_CONFIG_T *config = &file_storage_ctrl.config;
    memset(post_data, 0, post_data_len);
    snprintf(post_data, post_data_len,
             "{\"fileName\":\"%s/%s\",\"bucket\":\"%s\",\"type\":\"%s\", \"subAction\":\"%d\"}", config->pathconfig,
             remote_name, config->bucket, __file_storage_get_biz_type(), 1);
    rt = atop_service_comm_post_simple(TI_TC_UPLOAD_URL_SAVE, "1.0", post_data, NULL, &result);
    tal_free(post_data);
    if (OPRT_OK != rt) {
        PR_ERR("atop notify err,rt:%d", rt);
        return rt;
    }
    PR_DEBUG("atop notify success");
    cJSON *child = cJSON_GetObjectItem(result, "picId");
    if (NULL == child) {
        PR_ERR("fail to get value for key result");
        return OPRT_CJSON_GET_ERR;
    }
    *file_id = child->valueint;
    PR_DEBUG("file id:%d", *file_id);
    cJSON_Delete(result);
    return rt;
}

OPERATE_RET tuya_file_storage_upd_raw_send(char *data, uint32_t len)
{
    OPERATE_RET rt       = OPRT_OK;
    uint8_t    *enc_data = NULL;
    uint32_t    enc_len  = 0;
    if (file_storage_ctrl.raw_session == NULL) {
        PR_ERR("raw session null");
        return OPRT_COM_ERROR;
    }

    extern uint8_t *tuya_file_storage_get_tc_key(void);
    uint8_t        *key = tuya_file_storage_get_tc_key();
    if (NULL == key || strlen((char *)key) == 0) {
        PR_ERR("tc key is null");
        return OPRT_COM_ERROR;
    }

    file_storage_ctrl.offset += len;
    if (file_storage_ctrl.offset >= file_storage_ctrl.total_len) {
        rt = tal_aes128_cbc_encode((uint8_t *)data, len, key, file_storage_ctrl.encrypt_iv, &enc_data, &enc_len);
    } else {
        if (len % 16) {
            PR_ERR("data len err, must be 16 byte align %d", len);
            rt = OPRT_INVALID_PARM;
            goto EXIT;
        }
        enc_len  = len;
        enc_data = tal_malloc(enc_len);
        TUYA_CHECK_NULL_GOTO(enc_data, EXIT);
        memset(enc_data, 0, enc_len);
        rt = tal_aes128_cbc_encode_raw((uint8_t *)data, len, key, file_storage_ctrl.encrypt_iv, enc_data);
    }
    if (OPRT_OK != rt) {
        PR_ERR("aes encrypt err:%d", rt);
        goto EXIT;
    }
    PR_DEBUG("actual len:%d, %d", len, enc_len);
    int written = http_write_content(file_storage_ctrl.raw_session, enc_data, enc_len);
    tal_free(enc_data);
    if ((uint32_t)written != enc_len) {
        PR_ERR("http put raw write err %d, %d", written, enc_len);
        rt = OPRT_MID_HTTP_SD_REQ_ERROR;
        goto EXIT;
    }
    return OPRT_OK;

EXIT:
    if (OPRT_OK != rt) {
        __file_storage_upd_raw_close();
        tuya_file_storage_free();
    }
    return rt;
}

static OPERATE_RET __file_storage_upd_put_raw(char *remote_name, uint32_t total_len)
{
    OPERATE_RET rt             = OPRT_OK;
    uint8_t     aes_header[64] = {0};
    rt                         = __file_storage_get_config();
    if (OPRT_OK != rt) {
        goto EXIT;
    }

    char *url = NULL;
    rt        = __file_storage_make_url(remote_name, 1, 0, &url);
    if ((OPRT_OK != rt) || (NULL == url)) {
        PR_ERR("make url err, %d", rt);
        goto EXIT;
    }

    file_storage_ctrl.total_len = total_len;
    file_storage_ctrl.offset    = 0;

    // Open session for streaming
    rt = http_open_session(&file_storage_ctrl.raw_session, url, 30000);
    if (OPRT_OK != rt) {
        PR_ERR("http open session failed, rt:%d", rt);
        tal_free(url);
        goto EXIT;
    }

    FILE_STORAGE_HEADER_PARAM_T header_param;
    memset(&header_param, 0, sizeof(header_param));
    header_param.config = &file_storage_ctrl.config;
    header_param.method = HTTP_PUT;
    tuya_file_storage_build_headers(&header_param);

    uint32_t body_len = total_len + sizeof(aes_header);
    body_len += (16 - (body_len % 16)); // 16 byte align

    http_req_t req = {
        .type                 = HTTP_PUT,
        .version              = HTTP_VER_1_1,
        .custom_headers       = header_param.headers,
        .custom_headers_count = header_param.headers_count,
    };

    rt = http_send_request_stream(file_storage_ctrl.raw_session, &req, body_len, 0);
    tal_free(url);
    if (OPRT_OK != rt) {
        PR_ERR("http put raw start err %d", rt);
        rt = OPRT_MID_HTTP_SD_REQ_ERROR;
        goto EXIT;
    }

    memset(file_storage_ctrl.encrypt_iv, 0, sizeof(file_storage_ctrl.encrypt_iv));
    uni_random_bytes(file_storage_ctrl.encrypt_iv, sizeof(file_storage_ctrl.encrypt_iv));
    memcpy(aes_header + 4, file_storage_ctrl.encrypt_iv, sizeof(file_storage_ctrl.encrypt_iv));

    PR_DEBUG("actual len:%d", (int)sizeof(aes_header));
    int written = http_write_content(file_storage_ctrl.raw_session, aes_header, sizeof(aes_header));
    if ((size_t)written != sizeof(aes_header)) {
        PR_ERR("http put raw write err %d", written);
        rt = OPRT_MID_HTTP_SD_REQ_ERROR;
        goto EXIT;
    }

    return OPRT_OK;

EXIT:
    if (OPRT_OK != rt) {
        __file_storage_upd_raw_close();
        tuya_file_storage_free();
    }
    return rt;
}

OPERATE_RET tuya_file_storage_upd_raw_start(char *remote_name, uint32_t total_len)
{
    OPERATE_RET        rt     = OPRT_OK;
    tuya_iot_client_t *client = tuya_iot_client_get();
    if (!tuya_mqtt_connected(&client->mqctx)) {
        PR_ERR("mqtt was offline");
        return OPRT_SVC_MQTT_GW_MQ_OFFLILNE;
    }
    PR_DEBUG("total len:%d", total_len);
    __file_storage_init();
    tal_mutex_lock(file_storage_ctrl.mutex);
    file_storage_ctrl.biz_type = BIZ_TYPE_AI_CAMERA;
    rt                         = __file_storage_upd_put_raw(remote_name, total_len);
    if (OPRT_MID_HTTP_SD_REQ_ERROR == rt) {
        PR_NOTICE("http upload failed, do it again");
        rt = __file_storage_upd_put_raw(remote_name, total_len);
    }
    tal_mutex_unlock(file_storage_ctrl.mutex);
    PR_NOTICE("upload start, rt:%d", rt);

    return rt;
}
