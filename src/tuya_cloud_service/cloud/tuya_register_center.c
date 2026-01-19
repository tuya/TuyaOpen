/**
 * @file tuya_register_center.c
 * @brief Implementation of Tuya's device registration center functionalities.
 *
 * This file contains the implementation of functionalities related to the
 * device registration process with Tuya's cloud services. It includes handling
 * of device registration, certificate management, and secure communication
 * setup for newly onboarded devices. The registration center ensures devices
 * are securely and efficiently registered and authenticated with Tuya's IoT
 * platform.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "mix_method.h"
#include "tal_event.h"
#include "tal_api.h"
#include "tal_security.h"
#include "tal_kv.h"
#include "tuya_register_center.h"

static const unsigned char default_iotdns_cacert[] = {
    0x30, 0x82, 0x02, 0x18, 0x30, 0x82, 0x01, 0xbd, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x11, 0x00, 0x8e, 0x24, 0x55,
    0x22, 0x3f, 0x0d, 0x1e, 0x93, 0x95, 0x1a, 0xaf, 0xbf, 0x40, 0xbb, 0x03, 0x81, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86,
    0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x30, 0x35, 0x31, 0x1d, 0x30, 0x1b, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x14,
    0x53, 0x69, 0x6e, 0x62, 0x61, 0x79, 0x20, 0x47, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x4c, 0x69, 0x6d, 0x69, 0x74, 0x65,
    0x64, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x0b, 0x43, 0x6c, 0x6f, 0x75, 0x64, 0x20, 0x52,
    0x43, 0x41, 0x20, 0x32, 0x30, 0x20, 0x17, 0x0d, 0x32, 0x32, 0x30, 0x35, 0x33, 0x31, 0x31, 0x36, 0x30, 0x30, 0x30,
    0x30, 0x5a, 0x18, 0x0f, 0x32, 0x30, 0x37, 0x32, 0x30, 0x36, 0x33, 0x30, 0x31, 0x35, 0x35, 0x39, 0x35, 0x39, 0x5a,
    0x30, 0x35, 0x31, 0x1d, 0x30, 0x1b, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x14, 0x53, 0x69, 0x6e, 0x62, 0x61, 0x79,
    0x20, 0x47, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x4c, 0x69, 0x6d, 0x69, 0x74, 0x65, 0x64, 0x31, 0x14, 0x30, 0x12, 0x06,
    0x03, 0x55, 0x04, 0x03, 0x0c, 0x0b, 0x43, 0x6c, 0x6f, 0x75, 0x64, 0x20, 0x52, 0x43, 0x41, 0x20, 0x32, 0x30, 0x59,
    0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03,
    0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0xea, 0x8d, 0xfb, 0xb3, 0xc9, 0x78, 0x7c, 0x3f, 0x43, 0x2e, 0xb9, 0x6a, 0xc7,
    0x3d, 0x44, 0xa8, 0x3a, 0x9f, 0x4e, 0xaa, 0xc4, 0x87, 0x9c, 0x2b, 0xc3, 0x2f, 0x5f, 0x04, 0x2b, 0xf5, 0x9f, 0xa3,
    0x53, 0x0e, 0x69, 0x0a, 0x08, 0x03, 0x2f, 0x36, 0xcd, 0xa0, 0xbe, 0x82, 0x08, 0xd0, 0xa6, 0x1d, 0x00, 0xc0, 0x22,
    0x02, 0x4d, 0xfc, 0xda, 0xb8, 0xca, 0xf4, 0x01, 0x42, 0xb0, 0xdb, 0xc5, 0x64, 0xa3, 0x81, 0xab, 0x30, 0x81, 0xa8,
    0x30, 0x0e, 0x06, 0x03, 0x55, 0x1d, 0x0f, 0x01, 0x01, 0xff, 0x04, 0x04, 0x03, 0x02, 0x01, 0x06, 0x30, 0x45, 0x06,
    0x03, 0x55, 0x1d, 0x25, 0x04, 0x3e, 0x30, 0x3c, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01, 0x06,
    0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x03,
    0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x08, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03,
    0x04, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x09, 0x30, 0x0f, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x04,
    0x08, 0x30, 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x01, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23, 0x04, 0x18, 0x30,
    0x16, 0x80, 0x14, 0x8d, 0x6e, 0x69, 0x75, 0xb3, 0x85, 0xe4, 0x19, 0xaf, 0x9f, 0xe3, 0x2b, 0x0f, 0xec, 0x86, 0xea,
    0xd7, 0x09, 0xef, 0x2a, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0x8d, 0x6e, 0x69, 0x75,
    0xb3, 0x85, 0xe4, 0x19, 0xaf, 0x9f, 0xe3, 0x2b, 0x0f, 0xec, 0x86, 0xea, 0xd7, 0x09, 0xef, 0x2a, 0x30, 0x0a, 0x06,
    0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x03, 0x49, 0x00, 0x30, 0x46, 0x02, 0x21, 0x00, 0xda, 0x36,
    0x71, 0x53, 0xaf, 0xae, 0x8b, 0x9e, 0x16, 0x18, 0xfb, 0x9e, 0x42, 0xdb, 0x7e, 0x08, 0xec, 0xc5, 0x81, 0xbd, 0xe5,
    0x37, 0x44, 0xb8, 0x39, 0xa5, 0xc5, 0x73, 0x6f, 0x91, 0x18, 0x50, 0x02, 0x21, 0x00, 0x93, 0x78, 0xfb, 0x88, 0x28,
    0x17, 0x7a, 0xeb, 0x6f, 0x4d, 0xff, 0xa3, 0x60, 0x2c, 0x98, 0xf3, 0x0b, 0x2d, 0x9f, 0x94, 0x8d, 0x38, 0xde, 0x09,
    0x7a, 0xd3, 0x5d, 0x4c, 0xb1, 0x06, 0xdd, 0x8d};

#define RCS_URL_LEN         32
#define DEF_RCS_URL0        "h3.iot-dns.com"
#define DEF_RCS_URL0_REGION "h3-%s.iot-dns.com"

#if (TUYA_SECURITY_LEVEL == TUYA_SL_0)
#define DEF_RCS_URLX        NULL
#define DEF_RCS_URLX_REGION NULL
#define DEF_RCS_CA          NULL
#elif (TUYA_SECURITY_LEVEL == TUYA_SL_1)
#define DEF_RCS_URLX        "h6.iot-dns.com"
#define DEF_RCS_URLX_REGION "h6-%s.iot-dns.com"
#define DEF_RCS_CA          default_iotdns_cacert
#elif ((TUYA_SECURITY_LEVEL == TUYA_SL_2) || (TUYA_SECURITY_LEVEL == TUYA_SL_3))
#define DEF_RCS_URLX        "h4.iot-dns.com"
#define DEF_RCS_URLX_REGION "h4-%s.iot-dns.com"
#define DEF_RCS_CA          default_iotdns_cacert
#else
#error "TUYA_SECURITY_LEVEL is invalid"
#endif

static register_center_t s_def_rcs = {.source = RCS_CODE,
                                      .pub = TRUE,
                                      .code = NULL,
                                      .url0 = DEF_RCS_URL0,
                                      .urlx = DEF_RCS_URLX,
                                      .ca_cert = (uint8_t *)DEF_RCS_CA,
                                      .ca_cert_len = sizeof(DEF_RCS_CA)};

#define TO_STRING(x)  #x
#define RCS_URLX(x)   ("url" TO_STRING(x))
#define RCS_KV_MF     "rcs.mf"
#define RCS_KV_ACTIVE "rcs.active"

typedef enum { TLV_SRC = 1, TLV_PUB, TLV_CODE, TLV_URL0, TLV_URLX, TLV_CA, TLV_MAX } RCS_TLV_E;

static register_center_t s_tuya_rcs = {0};
// static BOOL_T s_default_rsc_init = FALSE;
/**
 * |type|length|value|
 * | 1B |  2B  | ... |
 */
static OPERATE_RET __rcs_serialize(register_center_t *rcs, uint8_t **data, uint16_t *length)
{
    uint32_t total = 4 * TLV_MAX + 1 + 1 + strlen(rcs->url0) + 1;

    if (rcs->code) {
        total += strlen(rcs->code);
    }

#if (TUYA_SECURITY_LEVEL != TUYA_SL_0)
    uint8_t *out = NULL;
    uint32_t outlen = 0;
    OPERATE_RET rt = OPRT_OK;
    rt = tuya_x509_pem2der(rcs->ca_cert, rcs->ca_cert_len, &out, &outlen);
    if (OPRT_OK != rt) {
        PR_ERR("pem2der err,rt:%d", rt);
        return OPRT_INVALID_PARM;
    }

    total += outlen;
    total += strlen(rcs->urlx);
#endif

    uint8_t *buf = tal_malloc(total);
    if (NULL == buf) {
        PR_ERR("tal_malloc err");
        return OPRT_MALLOC_FAILED;
    }

    memset(buf, 0, total);

    uint16_t offset = 0;
    uint16_t len = 0;

    len = 1;
    buf[offset++] = TLV_SRC;
    buf[offset++] = len & 0xFF;
    buf[offset++] = (len >> 8) & 0xFF;
    buf[offset++] = rcs->source;

    len = 1;
    buf[offset++] = TLV_PUB;
    buf[offset++] = len & 0xFF;
    buf[offset++] = (len >> 8) & 0xFF;
    buf[offset++] = rcs->pub;

    if (rcs->code) {
        len = strlen(rcs->code) + 1;
        buf[offset++] = TLV_CODE;
        buf[offset++] = len & 0xFF;
        buf[offset++] = (len >> 8) & 0xFF;
        memcpy(&buf[offset], rcs->code, len);
        offset += len;
    }

    len = strlen(rcs->url0) + 1;
    buf[offset++] = TLV_URL0;
    buf[offset++] = len & 0xFF;
    buf[offset++] = (len >> 8) & 0xFF;
    memcpy(&buf[offset], rcs->url0, len);
    offset += len;

#if (TUYA_SECURITY_LEVEL != TUYA_SL_0)
    len = strlen(rcs->urlx) + 1;
    buf[offset++] = TLV_URLX;
    buf[offset++] = len & 0xFF;
    buf[offset++] = (len >> 8) & 0xFF;
    memcpy(&buf[offset], rcs->urlx, len);
    offset += len;

    len = outlen;
    buf[offset++] = TLV_CA;
    buf[offset++] = len & 0xFF;
    buf[offset++] = (len >> 8) & 0xFF;
    memcpy(&buf[offset], out, len);
    tal_free(out);
#endif

    *data = buf;
    *length = total;

    // tuya_debug_hex_dump("buf", 16, buf, total);
    return OPRT_OK;
}

static OPERATE_RET __rcs_deserialize(uint8_t *data, uint16_t length, register_center_t *rcs)
{
    uint16_t offset = 0;
    uint16_t len = 0;
    RCS_TLV_E type = 0;
    uint8_t *buf = NULL;
    // tuya_debug_hex_dump("data", 16, data, length);
    while ((offset + 3) < length) {
        type = data[offset++];
        if (0 == type) {
            break;
        }
        len = data[offset++];
        len += (data[offset++] << 8);
        if (!len || (len + offset) > length) {
            PR_ERR("len:%d,offset:%d,length:%d", len, offset, length);
            return OPRT_INVALID_PARM;
        }

        if (TLV_SRC == type) {
            rcs->source = data[offset++];
        } else if (TLV_PUB == type) {
            rcs->pub = data[offset++];
        } else {
            buf = tal_malloc(len);
            if (NULL == buf) {
                PR_ERR("malloc err");
                return OPRT_MALLOC_FAILED;
            }
            memcpy(buf, &(data[offset]), len);

            if (TLV_CODE == type) {
                rcs->code = (char *)buf;
            } else if (TLV_URL0 == type) {
                rcs->url0 = (char *)buf;
            } else if (TLV_URLX == type) {
                rcs->urlx = (char *)buf;
            } else if (TLV_CA == type) {
                rcs->ca_cert = buf;
                rcs->ca_cert_len = len;
            } else {
                tal_free(buf);
                PR_ERR("type:%d", type);
                return OPRT_INVALID_PARM;
            }

            offset += len;
        }
    }

    return OPRT_OK;
}

static OPERATE_RET __rcs_reset_cb(void *data)
{
    tal_kv_del(RCS_KV_ACTIVE);
    return OPRT_OK;
}

static OPERATE_RET __rcs_restore(char *data, register_center_t *rcs)
{
    OPERATE_RET rt = OPRT_OK;
    cJSON *root = NULL;

    memset(rcs, 0, sizeof(register_center_t));

    root = cJSON_Parse((char *)data);
    if (NULL == root) {
        rt = OPRT_CJSON_PARSE_ERR;
        goto EXIT;
    }

    cJSON *p_source = cJSON_GetObjectItem(root, "source");
    cJSON *p_code = cJSON_GetObjectItem(root, "code");
    cJSON *p_pub = cJSON_GetObjectItem(root, "pub");
    cJSON *p_url0 = cJSON_GetObjectItem(root, "url0");
    if (!p_source || !p_pub || !p_url0) {
        rt = OPRT_CJSON_GET_ERR;
        goto EXIT;
    }

#if (TUYA_SECURITY_LEVEL != TUYA_SL_0)
    cJSON *p_urlx = cJSON_GetObjectItem(root, RCS_URLX(TUYA_SECURITY_LEVEL));
    cJSON *p_ca = cJSON_GetObjectItem(root, "ca");
    if (!p_urlx || !p_ca) {
        rt = OPRT_CJSON_GET_ERR;
        goto EXIT;
    }
#endif

    rcs->source = p_source->valueint;
    rcs->pub = p_pub->valueint;
    rcs->code = p_code ? mm_strdup(p_code->valuestring) : NULL;
    rcs->url0 = mm_strdup(p_url0->valuestring);
    if ((!rcs->code && p_code) || !rcs->url0) {
        rt = OPRT_MALLOC_FAILED;
        goto EXIT;
    }

#if (TUYA_SECURITY_LEVEL != TUYA_SL_0)
    rcs->urlx = mm_strdup(p_urlx->valuestring);
    rcs->ca_cert = (uint8_t *)mm_strdup(p_ca->valuestring);
    rcs->ca_cert_len = strlen(p_ca->valuestring) + 1;
    if (!rcs->urlx || !rcs->ca_cert) {
        rt = OPRT_MALLOC_FAILED;
        goto EXIT;
    }
#endif

EXIT:
    if (OPRT_OK != rt) {
        PR_NOTICE("rcs restore failed %d", rt);

        tal_free(rcs->code);
        tal_free(rcs->url0);
        tal_free(rcs->urlx);
        tal_free(rcs->ca_cert);
    }

    if (NULL != root) {
        cJSON_Delete(root);
    }

    return rt;
}

static void __rcs_update(register_center_t *rcs)
{
    if (s_tuya_rcs.source != RCS_CODE) {
        if (s_tuya_rcs.url0) {
            tal_free(s_tuya_rcs.url0);
        }
        if (s_tuya_rcs.urlx) {
            tal_free(s_tuya_rcs.urlx);
        }

        if (s_tuya_rcs.code) {
            tal_free(s_tuya_rcs.code);
        }

        if (s_tuya_rcs.ca_cert) {
            tal_free(s_tuya_rcs.ca_cert);
        }
    }
    memcpy(&s_tuya_rcs, rcs, sizeof(register_center_t));
}

/**
 * @brief Init and load register center
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_register_center_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t *data = NULL;
    size_t len = 0;

    if (s_tuya_rcs.url0) {
        return OPRT_OK;
    }

    rt = tal_kv_get(RCS_KV_ACTIVE, &data, &len);
    if (OPRT_OK != rt) {
        rt = tal_kv_get(RCS_KV_MF, &data, &len);
        if (OPRT_OK != rt) {
            goto EXIT;
        }
    }

    rt = __rcs_deserialize(data, len, &s_tuya_rcs);
    tal_kv_free(data);

EXIT:

    if (rt != OPRT_OK) {
        PR_DEBUG("use default rcs [%d]", rt);
        memcpy(&s_tuya_rcs, &s_def_rcs, sizeof(register_center_t));
    }
    // tal_event_subscribe(EVENT_RESET, "rcs", __rcs_reset_cb, 0);

    return OPRT_OK;
}

/**
 * @brief Save register center
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_register_center_save(RCS_E source, cJSON *rcs)
{
    OPERATE_RET rt = OPRT_OK;
    char *key = NULL;
    char *data = NULL;
    uint16_t length = 0;

    if (!rcs) {
        return OPRT_INVALID_PARM;
    }

    cJSON_AddNumberToObject(rcs, "source", source);
    data = cJSON_PrintUnformatted(rcs);
    if (NULL == data) {
        return OPRT_CJSON_PARSE_ERR;
    }

    register_center_t tmp_rcs = {0};
    rt = __rcs_restore(data, &tmp_rcs);
    if (OPRT_OK != rt) {
        cJSON_free(data);
        return OPRT_CJSON_GET_ERR;
    }

    cJSON_free(data);

    data = NULL;
    rt = __rcs_serialize(&tmp_rcs, (uint8_t **)&data, &length);
    if (OPRT_OK != rt) {
        return OPRT_CR_CJSON_ERR;
    }

    if (RCS_MF == source) {
        key = RCS_KV_MF;
    } else {
        key = RCS_KV_ACTIVE;
    }

    rt = tal_kv_set(key, (uint8_t *)data, length);
    if (OPRT_OK == rt) {
        __rcs_update(&tmp_rcs);
    }
    tal_free(data);

    tal_event_publish(EVENT_RSC_UPDATE, NULL);

    return rt;
}

/**
 * @brief Get register center
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_register_center_get(register_center_t *rcs)
{
    memcpy(rcs, &s_tuya_rcs, sizeof(register_center_t));
    return OPRT_OK;
}

/**
 * @brief get register center url
 *
 * @return url address
 */
char *tuya_register_center_get_url(void)
{
    if (s_tuya_rcs.urlx) {
        PR_DEBUG("rcs.urlx:%s", s_tuya_rcs.urlx);
        return s_tuya_rcs.urlx;
    } else {
        PR_DEBUG("rcs.url0:%s", s_tuya_rcs.url0);
        return s_tuya_rcs.url0;
    }
}

/**
 * @brief Update ca cert of register center
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_register_center_update(uint8_t *ca_cert, uint32_t ca_cert_len)
{
    OPERATE_RET rt = OPRT_OK;

    if ((NULL == ca_cert) || (0 == ca_cert_len)) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == s_tuya_rcs.url0) {
        return OPRT_RESOURCE_NOT_READY;
    }

    cJSON *rcs = cJSON_CreateObject();
    cJSON_AddBoolToObject(rcs, "pub", s_tuya_rcs.pub);
    cJSON_AddStringToObject(rcs, "url0", s_tuya_rcs.url0);
    if (s_tuya_rcs.code) {
        cJSON_AddStringToObject(rcs, "code", s_tuya_rcs.code);
    } else {
        cJSON_AddNullToObject(rcs, "code");
    }

    cJSON_AddStringToObject(rcs, "ca", (char *)ca_cert);
    if (s_tuya_rcs.urlx) {
        cJSON_AddStringToObject(rcs, RCS_URLX(TUYA_SECURITY_LEVEL), s_tuya_rcs.urlx);
    } else {
        cJSON_AddNullToObject(rcs, RCS_URLX(TUYA_SECURITY_LEVEL));
    }

    rt = tuya_register_center_save(RCS_APP, rcs);
    cJSON_Delete(rcs);

    return rt;
}

/**
 * @brief Reset register center
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_register_center_reset(void)
{
    return __rcs_reset_cb(NULL);
}
