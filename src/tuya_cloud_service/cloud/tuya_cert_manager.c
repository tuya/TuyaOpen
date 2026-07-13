/**
 * @file tuya_cert_manager.c
 * @brief Tuya Certificate Manager
 *
 * This file implements the certificate management functionalities for the Tuya
 * IoT SDK, including managing third-party cloud CA certificates, client
 * certificates, PSK authentication, and periodic certificate validation.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tuya_cloud_com_defs.h"
#include "tuya_list.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_mutex.h"
#include "tal_kv.h"
#include "tal_workqueue.h"
#include "tal_sw_timer.h"
#include "tal_workq_service.h"
#include "tal_hash.h"
#include "tal_x509.h"
#include "tal_event.h"
#include "tal_event_info.h"
#include "tuya_tools.h"
#include "tuya_tls.h"
#include "tuya_iot.h"
#include "tuya_endpoint.h"
#include "tuya_register_center.h"
#include "iotdns.h"
#include "mix_method.h"
#include "uni_random.h"
#include "tuya_cert_manager.h"

#define CLT_PVT_KEY_MF      "clt_private_key_mf"
#define CLT_CERT_MF         "clt_cert_mf"
#define CERT_QUERY_INTERVAL (1000 * 60 * 60 * 6) // 6 hour
#define DERIVED_PSK_KEY_LEN 32
#define PSK30_KEY_LEN       44
#define PSK20_KEY_LEN       37
#define CERT_URL_MAX_LEN    HTTP_URL_LMT

typedef struct {
    uint8_t *p_der;
    uint32_t der_len;
} ty_tls_der_t;

typedef struct {
    uint8_t  ref;
    uint32_t len;
    uint8_t  ca[0];
} THIRD_CLOUD_CA_INFO;

typedef struct {
    LIST_HEAD            node;
    char                 url[CERT_URL_MAX_LEN];
    THIRD_CLOUD_CA_INFO *ca_info;
} THIRD_CLOUD_CA_NODE;

typedef struct {
    LIST_HEAD    listHead;
    MUTEX_HANDLE mutex;
} THIRD_CLOUD_CA_MNG, *P_THIRD_CLOUD_MNG;

typedef struct {
    client_cert_info_t   client_cert_info;
    ty_tls_der_t        *p_der_arr;
    uint32_t             der_arr_size;
    P_THIRD_CLOUD_MNG    p_third_cloud_ca;
    tuya_tls_event_cb    event_cb;
    DELAYED_WORK_HANDLE  tm_update_tls_url;
    TIMER_ID             cert_query;
    client_psk_info_t    client_psk_info;
    TUYA_CERT_SAVE_CB    ca_save;
    TUYA_CERT_RESTORE_CB ca_restore;
} ca_cert_manager_t;

static ca_cert_manager_t s_tuya_cert_mng = {0};

OPERATE_RET __ty_third_cloud_ca_init(void)
{
    OPERATE_RET op_ret = OPRT_OK;
    if (s_tuya_cert_mng.p_third_cloud_ca != NULL) {
        return OPRT_OK;
    }
    s_tuya_cert_mng.p_third_cloud_ca = tal_malloc(sizeof(THIRD_CLOUD_CA_MNG));
    if (NULL == s_tuya_cert_mng.p_third_cloud_ca) {
        PR_ERR("tal_malloc failed");
        return OPRT_MALLOC_FAILED;
    }
    INIT_LIST_HEAD(&s_tuya_cert_mng.p_third_cloud_ca->listHead);
    op_ret = tal_mutex_create_init(&s_tuya_cert_mng.p_third_cloud_ca->mutex);
    if (OPRT_OK != op_ret) {
        tal_free(s_tuya_cert_mng.p_third_cloud_ca);
        return op_ret;
    }
    return OPRT_OK;
}

bool __ty_iot_third_cloud_load_ca(void *p_ctx, char *p_url)
{
    __ty_third_cloud_ca_init();
    tal_mutex_lock(s_tuya_cert_mng.p_third_cloud_ca->mutex);
    P_LIST_HEAD          pPos;
    THIRD_CLOUD_CA_NODE *ca_node;
    tuya_list_for_each(pPos, &(s_tuya_cert_mng.p_third_cloud_ca->listHead))
    {
        ca_node = tuya_list_entry(pPos, THIRD_CLOUD_CA_NODE, node);
        if ((ca_node) && (!strncmp(ca_node->url, p_url, strlen(p_url)))) {
            int rt = tuya_tls_register_x509_crt_der(p_ctx, ca_node->ca_info->ca, ca_node->ca_info->len);
            PR_DEBUG("load ca to tls %d", rt);
            tal_mutex_unlock(s_tuya_cert_mng.p_third_cloud_ca->mutex);
            return TRUE;
        }
    }
    tal_mutex_unlock(s_tuya_cert_mng.p_third_cloud_ca->mutex);
    if (s_tuya_cert_mng.ca_restore) {
        return s_tuya_cert_mng.ca_restore(p_ctx, p_url);
    }
    PR_DEBUG("not found third ca");
    return FALSE;
}

/**
 * @brief Store third-party cloud CA certificate
 *
 * @param[in] p_url URL associated with the certificate
 * @param[in] ca_data CA certificate data (base64 encoded or raw)
 * @param[in] ca_data_len Length of CA data
 * @param[in] need_base64_decode Whether base64 decoding is needed
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
static OPERATE_RET __tuya_iot_store_third_cloud_ca(char *p_url, const uint8_t *ca_data, uint32_t ca_data_len,
                                                   bool need_base64_decode)
{
    OPERATE_RET op_ret = OPRT_OK;

    if (p_url == NULL || ca_data == NULL || ca_data_len == 0) {
        PR_ERR("invalid input parameters");
        return OPRT_INVALID_PARM;
    }

    // Allocate new CA node
    THIRD_CLOUD_CA_NODE *ca_node;
    NEW_LIST_NODE(THIRD_CLOUD_CA_NODE, ca_node);
    if (!ca_node) {
        PR_ERR("new list node error");
        return OPRT_MALLOC_FAILED;
    }
    memset(ca_node, 0, sizeof(THIRD_CLOUD_CA_NODE));

    // Allocate memory for CA info
    uint32_t alloc_len = ca_data_len + 1;
    ca_node->ca_info   = tal_malloc(sizeof(THIRD_CLOUD_CA_INFO) + alloc_len);
    if (NULL == ca_node->ca_info) {
        PR_ERR("malloc error");
        FreeNode(ca_node);
        return OPRT_MALLOC_FAILED;
    }
    memset(ca_node->ca_info, 0, sizeof(THIRD_CLOUD_CA_INFO) + alloc_len);

    // Copy URL
    if (strlen(p_url) < sizeof(ca_node->url)) {
        strncpy(ca_node->url, p_url, strlen(p_url));
    }

    // Decode or copy CA data
    if (need_base64_decode) {
        ca_node->ca_info->len = tuya_base64_decode((char *)ca_data, ca_node->ca_info->ca);
    } else {
        ca_node->ca_info->len = ca_data_len;
        memcpy(ca_node->ca_info->ca, ca_data, ca_data_len);
    }

    // Initialize third cloud CA manager
    op_ret = __ty_third_cloud_ca_init();
    if (OPRT_OK != op_ret) {
        tal_free(ca_node->ca_info);
        FreeNode(ca_node);
        return op_ret;
    }

    // Lock and search for existing CA
    tal_mutex_lock(s_tuya_cert_mng.p_third_cloud_ca->mutex);
    P_LIST_HEAD          pPos, pNext;
    THIRD_CLOUD_CA_NODE *ca_node_old;
    tuya_list_for_each_safe(pPos, pNext, &(s_tuya_cert_mng.p_third_cloud_ca->listHead))
    {
        ca_node_old = tuya_list_entry(pPos, THIRD_CLOUD_CA_NODE, node);
        if (ca_node_old) {
            if (!strncmp(ca_node_old->url, p_url, strlen(p_url))) { // same url
                if (!strncmp((char *)ca_node_old->ca_info->ca, (char *)ca_node->ca_info->ca, ca_node->ca_info->len)) {
                    // Same URL and CA, no need to update
                    tal_free(ca_node->ca_info);
                    FreeNode(ca_node);
                    tal_mutex_unlock(s_tuya_cert_mng.p_third_cloud_ca->mutex);
                    PR_DEBUG("same url and ca %s", p_url);
                    return OPRT_OK;
                } else {
                    if (ca_node_old->ca_info->ref == 0) {
                        // Same url but ca change and no ref, delete then add
                        tal_free(ca_node_old->ca_info);
                        DeleteNodeAndFree(ca_node_old, node);
                        PR_DEBUG("same url but ca change and no ref");
                        break;
                    } else {
                        // ref-- then old modify, new add
                        ca_node_old->ca_info->ref--;
                        PR_DEBUG("same url but ca change and has ref %d", ca_node_old->ca_info->ref);
                        DeleteNodeAndFree(ca_node_old, node);
                        break;
                    }
                }
            } else if (!strncmp((char *)ca_node_old->ca_info->ca, (char *)ca_node->ca_info->ca,
                                ca_node->ca_info->len)) {
                // Different url but same CA
                tal_free(ca_node->ca_info);
                ca_node_old->ca_info->ref++;
                ca_node->ca_info = ca_node_old->ca_info;
                PR_DEBUG("diff url but ca same %d", ca_node_old->ca_info->ref);
                break;
            }
        }
    }

    // Add new node to list
    tuya_list_add(&(ca_node->node), &(s_tuya_cert_mng.p_third_cloud_ca->listHead));
    if (s_tuya_cert_mng.ca_save) {
        s_tuya_cert_mng.ca_save(p_url, ca_node->ca_info->ca, ca_node->ca_info->len);
    }
    tal_mutex_unlock(s_tuya_cert_mng.p_third_cloud_ca->mutex);
    PR_DEBUG("new CA stored for url %s", p_url);

    return OPRT_OK;
}

OPERATE_RET tuya_iot_store_third_cloud_ca(char *p_url, const uint8_t *ca_data, uint32_t ca_data_len,
                                          bool need_base64_decode)
{
    return __tuya_iot_store_third_cloud_ca(p_url, ca_data, ca_data_len, need_base64_decode);
}

void tuya_iot_get_third_cloud_ca(char *p_url)
{
    OPERATE_RET op_ret = OPRT_OK;

    if (p_url == NULL) {
        PR_ERR("input null");
        return;
    }
    PR_DEBUG("require CA for url %s", p_url);

    uint8_t  *cacert     = NULL;
    uint16_t  cacert_len = 0;
    op_ret = tuya_iotdns_query_domain_certs(p_url, &cacert, &cacert_len);
    if ((OPRT_OK != op_ret) || (cacert == NULL) || (cacert_len == 0)) {
        PR_ERR("certificate get failed %d", op_ret);
        return;
    }

    // Store the certificate (raw DER format from iotdns, no base64 decoding needed)
    op_ret = __tuya_iot_store_third_cloud_ca(p_url, cacert, cacert_len, FALSE);
    tal_free(cacert);

    if (OPRT_OK != op_ret) {
        PR_ERR("store CA failed %d", op_ret);
    }

    return;
}

bool __ty_is_iot_dns_url(const char *p_url)
{
    register_center_t rcs = {0};
    tuya_register_center_get(&rcs);
    if (rcs.urlx && NULL != strstr(rcs.urlx, p_url)) {
        PR_DEBUG("iot-dns url");
        return TRUE;
    }

    PR_DEBUG("p_url:%s, rcs.urlx:%s", p_url, rcs.urlx);
    return FALSE;
}

bool __ty_is_tuya_public_url(const char *p_url)
{
    const tuya_endpoint_t *endpoint = tuya_endpoint_get();
    if (endpoint == NULL) {
        return FALSE;
    }

    if ((NULL != strstr(endpoint->atop.host, p_url)) || (NULL != strstr(endpoint->mqtt.host, p_url))) {
        PR_DEBUG("tuya public url");
        return TRUE;
    }

    PR_DEBUG("third cloud url");
    return FALSE;
}

OPERATE_RET tuya_iot_third_cloud_x509_crt_der(void *p_ctx, char *p_url)
{
    if (p_ctx == NULL || p_url == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (__ty_iot_third_cloud_load_ca(p_ctx, p_url)) {
        return OPRT_OK;
    }

    tuya_iot_get_third_cloud_ca(p_url);

    if (!__ty_iot_third_cloud_load_ca(p_ctx, p_url)) {
        return OPRT_NOT_FOUND;
    }

    return OPRT_OK;
}

OPERATE_RET httpc_domain_certs_get(cJSON **result, const char *url_msg)
{
    if (NULL == url_msg || NULL == result) {
        return OPRT_INVALID_PARM;
    }

    uint8_t  *cacert     = NULL;
    uint16_t  cacert_len = 0;
    int rt = tuya_iotdns_query_domain_certs((char *)url_msg, &cacert, &cacert_len);
    if (OPRT_OK != rt || cacert == NULL || cacert_len == 0) {
        PR_ERR("iotdns query certs failed %d", rt);
        return rt;
    }

    // Encode the DER cert as base64 and return as a cJSON string
    uint32_t b64_len = cacert_len / 3 * 4 + 8;
    char *b64_buf = tal_malloc(b64_len);
    if (NULL == b64_buf) {
        tal_free(cacert);
        return OPRT_MALLOC_FAILED;
    }
    memset(b64_buf, 0, b64_len);
    tuya_base64_encode(cacert, b64_buf, cacert_len);
    tal_free(cacert);

    *result = cJSON_CreateString(b64_buf);
    tal_free(b64_buf);

    return OPRT_OK;
}

OPERATE_RET tuya_client_cert_write(const uint8_t *value, const uint32_t len)
{
    OPERATE_RET rt         = OPRT_OK;
    bool        pem_format = FALSE;
    uint8_t    *p          = NULL;
    uint32_t    size;
    p          = (uint8_t *)value;
    size       = len;
    pem_format = tuya_x509_is_ca_pem_format((uint8_t *)value, len);
    PR_DEBUG("pem_format:%d", pem_format);
    if (pem_format) {
        rt = tuya_x509_pem2der((uint8_t *)value, len, &p, &size);
        if (OPRT_OK != rt) {
            PR_ERR("pem to der err,ret:%d", rt);
            return rt;
        }
        PR_DEBUG("pem format,change to der,pem len:%d,der len:%d", len, size);
    }
    rt = tal_kv_set(CLT_CERT_MF, p, size);
    if (pem_format) {
        tal_free(p);
    }
    if (OPRT_OK != rt) {
        PR_ERR("kv set error, key:%s, rt:%d", CLT_CERT_MF, rt);
        return rt;
    }

    return rt;
}

OPERATE_RET tuya_client_private_key_write(const uint8_t *value, const uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    rt = tal_kv_set(CLT_PVT_KEY_MF, value, len);
    if (OPRT_OK != rt) {
        PR_ERR("tal_kv_set fails %s %d", CLT_PVT_KEY_MF, rt);
        return rt;
    }

    return rt;
}

const client_cert_info_t *tuya_client_cert_get(void)
{
    return (const client_cert_info_t *)(&s_tuya_cert_mng.client_cert_info);
}

const client_psk_info_t *tuya_client_psk_get(void)
{
    if (s_tuya_cert_mng.client_psk_info.psk_key && s_tuya_cert_mng.client_psk_info.psk_id) {
        return (const client_psk_info_t *)(&s_tuya_cert_mng.client_psk_info);
    } else {
        return NULL;
    }
}

void tuya_cert_save_to_kv(cJSON *result)
{
    tal_kv_del("tls_ca_cnt");

    cJSON *p_ca_arr = cJSON_GetObjectItem(result, "caArr");
    if (p_ca_arr != NULL) {
        int arr_size = cJSON_GetArraySize(p_ca_arr);
        int index    = 0;
        for (index = 0; index < arr_size; index++) {
            cJSON *p_ca            = cJSON_GetArrayItem(p_ca_arr, index);
            char   tls_ca_name[20] = {0};
            snprintf(tls_ca_name, sizeof(tls_ca_name), "tls_ca%d", index);
            PR_DEBUG("tls_ca_name:%s", tls_ca_name);
            tal_kv_set(tls_ca_name, (uint8_t *)(p_ca->valuestring), strlen(p_ca->valuestring) + 1);
        }
        // Delete old entries beyond the new count
        for (int i = arr_size; i < 16; i++) {
            char tls_ca_name[20] = {0};
            snprintf(tls_ca_name, sizeof(tls_ca_name), "tls_ca%d", i);
            tal_kv_del(tls_ca_name);
        }
        char ca_cnt_str[5] = {0};
        snprintf(ca_cnt_str, 5, "%u", arr_size);
        tal_kv_set("tls_ca_cnt", (uint8_t *)ca_cnt_str, 5);
    }

    return;
}

void tuya_cert_kv_to_ram(void)
{
    int index = 0;
    for (index = 0; index < (int)s_tuya_cert_mng.der_arr_size; index++) {
        tal_free(s_tuya_cert_mng.p_der_arr[index].p_der);
    }
    tal_free(s_tuya_cert_mng.p_der_arr);
    s_tuya_cert_mng.p_der_arr    = NULL;
    s_tuya_cert_mng.der_arr_size = 0;

    uint8_t *p_ca_cnt_str   = NULL;
    size_t   ca_cnt_str_len = 0;
    int rt = tal_kv_get("tls_ca_cnt", &p_ca_cnt_str, &ca_cnt_str_len);
    if (OPRT_OK != rt || NULL == p_ca_cnt_str) {
        PR_DEBUG("load tls_ca_cnt fail. no ca in local db");
        return;
    }
    int ca_cnt = atoi((char *)p_ca_cnt_str);
    PR_DEBUG("tls_ca_cnt:%s and parse:%d", (char *)p_ca_cnt_str, ca_cnt);
    tal_kv_free(p_ca_cnt_str);
    if (ca_cnt == 0) {
        return;
    }
    s_tuya_cert_mng.p_der_arr = tal_malloc(sizeof(ty_tls_der_t) * ca_cnt);
    if (NULL == s_tuya_cert_mng.p_der_arr) {
        PR_ERR("malloc der array fail. %d", ca_cnt);
        return;
    }
    memset(s_tuya_cert_mng.p_der_arr, 0, (sizeof(ty_tls_der_t) * ca_cnt));

    for (index = 0; index < ca_cnt; index++) {
        char tls_ca_name[20] = {0};
        snprintf(tls_ca_name, sizeof(tls_ca_name), "tls_ca%d", index);
        uint8_t *p_ca_str = NULL;
        size_t   ca_len   = 0;
        rt = tal_kv_get(tls_ca_name, &p_ca_str, &ca_len);
        if (OPRT_OK != rt || NULL == p_ca_str) {
            PR_ERR("read %s fail.", tls_ca_name);
            return;
        }
        s_tuya_cert_mng.p_der_arr[index].p_der = tal_malloc(ca_len);
        if (NULL == s_tuya_cert_mng.p_der_arr[index].p_der) {
            PR_ERR("malloc der fail. %d", (int)ca_len);
            tal_kv_free(p_ca_str);
            return;
        }
        s_tuya_cert_mng.p_der_arr[index].der_len = tuya_base64_decode((char *)p_ca_str, s_tuya_cert_mng.p_der_arr[index].p_der);
        tal_kv_free(p_ca_str);
        PR_DEBUG("Parse DER %s:%d success", tls_ca_name, s_tuya_cert_mng.p_der_arr[index].der_len);
    }
    s_tuya_cert_mng.der_arr_size = ca_cnt;
}

OPERATE_RET http_iot_dns_get_root_ca(void)
{
    OPERATE_RET op_ret = OPRT_OK;

    register_center_t rcs = {0};
    tuya_register_center_get(&rcs);
    if (NULL == rcs.urlx) {
        PR_ERR("urlx was null");
        return OPRT_COM_ERROR;
    }

    tuya_iot_client_t *client = tuya_iot_client_get();
    if (NULL == client) {
        PR_ERR("client was null");
        return OPRT_COM_ERROR;
    }

    cJSON *root = cJSON_CreateObject();
    if (NULL == root) {
        PR_ERR("json err");
        return OPRT_MALLOC_FAILED;
    }
    cJSON_AddStringToObject(root, "uuid", client->config.uuid);
    char *p_buffer = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (NULL == p_buffer) {
        PR_ERR("json err");
        return OPRT_CR_CJSON_ERR;
    }

    // Use endpoint cert update to refresh the root CA
    tuya_endpoint_t endpoint_copy = {0};
    const tuya_endpoint_t *endpoint = tuya_endpoint_get();
    if (endpoint) {
        memcpy(&endpoint_copy, endpoint, sizeof(tuya_endpoint_t));
    }
    op_ret = tuya_endpoint_cert_get(&endpoint_copy);
    if (OPRT_OK != op_ret) {
        PR_ERR("get root ca err:%d", op_ret);
    }

    tal_free(p_buffer);
    return op_ret;
}

static void __ty_tls_event_cb(tuya_tls_event_t event, void *p_args)
{
    OPERATE_RET rt    = OPRT_OK;
    const char *p_url = (char *)p_args;
    if (NULL == p_url) {
        PR_ERR("url was NULL");
        return;
    }
    if (event == TUYA_TLS_CERT_EXPIRED) {
        PR_DEBUG("tls cert expired");
        if (__ty_is_iot_dns_url(p_url)) {
            rt = http_iot_dns_get_root_ca();
            if (OPRT_OK != rt) {
                PR_DEBUG("get root ca err,rt:%d", rt);
            }
        } else if (__ty_is_tuya_public_url(p_url)) {
            tal_workq_start_delayed(s_tuya_cert_mng.tm_update_tls_url, 100, LOOP_ONCE);
        } else {
            tuya_iot_get_third_cloud_ca((char *)p_url);
        }
        return;
    }
}

static void __update_endpoint_work(void *data)
{
    tuya_endpoint_update();
}

static void __update_tls_url_cb(void *data)
{
    if (!tuya_iot_is_connected()) {
        tal_workq_start_delayed(s_tuya_cert_mng.tm_update_tls_url, 1000, LOOP_ONCE);
        return;
    }
    PR_NOTICE("local cert expired. require New CA");
    tal_workq_schedule(WORKQ_SYSTEM, __update_endpoint_work, NULL);
}

tuya_tls_event_cb tuya_cert_get_tls_event_cb(void)
{
    return s_tuya_cert_mng.event_cb;
}

#if (TUYA_SECURITY_LEVEL >= TUYA_SL_1)
static void __cert_query_tm_cb(TIMER_ID timerID, void *pTimerArg)
{
    OPERATE_RET op_ret = OPRT_OK;

    register_center_t rcs = {0};
    tuya_register_center_get(&rcs);
    if (NULL == rcs.ca_cert) {
        PR_ERR("local cert was null");
        return;
    }

    char     serial_number[32] = {0};
    uint32_t serial_number_len = 0;
    op_ret = tuya_x509_get_serial(rcs.ca_cert, rcs.ca_cert_len, (uint8_t *)serial_number, &serial_number_len);
    if (OPRT_OK != op_ret) {
        PR_ERR("x509 get ca serial num err,op_ret:%d", op_ret);
        return;
    }
    char serial_number_str[80] = {0};
    tuya_hex2str((uint8_t *)serial_number_str, (uint8_t *)serial_number, serial_number_len);
    PR_DEBUG("ca serial num:%s,len:%d,hexlen:%d", serial_number_str, (int)strlen(serial_number_str), serial_number_len);

    char     fingerprint[64] = {0};
    uint32_t fingerprint_len = 0;
    op_ret = tuya_x509_get_fingerprint(rcs.ca_cert, rcs.ca_cert_len, X509_fingerprint_SHA256, (uint8_t *)fingerprint,
                                       &fingerprint_len);
    if (OPRT_OK != op_ret) {
        PR_ERR("x509 get ca finger print err,op_ret:%d", op_ret);
        return;
    }
    char fingerprint_str[200] = {0};
    tuya_hex2str((uint8_t *)fingerprint_str, (uint8_t *)fingerprint, fingerprint_len);
    PR_DEBUG("ca finger print:%s,len:%d,hexlen:%d", fingerprint_str, (int)strlen(fingerprint_str), fingerprint_len);

    // Build cert verify request
    cJSON *js_array = cJSON_CreateArray();
    if (NULL == js_array) {
        PR_ERR("json err");
        return;
    }

    cJSON *js_config = cJSON_CreateObject();
    if (NULL == js_config) {
        cJSON_Delete(js_array);
        PR_ERR("json err");
        return;
    }

    cJSON_AddStringToObject(js_config, "serial_number", serial_number_str);
    cJSON_AddStringToObject(js_config, "fingerprint", fingerprint_str);
    cJSON_AddItemToArray(js_array, js_config);
    char *p_buffer = cJSON_PrintUnformatted(js_array);
    cJSON_Delete(js_array);
    if (NULL == p_buffer) {
        PR_ERR("json err");
        return;
    }

    // Verify certificate via endpoint cert refresh
    PR_DEBUG("cert verify request:%s", p_buffer);
    op_ret = http_iot_dns_get_root_ca();
    if (OPRT_OK != op_ret) {
        PR_ERR("cert verify/refresh err:%d", op_ret);
    }

    tal_free(p_buffer);
    return;
}
#endif

static void __tuya_calc_psk_id(char version)
{
    tuya_iot_client_t *client = tuya_iot_client_get();
    if (NULL == client) {
        PR_ERR("client null");
        return;
    }

    uint8_t sha256_psk_id[32] = {0};
    tal_sha256_ret((const unsigned char *)(client->config.uuid), strlen(client->config.uuid), sha256_psk_id, 0);

    char *psk_id_arr = tal_malloc(70);
    if (NULL == psk_id_arr) {
        PR_ERR("psk_id_arr null");
        return;
    }
    int psk_id_len = 0;
    /* version */
    psk_id_arr[0] = version;
    psk_id_len += 1;

    /* random */
    uni_random_bytes((uint8_t *)psk_id_arr + psk_id_len, 16);
    psk_id_len += 16;

    /* uuid sha256 */
    memcpy(psk_id_arr + psk_id_len, sha256_psk_id, 32);
    psk_id_len += 32;

    int tmp_index = 0;
    for (tmp_index = 0; tmp_index < psk_id_len; tmp_index++) {
        if (psk_id_arr[tmp_index] == 0x00) {
            psk_id_arr[tmp_index] = '?';
        }
    }

    s_tuya_cert_mng.client_psk_info.psk_id      = psk_id_arr;
    s_tuya_cert_mng.client_psk_info.psk_id_size = psk_id_len;

    PR_DEBUG("psk_id len:%d", psk_id_len);
    PR_HEXDUMP_DEBUG("psk_id_arr", (uint8_t *)psk_id_arr, psk_id_len);
}

static OPERATE_RET __tuya_calc_client_psk_key(void)
{
    OPERATE_RET       rt  = OPRT_OK;
    register_center_t rcs = {0};
    tuya_register_center_get(&rcs);

    const tuya_tls_config_t *psk_config = tuya_tls_psk_mode_config_get();
    if (psk_config == NULL || psk_config->psk_key == NULL) {
        PR_DEBUG("psk config not available");
        return OPRT_OK;
    }

    char *psk_key = psk_config->psk_key;

    if (psk_key[0] != 0) {
        if (strlen(psk_key) == PSK30_KEY_LEN) {
            PR_DEBUG("psk key was 3.0");
            __tuya_calc_psk_id(0x03);
            if (rcs.pub == 0) { // private cloud, derive psk key
                PR_DEBUG("pub was 0,psk key will derived");
                s_tuya_cert_mng.client_psk_info.psk_key      = psk_key;
                s_tuya_cert_mng.client_psk_info.psk_key_size = strlen(psk_key);
            } else {
                PR_DEBUG("pub was 1,use mf psk key");
                s_tuya_cert_mng.client_psk_info.psk_key      = psk_key;
                s_tuya_cert_mng.client_psk_info.psk_key_size = strlen(psk_key);
            }
        } else {
            PR_DEBUG("psk key was 2.0");
            __tuya_calc_psk_id(0x02);
            s_tuya_cert_mng.client_psk_info.psk_key      = psk_key;
            s_tuya_cert_mng.client_psk_info.psk_key_size = strlen(psk_key);
        }
    } else {
        PR_DEBUG("psk key was 1.0");
    }

    return rt;
}

static int __tuya_update_client_key(void *data)
{
    OPERATE_RET rt = OPRT_OK;
    rt             = __tuya_calc_client_psk_key();
    if (OPRT_OK != rt) {
        PR_ERR("update psk key err");
        return rt;
    }

    return rt;
}

void tuya_cert_manager_deinit(void)
{
    if (s_tuya_cert_mng.cert_query) {
        tal_sw_timer_delete(s_tuya_cert_mng.cert_query);
    }
    if (s_tuya_cert_mng.client_cert_info.cert) {
        tal_kv_free(s_tuya_cert_mng.client_cert_info.cert);
    }
    if (s_tuya_cert_mng.client_cert_info.private_key) {
        tal_kv_free(s_tuya_cert_mng.client_cert_info.private_key);
    }

    int index = 0;
    for (index = 0; index < (int)s_tuya_cert_mng.der_arr_size; index++) {
        if (s_tuya_cert_mng.p_der_arr[index].p_der) {
            tal_free(s_tuya_cert_mng.p_der_arr[index].p_der);
        }
    }
    if (s_tuya_cert_mng.p_der_arr) {
        tal_free(s_tuya_cert_mng.p_der_arr);
    }

    tal_event_unsubscribe(EVENT_RSC_UPDATE, "cert_man", __tuya_update_client_key);

    memset(&s_tuya_cert_mng, 0, sizeof(ca_cert_manager_t));
    PR_DEBUG("cert manager deinit");
    return;
}

OPERATE_RET tuya_cert_manager_init(void)
{
    OPERATE_RET rt = OPRT_OK;
#if (TUYA_SECURITY_LEVEL >= TUYA_SL_1)
    rt = tal_sw_timer_create(__cert_query_tm_cb, NULL, &s_tuya_cert_mng.cert_query);
    if (OPRT_OK != rt) {
        PR_ERR("cert_query timer create err");
        goto EXIT;
    } else {
        tal_sw_timer_start(s_tuya_cert_mng.cert_query, CERT_QUERY_INTERVAL, TAL_TIMER_CYCLE);
    }
#endif

    s_tuya_cert_mng.event_cb = __ty_tls_event_cb;
    tal_workq_init_delayed(WORKQ_HIGHTPRI, __update_tls_url_cb, NULL, &s_tuya_cert_mng.tm_update_tls_url);
    tuya_cert_kv_to_ram();

#if (TUYA_SECURITY_LEVEL >= TUYA_SL_2)
    {
        size_t cert_len = 0;
        rt = tal_kv_get(CLT_CERT_MF, &s_tuya_cert_mng.client_cert_info.cert, &cert_len);
        if (OPRT_OK != rt) {
            PR_ERR("tal_kv_get fails %s %d", CLT_CERT_MF, rt);
            goto EXIT;
        }
        s_tuya_cert_mng.client_cert_info.cert_len = (uint32_t)cert_len;

        size_t key_len = 0;
        rt = tal_kv_get(CLT_PVT_KEY_MF, &s_tuya_cert_mng.client_cert_info.private_key, &key_len);
        if (OPRT_OK != rt) {
            PR_ERR("tal_kv_get fails %s %d", CLT_PVT_KEY_MF, rt);
            goto EXIT;
        }
        s_tuya_cert_mng.client_cert_info.private_key_len = (uint32_t)key_len;
    }
#endif

    rt = __tuya_calc_client_psk_key();
    if (OPRT_OK != rt) {
        PR_ERR("calc psk key err");
        goto EXIT;
    }

    tal_event_subscribe(EVENT_RSC_UPDATE, "cert_man", __tuya_update_client_key, SUBSCRIBE_TYPE_NORMAL);

    PR_DEBUG("cert manager init");

    return rt;

EXIT:

    tuya_cert_manager_deinit();

    return rt;
}

OPERATE_RET tuya_cert_manager_load(char *p_url, CERT_PARSE_CB cb, void *p_ctx)
{
    OPERATE_RET ret   = OPRT_OK;
    int         index = 0;
    if (NULL == p_url) {
        PR_ERR("url was null");
        return OPRT_INVALID_PARM;
    }

#if (TUYA_SECURITY_LEVEL >= TUYA_SL_1)
    // fix SL switch while not renetcfg,then will cause system crash
    register_center_t rcs = {0};
    tuya_register_center_get(&rcs);
    if (NULL == rcs.urlx) {
        PR_ERR("urlx was not exist, please renetcfg");
        return OPRT_INVALID_PARM;
    }
#endif

    if (__ty_is_iot_dns_url(p_url)) {
        register_center_t rcs = {0};
        tuya_register_center_get(&rcs);
        ret = cb(p_ctx, rcs.ca_cert, rcs.ca_cert_len);
        if (ret != OPRT_OK) {
            PR_DEBUG("cert:%s,len:%d", rcs.ca_cert, rcs.ca_cert_len);
            PR_ERR("load iot dns cert failed. ret: %x", ret);
        }
    } else if (__ty_is_tuya_public_url(p_url)) {
        for (index = 0; index < (int)s_tuya_cert_mng.der_arr_size; index++) {
            ret = cb(p_ctx, s_tuya_cert_mng.p_der_arr[index].p_der, s_tuya_cert_mng.p_der_arr[index].der_len);
            PR_DEBUG("parse crt <%d> len <%d> ret <%d>", index, s_tuya_cert_mng.p_der_arr[index].der_len, ret);
            if (ret != OPRT_OK) {
                PR_ERR("cert <%d> parse fail. ret: %x %d", index, ret, ret);
            }
        }
    } else {
        ret = tuya_iot_third_cloud_x509_crt_der(p_ctx, p_url);
        if (ret != OPRT_OK) {
            PR_ERR("third cloud cert parse fail. ret: %x", ret);
        }
    }

    return ret;
}

void tuya_cert_manager_reg(TUYA_CERT_SAVE_CB scb, TUYA_CERT_RESTORE_CB rcb)
{
    s_tuya_cert_mng.ca_save    = scb;
    s_tuya_cert_mng.ca_restore = rcb;
}
