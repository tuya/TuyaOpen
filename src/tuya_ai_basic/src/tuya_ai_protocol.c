/**
 * @file tuya_ai_protocol.c
 * @brief This file contains the implementation of Tuya AI protocol processing,
 * including encryption/decryption, message packaging and cloud communication.
 *
 * The Tuya AI protocol module provides core cryptographic functionalities for
 * secure AI service communication, implementing mbedTLS-based encryption (HKDF,
 * ChaCha20) and custom protocol message handling with JSON payload support.
 *
 * Key features include:
 * - Secure communication using mbedTLS cryptographic primitives
 * - Configurable timeout settings (AI_DEFAULT_TIMEOUT_MS)
 * - Cloud service configuration (AI_ATOP_THING_CONFIG_INFO)
 * - Protocol buffer management (AI_ADD_PKT_LEN)
 * - Default business tag handling (AI_DEFAULT_BIZ_TAG)
 * - Socket buffer size configuration (AI_READ_SOCKET_BUF_SIZE)
 * - Integration with Tuya transporter and IoT core services
 * - Cross-platform cipher operations through cipher_wrapper
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdint.h>

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tuya_transporter.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/chacha20.h"
#include "mix_method.h"
#include "tuya_iot.h"
#include "cJSON.h"
#include "tal_log.h"
#include "uni_random.h"
#include "tal_system.h"
#include "tal_hash.h"
#include "cipher_wrapper.h"
#include "tal_security.h"
#include "tal_memory.h"
#include "tuya_ai_protocol.h"
#include "tuya_ai_private.h"

#define AI_DEFAULT_TIMEOUT_MS     5000
#define AI_ATOP_THING_CONFIG_INFO "thing.aigc.basic.server.config.info"
#define AI_ADD_PKT_LEN            128
#define AI_DEFAULT_BIZ_TAG        0

#ifndef AI_READ_SOCKET_BUF_SIZE
#define AI_READ_SOCKET_BUF_SIZE 0
#endif
#ifndef AI_WRITE_SOCKET_BUF_SIZE
#define AI_WRITE_SOCKET_BUF_SIZE 0
#endif

typedef struct {
    AI_FRAG_FLAG frag_flag;
    uint32_t offset;
    char *data;
} AI_BASIC_FRAG_MNG_T;

typedef struct {
    AI_ATOP_CFG_INFO_T config;
    MUTEX_HANDLE mutex;
    tuya_transporter_t transporter;
    char crypt_key[AI_KEY_LEN + 1];
    char sign_key[AI_KEY_LEN + 1];
    uint16_t sequence_in;
    uint16_t sequence_out;
    char crypt_random[AI_RANDOM_LEN + 1];
    char sign_random[AI_RANDOM_LEN + 1];
    AI_PACKET_SL sl;
    uint8_t connected;
    char *connection_id;
    char encrypt_iv[AI_IV_LEN + 1];
    char decrypt_iv[AI_IV_LEN + 1];
    AI_BASIC_FRAG_MNG_T frag_mng;
    char recv_buf[AI_MAX_FRAGMENT_LENGTH + AI_ADD_PKT_LEN];
} AI_BASIC_PROTO_T;

static AI_BASIC_PROTO_T *ai_basic_proto = NULL;

static void __ai_atop_cfg_free(void)
{
    uint32_t idx = 0;
    if (ai_basic_proto->config.username) {
        Free(ai_basic_proto->config.username);
    }
    if (ai_basic_proto->config.credential) {
        Free(ai_basic_proto->config.credential);
    }
    if (ai_basic_proto->config.client_id) {
        Free(ai_basic_proto->config.client_id);
    }
    if (ai_basic_proto->config.derived_algorithm) {
        Free(ai_basic_proto->config.derived_algorithm);
    }
    if (ai_basic_proto->config.derived_iv) {
        Free(ai_basic_proto->config.derived_iv);
    }
    if (ai_basic_proto->config.hosts) {
        for (idx = 0; idx < ai_basic_proto->config.host_num; idx++) {
            Free(ai_basic_proto->config.hosts[idx]);
        }
        Free(ai_basic_proto->config.hosts);
    }
    memset(&ai_basic_proto->config, 0, sizeof(AI_ATOP_CFG_INFO_T));
}

AI_ATOP_CFG_INFO_T *tuya_ai_basic_get_atop_cfg(void)
{
    return &(ai_basic_proto->config);
}

static OPERATE_RET __ai_generate_crypt_key()
{
    OPERATE_RET rt = OPRT_OK;

    uni_random_string(ai_basic_proto->crypt_random, AI_RANDOM_LEN);

    char *slat = ai_basic_proto->crypt_random;
    size_t salt_len = AI_RANDOM_LEN;

    char *ikm = tuya_iot_client_get()->activate.localkey;
    size_t ikm_len = strlen(ikm);

    char *info = NULL;
    size_t info_len = 0;

    rt = mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const unsigned char *)slat, salt_len,
                      (const unsigned char *)ikm, ikm_len, (const unsigned char *)info, info_len,
                      (unsigned char *)ai_basic_proto->crypt_key, AI_KEY_LEN);
    return rt;
}

static char *__ai_get_crypt_key(void)
{
    return ai_basic_proto->crypt_key;
}

static OPERATE_RET __ai_generate_sign_key()
{
    OPERATE_RET rt = OPRT_OK;

    uni_random_string(ai_basic_proto->sign_random, AI_RANDOM_LEN);

    char *slat = ai_basic_proto->sign_random;
    size_t salt_len = AI_RANDOM_LEN;

    char *ikm = tuya_iot_client_get()->activate.localkey;
    size_t ikm_len = strlen(ikm);

    char *info = NULL;
    size_t info_len = 0;

    rt = mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const unsigned char *)slat, salt_len,
                      (const unsigned char *)ikm, ikm_len, (const unsigned char *)info, info_len,
                      (unsigned char *)ai_basic_proto->sign_key, AI_KEY_LEN);
    return rt;
}

static char *__ai_get_sign_key(void)
{
    return ai_basic_proto->sign_key;
}

static AI_PACKET_SL __ai_get_sl(AI_PACKET_PT type, uint8_t is_decrypt)
{
    if (is_decrypt) {
        return ai_basic_proto->sl;
    } else {
        if (type == AI_PT_CLIENT_HELLO) {
            return AI_PACKET_SL0;
        }
        return ai_basic_proto->sl;
    }
}

static void __ai_basic_proto_deinit(void)
{
    if (ai_basic_proto) {
        if (ai_basic_proto->transporter) {
            tuya_transporter_close(ai_basic_proto->transporter);
            tuya_transporter_destroy(ai_basic_proto->transporter);
            ai_basic_proto->transporter = NULL;
        }
        if (ai_basic_proto->mutex) {
            tal_mutex_release(ai_basic_proto->mutex);
        }
        __ai_atop_cfg_free();
        if (ai_basic_proto->connection_id) {
            Free(ai_basic_proto->connection_id);
            ai_basic_proto->connection_id = NULL;
        }
        Free(ai_basic_proto);
        ai_basic_proto = NULL;
    }
    return;
}

static void __ai_basic_proto_reinit(void)
{
    tal_mutex_lock(ai_basic_proto->mutex);
    if (ai_basic_proto->transporter) {
        tuya_transporter_close(ai_basic_proto->transporter);
        tuya_transporter_destroy(ai_basic_proto->transporter);
        ai_basic_proto->transporter = NULL;
    }
    __ai_atop_cfg_free();
    if (ai_basic_proto->connection_id) {
        OS_FREE(ai_basic_proto->connection_id);
        ai_basic_proto->connection_id = NULL;
    }
    __ai_generate_crypt_key();
    __ai_generate_sign_key();
    ai_basic_proto->connected = FALSE;
    ai_basic_proto->sequence_in = 0;
    ai_basic_proto->sequence_out = 1;
    memset(ai_basic_proto->recv_buf, 0, sizeof(ai_basic_proto->recv_buf));
    memset(ai_basic_proto->encrypt_iv, 0, AI_IV_LEN);
    uni_random_string(ai_basic_proto->encrypt_iv, AI_IV_LEN);
    ai_basic_proto->sl = AI_PACKET_SECURITY_LEVEL;
    memset(ai_basic_proto->decrypt_iv, 0, AI_IV_LEN);
    memset(&ai_basic_proto->frag_mng, 0, sizeof(ai_basic_proto->frag_mng));
    tal_mutex_unlock(ai_basic_proto->mutex);
    PR_NOTICE("ai proto reinit success");
    return;
}

static OPERATE_RET __ai_basic_proto_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    if (ai_basic_proto) {
        __ai_basic_proto_reinit();
    }
    ai_basic_proto = Malloc(sizeof(AI_BASIC_PROTO_T));
    TUYA_CHECK_NULL_RETURN(ai_basic_proto, OPRT_MALLOC_FAILED);
    memset(ai_basic_proto, 0, sizeof(AI_BASIC_PROTO_T));
    TUYA_CALL_ERR_GOTO(__ai_generate_crypt_key(), EXIT);
    TUYA_CALL_ERR_GOTO(__ai_generate_sign_key(), EXIT);
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&ai_basic_proto->mutex), EXIT);
    ai_basic_proto->sequence_out = 1;
    uni_random_string(ai_basic_proto->encrypt_iv, AI_IV_LEN);
    ai_basic_proto->sl = AI_PACKET_SECURITY_LEVEL;
    PR_NOTICE("ai proto init success, sl:%d", ai_basic_proto->sl);
    return rt;

EXIT:
    PR_ERR("ai proto init failed, rt:%d", rt);
    __ai_basic_proto_deinit();
    return OPRT_COM_ERROR;
}

OPERATE_RET tuya_ai_basic_atop_req(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t idx = 0;
    uint32_t timestamp = 0;

    rt = __ai_basic_proto_init();
    if (OPRT_OK != rt) {
        return rt;
    }

    timestamp = tal_time_get_posix();

    uint64_t bizTag = AI_DEFAULT_BIZ_TAG;
    cJSON *root = cJSON_CreateObject();
    TUYA_CHECK_NULL_RETURN(root, OPRT_MALLOC_FAILED);
    cJSON_AddNumberToObject(root, "bizTag", bizTag);
    cJSON_AddNumberToObject(root, "t", timestamp);
    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    TUYA_CHECK_NULL_RETURN(post_data, OPRT_MALLOC_FAILED);

    PR_DEBUG("post data:%s", post_data);

    cJSON *result = NULL;
    tuya_iot_client_t *iot_hdl = tuya_iot_client_get();
    atop_base_request_t atop_request = {
        .devid = iot_hdl->activate.devid,
        .key = iot_hdl->activate.seckey,
        .path = "/d.json",
        .timestamp = timestamp,
        .api = AI_ATOP_THING_CONFIG_INFO,
        .version = "1.0",
        .data = post_data,
        .datalen = strlen(post_data),
        .user_data = NULL,
    };
    atop_base_response_t response = {0};
    rt = atop_base_request(&atop_request, &response);

    Free(post_data);
    if (OPRT_OK != rt) {
        PR_ERR("http post err, rt:%d", rt);
        return rt;
    }

    result = response.result;
    if (!result) {
        PR_ERR("http post err, no result");
        return OPRT_COM_ERROR;
    }

    cJSON *tcpport = cJSON_GetObjectItem(result, "tcpport");
    cJSON *username = cJSON_GetObjectItem(result, "username");
    cJSON *credential = cJSON_GetObjectItem(result, "credential");
    cJSON *hosts = cJSON_GetObjectItem(result, "hosts");
    cJSON *udpport = cJSON_GetObjectItem(result, "udpport");
    cJSON *expire = cJSON_GetObjectItem(result, "expire");
    cJSON *bizCode = cJSON_GetObjectItem(result, "bizCode");
    cJSON *clientId = cJSON_GetObjectItem(result, "clientId");
    cJSON *derivedAlgorithm = cJSON_GetObjectItem(result, "derivedAlgorithm");
    cJSON *derivedIv = cJSON_GetObjectItem(result, "derivedIv");

    if (!tcpport || !username || !credential || !hosts || !expire || !bizCode || !clientId || !derivedAlgorithm ||
        !derivedIv) {
        PR_ERR("get ai config err");
        rt = OPRT_COM_ERROR;
        goto EXIT;
    }

    ai_basic_proto->config.host_num = cJSON_GetArraySize(hosts);
    if (ai_basic_proto->config.host_num <= 0) {
        rt = OPRT_COM_ERROR;
        goto EXIT;
    }
    ai_basic_proto->config.tcp_port = tcpport->valueint;
    if (udpport) {
        ai_basic_proto->config.udp_port = udpport->valueint;
    }
    ai_basic_proto->config.expire = expire->valuedouble;
    ai_basic_proto->config.biz_code = bizCode->valueint;
    ai_basic_proto->config.username = mm_strdup(username->valuestring);
    ai_basic_proto->config.credential = mm_strdup(credential->valuestring);
    ai_basic_proto->config.client_id = mm_strdup(clientId->valuestring);
    ai_basic_proto->config.derived_algorithm = mm_strdup(derivedAlgorithm->valuestring);
    ai_basic_proto->config.derived_iv = mm_strdup(derivedIv->valuestring);
    ai_basic_proto->config.hosts = Malloc(ai_basic_proto->config.host_num * sizeof(char *));
    if ((!ai_basic_proto->config.hosts) || (!ai_basic_proto->config.username) || (!ai_basic_proto->config.credential) ||
        (!ai_basic_proto->config.client_id) || (!ai_basic_proto->config.derived_algorithm) ||
        (!ai_basic_proto->config.derived_iv)) {
        rt = OPRT_MALLOC_FAILED;
        goto EXIT;
    }

    AI_PROTO_D("expire:%lld, biz_code:%d", ai_basic_proto->config.expire, ai_basic_proto->config.biz_code);
    AI_PROTO_D("username: %s, pwd: %s", ai_basic_proto->config.username, ai_basic_proto->config.credential);
    AI_PROTO_D("clinet_id: %s", ai_basic_proto->config.client_id);

    for (idx = 0; idx < ai_basic_proto->config.host_num; idx++) {
        cJSON *host = cJSON_GetArrayItem(hosts, idx);
        ai_basic_proto->config.hosts[idx] = mm_strdup(host->valuestring);
        AI_PROTO_D("host[%d]: %s", idx, ai_basic_proto->config.hosts[idx]);
        if (!ai_basic_proto->config.hosts[idx]) {
            rt = OPRT_MALLOC_FAILED;
            goto EXIT;
        }
    }
    atop_base_response_free(&response);
    return rt;

EXIT:
    atop_base_response_free(&response);
    __ai_atop_cfg_free();
    return rt;
}

static uint32_t __ai_get_head_len(char *buf)
{
    uint32_t head_len = 0;
    AI_PACKET_HEAD_T *head = (AI_PACKET_HEAD_T *)buf;
    head_len = sizeof(AI_PACKET_HEAD_T);
    if (head->iv_flag) {
        head_len += AI_IV_LEN;
    }
    head_len += sizeof(head_len);
    return head_len;
}

static uint32_t __ai_get_packet_len(char *buf)
{
    uint32_t packet_len = 0;
    uint32_t head_len = sizeof(AI_PACKET_HEAD_T);
    AI_PACKET_HEAD_T *head = (AI_PACKET_HEAD_T *)buf;
    if (head->iv_flag) {
        memcpy(&packet_len, buf + head_len + AI_IV_LEN, sizeof(packet_len));
    } else {
        memcpy(&packet_len, buf + head_len, sizeof(packet_len));
    }
    packet_len = UNI_NTOHL(packet_len);
    return packet_len;
}

static uint32_t __ai_get_payload_len(char *buf)
{
    return __ai_get_packet_len(buf) - AI_SIGN_LEN;
}

static OPERATE_RET __ai_packet_sign(char *buf, uint8_t *signature)
{
    OPERATE_RET rt = OPRT_OK;
    char *sign_key = __ai_get_sign_key();
    TUYA_CHECK_NULL_RETURN(sign_key, OPRT_COM_ERROR);

    uint32_t head_len = __ai_get_head_len(buf);
    uint32_t payload_len = __ai_get_payload_len(buf);

    // transport first 32 byte and packet last 32 byte, if less than 64 byte,use all packet
    uint8_t sign_data[64] = {0};
    uint32_t sign_len = 0;

    AI_PROTO_D("start sign head_len:%d, payload_len:%d", head_len, payload_len);
    if (head_len + payload_len <= sizeof(sign_data)) {
        memcpy(sign_data, buf, head_len + payload_len);
        sign_len = head_len + payload_len;
    } else {
        memcpy(sign_data, buf, 32);
        char *payload = buf + head_len;
        uint32_t offset = (payload_len > 32) ? payload_len - 32 : 0;
        uint32_t copy_len = (payload_len > 32) ? 32 : payload_len;
        memcpy(sign_data + 32, payload + offset, copy_len);
        sign_len = sizeof(sign_data);
    }

    rt = tal_sha256_mac((uint8_t *)sign_key, AI_KEY_LEN, (uint8_t *)sign_data, sign_len, signature);
    if (OPRT_OK != rt) {
        PR_ERR("sign packet failed, rt:%d", rt);
    }
    return rt;
}

uint32_t __ai_get_send_attr_len(AI_SEND_PACKET_T *info)
{
    uint32_t len = 0, idx = 0;
    if ((info->count != 0) && (info->attrs)) {
        len += sizeof(len);
        for (idx = 0; idx < info->count; idx++) {
            len += OFFSOF(AI_ATTRIBUTE_T, value) + info->attrs[idx]->length;
        }
    }
    // AI_PROTO_D("attr len:%d", len);
    return len;
}

uint8_t tuya_ai_is_need_attr(AI_FRAG_FLAG frag_flag)
{
    if ((frag_flag == AI_PACKET_NO_FRAG) || (frag_flag == AI_PACKET_FRAG_START)) {
        return true;
    }
    return false;
}

uint32_t __ai_get_send_payload_len(AI_SEND_PACKET_T *info, AI_FRAG_FLAG frag_flag)
{
    uint32_t len = 0;
    if (tuya_ai_is_need_attr(frag_flag)) {
        len += sizeof(AI_PAYLOAD_HEAD_T);
        len += __ai_get_send_attr_len(info);
        len += sizeof(len);
    }
    len += info->len;
    // AI_PROTO_D("packet len:%d", len);
    return len;
}

static uint8_t __ai_is_need_iv(AI_PACKET_PT type, AI_FRAG_FLAG frag_flag)
{
    if ((AI_PT_CLIENT_HELLO == type) || (AI_PACKET_FRAG_ING == frag_flag) || (AI_PACKET_FRAG_END == frag_flag)) {
        return false;
    }
    return true;
}

static uint32_t __ai_get_send_pkt_len(AI_SEND_PACKET_T *info, AI_FRAG_FLAG frag_flag)
{
    uint32_t len = 0;
    AI_PACKET_PT type = __ai_get_sl(info->type, false);
    len = sizeof(AI_PACKET_HEAD_T);
    if (__ai_is_need_iv(info->type, frag_flag)) {
        len += AI_IV_LEN;
    }
    len += sizeof(len);
    len += __ai_get_send_payload_len(info, frag_flag);
    len += AI_SIGN_LEN;
    if (type != AI_PACKET_SL0) {
        len += AI_ADD_PKT_LEN; // for tag and padding
        // if (type == AI_PACKET_SL4) {
        //     len += AI_GCM_TAG_LEN;
        // } else {
        //     len += AI_ADD_PKT_LEN;
        // }
    }
    AI_PROTO_D("uncrypt len:%d", len);
    return len;
}

static int __ai_encrypt_add_pkcs(char *p, uint32_t len)
{
    char pkcs[16];
    int cz = 0;
    int i = 0;

    cz = len < 16 ? (16 - len) : (16 - len % 16);
    memset(pkcs, 0, sizeof(pkcs));
    for (i = 0; i < cz; i++) {
        pkcs[i] = cz;
    }
    memcpy(p + len, pkcs, cz);
    return (len + cz);
}

static OPERATE_RET __ai_encrypt_packet(AI_PACKET_PT type, char *data, uint32_t len, char *output, uint32_t *en_len)
{
    OPERATE_RET rt = OPRT_OK;
    int data_out_len = 0;
    char *key = __ai_get_crypt_key();
    TUYA_CHECK_NULL_RETURN(key, OPRT_COM_ERROR);

    AI_PACKET_SL sl = __ai_get_sl(type, false);
    if (sl == AI_PACKET_SL2) {
#if (AI_PACKET_SECURITY_LEVEL == AI_PACKET_SL2)
        memcpy(output, data, len);
        data_out_len = __ai_encrypt_add_pkcs(output, len);
        char nonce[12] = {0};
        memcpy(nonce, ai_basic_proto->encrypt_iv, sizeof(nonce));
        rt = mbedtls_chacha20_crypt((uint8_t *)key, (uint8_t *)nonce, 0, len, (uint8_t *)data, (uint8_t *)output);
        if (OPRT_OK != rt) {
            PR_ERR("chacha20_crypt error:%d", rt);
            return rt;
        }
        *en_len = data_out_len;
#endif
    } else if (sl == AI_PACKET_SL3) {
#if (AI_PACKET_SECURITY_LEVEL == AI_PACKET_SL3)
        memcpy(output, data, len);
        data_out_len = tal_pkcs7padding_buffer((uint8_t *)output, len);
        rt = tal_aes256_cbc_encode_raw((uint8_t *)output, data_out_len, (uint8_t *)key,
                                       (uint8_t *)ai_basic_proto->encrypt_iv, (uint8_t *)output);
        if (OPRT_OK != rt) {
            PR_ERR("aes128_cbc_encode error:%d", rt);
            return rt;
        }
        *en_len = data_out_len;
#endif
    } else if (sl == AI_PACKET_SL4) {
#if (AI_PACKET_SECURITY_LEVEL == AI_PACKET_SL4)
        uint8_t tag[AI_GCM_TAG_LEN] = {0};
        memcpy(output, data, len);
        data_out_len = __ai_encrypt_add_pkcs(output, len);

        const cipher_params_t en_input = {
            .cipher_type = MBEDTLS_CIPHER_AES_256_GCM,
            .key = (unsigned char *)key,
            .key_len = AI_KEY_LEN,
            .nonce = (uint8_t *)ai_basic_proto->encrypt_iv,
            .nonce_len = AI_IV_LEN,
            .ad = NULL,
            .ad_len = 0,
            .data = (uint8_t *)output,
            .data_len = data_out_len,
        };
        rt = mbedtls_cipher_auth_encrypt_wrapper(&en_input, (uint8_t *)output, (size_t *)en_len, tag, sizeof(tag));
        if (rt != OPRT_OK) {
            PR_ERR("aes128_gcm_encode error:%x", rt);
        }
        memcpy(output + *en_len, tag, sizeof(tag));
        *en_len += sizeof(tag);
        // tuya_debug_hex_dump("encrypt_data", 64, (uint8_t *)output, *en_len);
#endif
    } else if (sl == AI_PACKET_SL0) {
        AI_PROTO_D("sl:%d do not need crypt", sl);
        memcpy(output, data, len);
        *en_len = len;
    } else {
        PR_ERR("sl:%d err", sl);
        rt = OPRT_COM_ERROR;
    }

    return rt;
}

static OPERATE_RET __ai_decrypt_packet(char *data, uint32_t len, char *output, uint32_t *de_len)
{
    OPERATE_RET rt = OPRT_OK;
    char *key = __ai_get_crypt_key();
    TUYA_CHECK_NULL_RETURN(key, OPRT_COM_ERROR);

    AI_PACKET_SL sl = __ai_get_sl(0, true);
    if (sl == AI_PACKET_SL2) {
#if (AI_PACKET_SECURITY_LEVEL == AI_PACKET_SL2)
        char nonce[12] = {0};
        memcpy(nonce, ai_basic_proto->decrypt_iv, sizeof(nonce));
        rt = mbedtls_chacha20_crypt((uint8_t *)key, (uint8_t *)nonce, 0, len, (uint8_t *)data, (uint8_t *)output);
        if (OPRT_OK != rt) {
            PR_ERR("chacha20_crypt error:%d", rt);
            return rt;
        }
        *de_len = len - output[len - 1];
#endif
    } else if (sl == AI_PACKET_SL3) {
#if (AI_PACKET_SECURITY_LEVEL == AI_PACKET_SL3)
        rt = tal_aes256_cbc_decode_raw((uint8_t *)data, len, (uint8_t *)key, (uint8_t *)ai_basic_proto->decrypt_iv,
                                       (uint8_t *)output);
        if (OPRT_OK != rt) {
            PR_ERR("aes128_cbc_decode error:%d", rt);
            return rt;
        }
        *de_len = len - output[len - 1];
#endif
    } else if (sl == AI_PACKET_SL4) {
#if (AI_PACKET_SECURITY_LEVEL == AI_PACKET_SL4)
        // tuya_debug_hex_dump("decrypt_data", 64, (uint8_t *)data, len - AI_GCM_TAG_LEN);
        // tuya_debug_hex_dump("decrypt_key", 64, (uint8_t *)key, AI_KEY_LEN);
        // tuya_debug_hex_dump("decrypt_iv", 64, (uint8_t *)ai_basic_proto->decrypt_iv, AI_IV_LEN);
        // tuya_debug_hex_dump("decrypt_tag", 64, (uint8_t *)(data + len - AI_GCM_TAG_LEN), AI_GCM_TAG_LEN);
        const cipher_params_t de_input = {
            .cipher_type = MBEDTLS_CIPHER_AES_256_GCM,
            .key = (unsigned char *)key,
            .key_len = AI_KEY_LEN,
            .nonce = (uint8_t *)ai_basic_proto->decrypt_iv,
            .nonce_len = AI_IV_LEN,
            .ad = NULL,
            .ad_len = 0,
            .data = (uint8_t *)data,
            .data_len = len - AI_GCM_TAG_LEN,
        };

        rt = mbedtls_cipher_auth_decrypt_wrapper(&de_input, (uint8_t *)output, (size_t *)de_len,
                                                 (uint8_t *)(data + len - AI_GCM_TAG_LEN), AI_GCM_TAG_LEN);
        if (rt != OPRT_OK) {
            PR_ERR("aes128_gcm_decode error:%x", rt);
            return rt;
        }
        *de_len = *de_len - output[*de_len - 1];
#endif
    } else if (sl == AI_PACKET_SL0) {
        AI_PROTO_D("sl:%d do not need crypt ", sl);
        memcpy(output, data, len);
        *de_len = len;
    } else {
        AI_PROTO_D("sl:%d err", sl);
        rt = OPRT_COM_ERROR;
    }

    return rt;
}

static OPERATE_RET __ai_pack_payload(AI_SEND_PACKET_T *info, char *payload_buf, uint32_t *payload_len,
                                     AI_FRAG_FLAG frag, uint32_t origin_len)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t idx = 0, attr_len = 0, packet_len = 0;
    uint32_t offset = 0;
    TUYA_CHECK_NULL_RETURN(info, OPRT_INVALID_PARM);
    packet_len = __ai_get_send_payload_len(info, frag);

    char *buf = Malloc(packet_len);
    TUYA_CHECK_NULL_RETURN(buf, OPRT_MALLOC_FAILED);
    memset(buf, 0, packet_len);

    if (tuya_ai_is_need_attr(frag)) {
        AI_PAYLOAD_HEAD_T payload_head = {0};
        payload_head.type = info->type;
        payload_head.attribute_flag = AI_NO_ATTR;
        offset = sizeof(AI_PAYLOAD_HEAD_T);
        if (info->count != 0) {
            payload_head.attribute_flag = AI_HAS_ATTR;
            attr_len = UNI_HTONL(__ai_get_send_attr_len(info) - sizeof(attr_len));
            memcpy(buf + offset, &attr_len, sizeof(attr_len));
            offset += sizeof(attr_len);
            for (idx = 0; idx < info->count; idx++) {
                AI_ATTR_TYPE type = UNI_HTONS(info->attrs[idx]->type);
                memcpy(buf + offset, &type, sizeof(AI_ATTR_TYPE));
                offset += sizeof(AI_ATTR_TYPE);
                AI_ATTR_PT payload_type = info->attrs[idx]->payload_type;
                memcpy(buf + offset, &payload_type, sizeof(AI_ATTR_PT));
                offset += sizeof(AI_ATTR_PT);
                uint32_t attr_idx_len = info->attrs[idx]->length;
                uint32_t length = UNI_HTONL(attr_idx_len);
                memcpy(buf + offset, &length, sizeof(length));
                offset += sizeof(length);
                if (payload_type == ATTR_PT_U8) {
                    memcpy(buf + offset, &info->attrs[idx]->value.u8, attr_idx_len);
                } else if (payload_type == ATTR_PT_U16) {
                    uint16_t value = UNI_HTONS(info->attrs[idx]->value.u16);
                    memcpy(buf + offset, &value, attr_idx_len);
                } else if (payload_type == ATTR_PT_U32) {
                    uint32_t value = UNI_HTONL(info->attrs[idx]->value.u32);
                    memcpy(buf + offset, &value, attr_idx_len);
                } else if (payload_type == ATTR_PT_U64) {
                    uint64_t value = info->attrs[idx]->value.u64;
                    UNI_HTONLL(value);
                    memcpy(buf + offset, &value, attr_idx_len);
                } else if (payload_type == ATTR_PT_BYTES) {
                    memcpy(buf + offset, info->attrs[idx]->value.bytes, attr_idx_len);
                } else if (payload_type == ATTR_PT_STR) {
                    memcpy(buf + offset, info->attrs[idx]->value.str, attr_idx_len);
                } else {
                    PR_ERR("unknow payload type:%d", payload_type);
                    Free(buf);
                    return OPRT_COM_ERROR;
                }
                offset += attr_idx_len;
                AI_PROTO_D("attr idx:%d, len:%d", idx, offset);
            }
        }
        memcpy(buf, &payload_head, sizeof(AI_PAYLOAD_HEAD_T));
        uint32_t info_len = UNI_HTONL(origin_len);
        memcpy(buf + offset, &info_len, sizeof(info->len));
        AI_PROTO_D("payload len:%d", info->len);
        offset += sizeof(info->len);
    }

    memcpy(buf + offset, info->data, info->len);
    offset += info->len;
    AI_PROTO_D("payload len:%d, offset:%d", packet_len, offset);

    // tuya_debug_hex_dump("payload_uncrypt", 64, (uint8_t *)buf, packet_len);
    rt = __ai_encrypt_packet(info->type, buf, packet_len, payload_buf, payload_len);
    if (OPRT_OK != rt) {
        PR_ERR("encrypt packet failed, rt:%d", rt);
    }

    Free(buf);
    return rt;
}

static uint8_t __ai_check_attr_vaild(AI_ATTRIBUTE_T *attr)
{
    AI_ATTR_TYPE type = attr->type;
    AI_ATTR_PT payload_type = attr->payload_type;
    uint32_t length = attr->length;

    if ((type == AI_ATTR_CONNECT_STATUS_CODE) || (type == AI_ATTR_CONNECT_CLOSE_ERR_CODE)) {
        if (payload_type != ATTR_PT_U16) {
            return false;
        }
    }
    switch (payload_type) {
    case ATTR_PT_U8:
        if (length != sizeof(uint8_t)) {
            return false;
        }
        break;
    case ATTR_PT_U16:
        if (length != sizeof(uint16_t)) {
            return false;
        }
        break;
    case ATTR_PT_U32:
        if (length != sizeof(uint32_t)) {
            return false;
        }
        break;
    case ATTR_PT_U64:
        if (length != sizeof(uint64_t)) {
            return false;
        }
        break;
    case ATTR_PT_STR:
    case ATTR_PT_BYTES:
        if (length == 0) {
            return false;
        }
        break;
    }
    return true;
}

uint32_t tuya_ai_get_attr_num(char *de_buf, uint32_t attr_len)
{
    uint32_t offset = 0, idx = 0, length = 0;
    while (offset < attr_len) {
        offset += sizeof(AI_ATTR_TYPE);
        offset += sizeof(AI_ATTR_PT);
        memcpy(&length, de_buf + offset, sizeof(length));
        offset += sizeof(length);
        offset += UNI_NTOHL(length);
        idx++;
    }
    return idx;
}

OPERATE_RET tuya_ai_get_attr_value(char *de_buf, uint32_t *offset, AI_ATTRIBUTE_T *attr)
{
    OPERATE_RET rt = OPRT_OK;
    AI_ATTR_TYPE type = 0;
    AI_ATTR_PT payload_type = 0;
    uint32_t length = 0;
    AI_ATTR_VALUE value = {0};
    memcpy(&type, de_buf + *offset, sizeof(type));
    attr->type = UNI_NTOHS(type);
    *offset += sizeof(type);

    memcpy(&payload_type, de_buf + *offset, sizeof(payload_type));
    attr->payload_type = payload_type;
    *offset += sizeof(payload_type);

    memcpy(&length, de_buf + *offset, sizeof(length));
    attr->length = UNI_NTOHL(length);
    *offset += sizeof(length);

    if (payload_type == ATTR_PT_U8) {
        memcpy(&(value.u8), de_buf + *offset, attr->length);
        attr->value.u8 = value.u8;
    } else if (payload_type == ATTR_PT_U16) {
        memcpy(&(value.u16), de_buf + *offset, attr->length);
        attr->value.u16 = UNI_NTOHS(value.u16);
    } else if (payload_type == ATTR_PT_U32) {
        memcpy(&(value.u32), de_buf + *offset, attr->length);
        attr->value.u32 = UNI_NTOHL(value.u32);
    } else if (payload_type == ATTR_PT_U64) {
        memcpy(&(value.u64), de_buf + *offset, attr->length);
        attr->value.u64 = value.u64;
        UNI_NTOHLL(attr->value.u64);
    } else if (payload_type == ATTR_PT_BYTES) {
        attr->value.bytes = (uint8_t *)(de_buf + *offset);
    } else if (payload_type == ATTR_PT_STR) {
        attr->value.str = de_buf + *offset;
    } else {
        PR_ERR("unknow payload type:%d", attr->payload_type);
        return OPRT_COM_ERROR;
    }

    *offset += attr->length;
    if (!__ai_check_attr_vaild(attr)) {
        PR_ERR("attr invaild, type:%d, payload_type:%d, length:%d", attr->type, attr->payload_type, attr->length);
        return OPRT_COM_ERROR;
    }
    return rt;
}

static OPERATE_RET __ai_packet_write(AI_SEND_PACKET_T *info, AI_FRAG_FLAG frag, uint32_t origin_len)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t payload_len = 0, offset = 0;
    AI_PACKET_SL sl = __ai_get_sl(info->type, false);
    uint8_t signature[AI_SIGN_LEN] = {0};
    if (ai_basic_proto->sequence_out >= 0xFFFF) {
        ai_basic_proto->sequence_out = 1;
    }
    uint16_t sequence = ai_basic_proto->sequence_out++;
    AI_PROTO_D("send packet sequence:%d, frag:%d", sequence, frag);

    uint32_t uncrypt_len = __ai_get_send_pkt_len(info, frag);
    if (uncrypt_len > AI_MAX_FRAGMENT_LENGTH) {
        PR_ERR("send packet too long, len: %d", uncrypt_len);
        return OPRT_COM_ERROR;
    }
    char *send_pkt_buf = Malloc(uncrypt_len);
    TUYA_CHECK_NULL_RETURN(send_pkt_buf, OPRT_MALLOC_FAILED);
    memset(send_pkt_buf, 0, uncrypt_len);

    uint32_t head_len = sizeof(AI_PACKET_HEAD_T);
    // AI_PROTO_D("head len:%d", head_len);

    AI_PACKET_HEAD_T head = {0};
    head.version = 0x01;
    head.sequence = UNI_HTONS(sequence);
    head.frag_flag = frag;
    head.security_level = sl;
    head.iv_flag = __ai_is_need_iv(info->type, frag);

    memcpy(send_pkt_buf, &head, head_len);
    offset += head_len;

    if (head.iv_flag) {
        memcpy(send_pkt_buf + offset, ai_basic_proto->encrypt_iv, AI_IV_LEN);
        offset += AI_IV_LEN;
    }

    uint32_t length = 0;
    offset += sizeof(length);

    rt = __ai_pack_payload(info, send_pkt_buf + offset, &payload_len, frag, origin_len);
    if (OPRT_OK != rt) {
        goto EXIT;
    }
    length = UNI_HTONL(payload_len + AI_SIGN_LEN);

    if (head.iv_flag) {
        memcpy(send_pkt_buf + head_len + AI_IV_LEN, &length, sizeof(length));
    } else {
        memcpy(send_pkt_buf + head_len, &length, sizeof(length));
    }

    rt = __ai_packet_sign(send_pkt_buf, signature);
    if (OPRT_OK != rt) {
        goto EXIT;
    }
    offset += payload_len;
    memcpy(send_pkt_buf + offset, signature, AI_SIGN_LEN);
    offset += AI_SIGN_LEN;

    AI_PROTO_D("send packet len:%d", payload_len + AI_SIGN_LEN);
    AI_PROTO_D("send payload len:%d", payload_len);

    AI_PROTO_D("send total len:%d, send_len:%d", offset, uncrypt_len);
    // tuya_debug_hex_dump("send_pkt_buf", 64, (uint8_t *)send_pkt_buf, offset);
    if (ai_basic_proto->transporter) {
        rt = tuya_transporter_write(ai_basic_proto->transporter, (uint8_t *)send_pkt_buf, offset, 0);
    }
    if (rt != offset) {
        PR_ERR("send to cloud failed, rt:%d, len:%d", rt, offset);
    } else {
        rt = OPRT_OK;
    }

EXIT:
    Free(send_pkt_buf);
    return rt;
}

void tuya_ai_free_attribute(AI_ATTRIBUTE_T *attr)
{
    if (!attr) {
        return;
    }
    switch (attr->payload_type) {
    case ATTR_PT_BYTES:
        if (attr->value.bytes) {
            Free(attr->value.bytes);
        }
        break;
    case ATTR_PT_STR:
        if (attr->value.str) {
            Free(attr->value.str);
        }
        break;
    default:
        break;
    }
    Free(attr);
}

void tuya_ai_free_attrs(AI_SEND_PACKET_T *pkt)
{
    uint32_t attr_idx = 0;
    for (attr_idx = 0; attr_idx < pkt->count; attr_idx++) {
        if (pkt->attrs[attr_idx]) {
            tuya_ai_free_attribute(pkt->attrs[attr_idx]);
        }
    }
}

OPERATE_RET tuya_ai_basic_pkt_send(AI_SEND_PACKET_T *info)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0, frag_len = 0, attr_len = 0;
    uint32_t one_packet_len = 0;
    uint32_t min_pkt_len = sizeof(AI_PACKET_HEAD_T) + (2 * AI_ADD_PKT_LEN); // AI_SIGN_LEN + AI_IV_LEN + AI_ADD_PKT_LEN
    uint32_t origin_len = info->len;
    char *origin_data = info->data;
    // AI_PROTO_D("send payload len:%d", payload_len);

    if (!ai_basic_proto) {
        tuya_ai_free_attrs(info);
        PR_ERR("ai basic proto was null");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(ai_basic_proto->mutex);
    if (!ai_basic_proto->connected) {
        tuya_ai_free_attrs(info);
        tal_mutex_unlock(ai_basic_proto->mutex);
        PR_ERR("ai proto not connected");
        return OPRT_COM_ERROR;
    }

    uint32_t send_pkt_len = __ai_get_send_pkt_len(info, AI_PACKET_NO_FRAG);
    if (send_pkt_len <= AI_MAX_FRAGMENT_LENGTH) {
        rt = __ai_packet_write(info, AI_PACKET_NO_FRAG, origin_len);
    } else {
        while (offset < origin_len) {
            if (offset == 0) {
                attr_len = __ai_get_send_attr_len(info);
                one_packet_len = AI_MAX_FRAGMENT_LENGTH - min_pkt_len - attr_len;
            } else {
                one_packet_len = AI_MAX_FRAGMENT_LENGTH - min_pkt_len;
            }
            frag_len = (origin_len - offset) > one_packet_len ? one_packet_len : (origin_len - offset);
            info->data = origin_data + offset;
            info->len = frag_len;
            AI_PROTO_D("offset:%d, frag_len:%d, %d", offset, frag_len, origin_len);
            if (offset == 0) {
                rt = __ai_packet_write(info, AI_PACKET_FRAG_START, origin_len);
            } else if ((offset + frag_len) == origin_len) {
                rt = __ai_packet_write(info, AI_PACKET_FRAG_END, origin_len);
            } else {
                rt = __ai_packet_write(info, AI_PACKET_FRAG_ING, origin_len);
            }
            if (OPRT_OK != rt) {
                AI_PROTO_D("send fragment failed, rt:%d", rt);
                break;
            }
            offset += frag_len;
        }
        info->data = origin_data;
        info->len = origin_len;
    }
    tuya_ai_free_attrs(info);

    tal_mutex_unlock(ai_basic_proto->mutex);
    return rt;
}

static uint8_t __ai_check_attr_created(AI_SEND_PACKET_T *pkt)
{
    uint32_t idx = 0;
    for (idx = 0; idx < pkt->count; idx++) {
        if (pkt->attrs[idx] == NULL) {
            PR_ERR("attr[%d] is null", idx);
            break;
        }
    }
    if (idx != pkt->count) {
        tuya_ai_free_attrs(pkt);
        return false;
    }
    return true;
}

AI_ATTRIBUTE_T *tuya_ai_create_attribute(AI_ATTR_TYPE type, AI_ATTR_PT payload_type, void *value, uint32_t len)
{
    AI_ATTRIBUTE_T *attr = (AI_ATTRIBUTE_T *)Malloc(sizeof(AI_ATTRIBUTE_T));
    if (!attr) {
        PR_ERR("malloc attr failed");
        return NULL;
    }
    memset(attr, 0, sizeof(AI_ATTRIBUTE_T));
    attr->type = type;
    attr->payload_type = payload_type;
    attr->length = len;
    switch (payload_type) {
    case ATTR_PT_U8:
        attr->value.u8 = *(uint8_t *)value;
        AI_PROTO_D("add value:%d", attr->value.u8);
        break;
    case ATTR_PT_U16:
        attr->value.u16 = *(uint16_t *)value;
        AI_PROTO_D("add value:%d", attr->value.u16);
        break;
    case ATTR_PT_U32:
        attr->value.u32 = *(uint32_t *)value;
        AI_PROTO_D("add value:%ld", attr->value.u32);
        break;
    case ATTR_PT_U64:
        attr->value.u64 = *(uint64_t *)value;
        AI_PROTO_D("add value:%llu", attr->value.u64);
        break;
    case ATTR_PT_BYTES:
        attr->value.bytes = Malloc(len);
        memset(attr->value.bytes, 0, len);
        if (attr->value.bytes) {
            memcpy(attr->value.bytes, value, len);
            // tuya_debug_hex_dump("AI_ATTR_VALUE", 64, attr->value.bytes, len);
        }
        break;
    case ATTR_PT_STR:
        attr->value.str = mm_strdup((char *)value);
        AI_PROTO_D("add value:%s", attr->value.str);
        break;
    default:
        PR_ERR("invalid payload type");
        break;
    }
    AI_PROTO_D("add attr type:%d, payload_type:%d, len:%d", type, payload_type, len);
    return attr;
}

static OPERATE_RET __create_conn_close_attrs(AI_SEND_PACKET_T *pkt, AI_STATUS_CODE code)
{
    uint32_t attr_idx = 0;
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_CONNECT_CLOSE_ERR_CODE, ATTR_PT_U16, &code, sizeof(AI_STATUS_CODE));
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

static OPERATE_RET __create_auth_req_attrs(AI_SEND_PACKET_T *pkt)
{
    uint32_t attr_idx = 0;
    char *user_name = ai_basic_proto->config.username;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_USER_NAME, ATTR_PT_STR, user_name, strlen(user_name));
    char *password = ai_basic_proto->config.credential;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_PASSWORD, ATTR_PT_STR, password, strlen(password));
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

static OPERATE_RET __create_clt_hello_attrs(AI_SEND_PACKET_T *pkt)
{
    uint32_t attr_idx = 0;

    uint8_t client_type = ATTR_CLIENT_TYPE_DEVICE;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_CLIENT_TYPE, ATTR_PT_U8, &client_type, sizeof(uint8_t));
    char *client_id = ai_basic_proto->config.client_id;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_CLIENT_ID, ATTR_PT_STR, client_id, strlen(client_id));
    char *derived_algorithm = ai_basic_proto->config.derived_algorithm;
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_DERIVED_ALGORITHM, ATTR_PT_STR, derived_algorithm, strlen(derived_algorithm));
    char *derived_iv = ai_basic_proto->config.derived_iv;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_DERIVED_IV, ATTR_PT_STR, derived_iv, strlen(derived_iv));
    char *crypt_random = ai_basic_proto->crypt_random;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_ENCRYPT_RANDOM, ATTR_PT_STR, crypt_random, AI_RANDOM_LEN);
    char *sign_random = ai_basic_proto->sign_random;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SIGN_RANDOM, ATTR_PT_STR, sign_random, AI_RANDOM_LEN);
    uint32_t max_fragment_len = AI_MAX_FRAGMENT_LENGTH;
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_MAX_FRAGMENT_LEN, ATTR_PT_U32, &max_fragment_len, sizeof(uint32_t));
    if (AI_READ_SOCKET_BUF_SIZE > 0) {
        uint32_t read_buf_size = AI_READ_SOCKET_BUF_SIZE;
        pkt->attrs[attr_idx++] =
            tuya_ai_create_attribute(AI_ATTR_READ_BUFFER_SIZE, ATTR_PT_U32, &read_buf_size, sizeof(uint32_t));
    }
    if (AI_WRITE_SOCKET_BUF_SIZE > 0) {
        uint32_t write_buf_size = AI_WRITE_SOCKET_BUF_SIZE;
        pkt->attrs[attr_idx++] =
            tuya_ai_create_attribute(AI_ATTR_WRITE_BUFFER_SIZE, ATTR_PT_U32, &write_buf_size, sizeof(uint32_t));
    }
    char *connection_id = ai_basic_proto->connection_id;
    if (connection_id) {
        pkt->attrs[attr_idx++] =
            tuya_ai_create_attribute(AI_ATTR_CONNECTION_ID, ATTR_PT_STR, connection_id, strlen(connection_id));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

static OPERATE_RET __create_refresh_req_attrs(AI_SEND_PACKET_T *pkt)
{
    uint32_t attr_idx = 0;
    char *connection_id = ai_basic_proto->connection_id;
    if (connection_id) {
        pkt->attrs[attr_idx++] =
            tuya_ai_create_attribute(AI_ATTR_CONNECTION_ID, ATTR_PT_STR, connection_id, strlen(connection_id));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

OPERATE_RET tuya_ai_basic_refresh_req(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_CONN_REFRESH_REQ;
    rt = __create_refresh_req_attrs(&pkt);
    if (OPRT_OK != rt) {
        return rt;
    }
    AI_PROTO_D("send connect refresh req");
    return tuya_ai_basic_pkt_send(&pkt);
}

OPERATE_RET tuya_ai_pong(char *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_ATTRIBUTE_T attr = {0};
    uint64_t client_ts = 0;
    uint64_t server_ts = 0;

    AI_PAYLOAD_HEAD_T *packet = (AI_PAYLOAD_HEAD_T *)data;
    if (packet->attribute_flag != AI_HAS_ATTR) {
        PR_ERR("refresh resp packet has no attribute");
        return OPRT_COM_ERROR;
    }

    uint32_t attr_len = 0;
    memcpy(&attr_len, data + sizeof(AI_PAYLOAD_HEAD_T), sizeof(attr_len));
    attr_len = UNI_NTOHL(attr_len);
    uint32_t offset = sizeof(AI_PAYLOAD_HEAD_T) + sizeof(attr_len);
    char *attr_data = data + offset;
    offset = 0;

    while (offset < attr_len) {
        memset(&attr, 0, sizeof(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(attr_data, &offset, &attr);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }

        if (attr.type == AI_ATTR_CLIENT_TS) {
            client_ts = attr.value.u64;
        } else if (attr.type == AI_ATTR_SERVER_TS) {
            server_ts = attr.value.u64;
        } else {
            PR_ERR("unknow attr type:%d", attr.type);
        }
    }

    AI_PROTO_D("client ts:%llu, server ts:%llu", client_ts, server_ts);
    return rt;
}

OPERATE_RET tuya_ai_refresh_resp(char *de_buf, uint32_t attr_len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_ATTRIBUTE_T *attr = NULL;
    uint32_t attr_num = 0, idx = 0;

    rt = tuya_parse_user_attrs(de_buf, attr_len, &attr, &attr_num);
    if (OPRT_OK != rt) {
        PR_ERR("parse user attr failed, rt:%d", rt);
        return rt;
    }
    for (idx = 0; idx < attr_num; idx++) {
        if (attr[idx].type == AI_ATTR_CONNECT_STATUS_CODE) {
            uint16_t status = attr[idx].value.u16;
            if (status == AI_CODE_OK) {
                PR_NOTICE("connect refresh resp success");
            } else {
                PR_ERR("refresh resp failed, status:%d", status);
                return OPRT_COM_ERROR;
            }
        } else if (attr[idx].type == AI_ATTR_LAST_EXPIRE_TS) {
            ai_basic_proto->config.expire = attr[idx].value.u64;
            AI_PROTO_D("refresh expire ts:%llu", ai_basic_proto->config.expire);
        } else {
            PR_ERR("unknow attr type:%d", attr[idx].type);
        }
    }
    Free(attr);
    return rt;
}

OPERATE_RET tuya_ai_basic_client_hello(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_CLIENT_HELLO;
    rt = __create_clt_hello_attrs(&pkt);
    if (OPRT_OK != rt) {
        return rt;
    }
    AI_PROTO_D("send client hello");
    return tuya_ai_basic_pkt_send(&pkt);
}

OPERATE_RET tuya_ai_basic_auth_req(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_AUTH_REQ;
    rt = __create_auth_req_attrs(&pkt);
    if (OPRT_OK != rt) {
        return rt;
    }
    AI_PROTO_D("send auth req");
    return tuya_ai_basic_pkt_send(&pkt);
}

void tuya_ai_basic_disconnect(void)
{
    __ai_basic_proto_deinit();
}

OPERATE_RET tuya_ai_basic_conn_close(AI_STATUS_CODE code)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_CONN_CLOSE;
    rt = __create_conn_close_attrs(&pkt, code);
    if (OPRT_OK != rt) {
        return rt;
    }
    rt = tuya_ai_basic_pkt_send(&pkt);
    ai_basic_proto->connected = false;
    // tuya_ai_basic_disconnect();
    return rt;
}

static int __ai_baisc_read_pkt_head(char *recv_buf)
{
    int offset = sizeof(AI_PACKET_HEAD_T);
    int need_recv_len = offset;
    int total_recv_len = 0;
    int recv_len = 0;

    while (total_recv_len < need_recv_len) {
        recv_len = tuya_transporter_read(ai_basic_proto->transporter, (uint8_t *)recv_buf + total_recv_len,
                                         need_recv_len - total_recv_len, AI_DEFAULT_TIMEOUT_MS);
        if (recv_len <= 0) {
            if (total_recv_len == 0) {
                return recv_len;
            } else {
                if (recv_len == OPRT_RESOURCE_NOT_READY) {
                    continue;
                } else {
                    PR_ERR("recv head err, rt:%d, %d", recv_len, need_recv_len);
                    return OPRT_COM_ERROR;
                }
            }
        }
        total_recv_len += recv_len;
    }

    need_recv_len = sizeof(uint32_t);
    AI_PACKET_HEAD_T *head = (AI_PACKET_HEAD_T *)recv_buf;
    if (head->iv_flag) {
        need_recv_len += AI_IV_LEN;
    }

    total_recv_len = 0;
    while (total_recv_len < need_recv_len) {
        recv_len = tuya_transporter_read(ai_basic_proto->transporter, (uint8_t *)recv_buf + offset + total_recv_len,
                                         need_recv_len - total_recv_len, AI_DEFAULT_TIMEOUT_MS);
        if (recv_len <= 0) {
            if (recv_len == OPRT_RESOURCE_NOT_READY) {
                continue;
            }
            PR_ERR("recv length err, rt:%d, %d", recv_len, need_recv_len);
            return OPRT_COM_ERROR;
        }
        total_recv_len += recv_len;
    }
    offset += need_recv_len;

    if (head->iv_flag) {
        memcpy(ai_basic_proto->decrypt_iv, recv_buf + sizeof(AI_PACKET_HEAD_T), AI_IV_LEN);
    }
    // AI_PROTO_D("recv head len:%d", offset);
    return offset;
}

void tuya_ai_basic_pkt_free(char *data)
{
    if (data == ai_basic_proto->frag_mng.data) {
        Free(data);
        ai_basic_proto->frag_mng.data = NULL;
        memset(&ai_basic_proto->frag_mng, 0, sizeof(AI_BASIC_FRAG_MNG_T));
    } else {
        Free(data);
    }
}

OPERATE_RET tuya_ai_basic_pkt_read(char **out, uint32_t *out_len)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t calc_sign[AI_SIGN_LEN] = {0};
    uint8_t packet_sign[AI_SIGN_LEN] = {0};
    char *decrypt_buf = NULL;
    char *recv_buf = ai_basic_proto->recv_buf;
    TUYA_CHECK_NULL_RETURN(recv_buf, OPRT_COM_ERROR);

    memset(recv_buf, 0, sizeof(ai_basic_proto->recv_buf));
    AI_PROTO_D("recv packet ing");
    int recv_len = __ai_baisc_read_pkt_head(recv_buf);
    if (recv_len <= 0) {
        goto EXIT;
    }

    AI_PACKET_HEAD_T *head = (AI_PACKET_HEAD_T *)recv_buf;
    AI_PROTO_D("recv packet ver:%d", head->version);
    AI_PROTO_D("recv packet seq:%d", UNI_NTOHS(head->sequence));
    AI_PROTO_D("recv packet frag:%d", head->frag_flag);
    AI_PROTO_D("recv packet sl:%d", head->security_level);
    AI_PROTO_D("recv packet iv flag:%d", head->iv_flag);

    uint32_t head_len = __ai_get_head_len(recv_buf);
    uint32_t packet_len = __ai_get_packet_len(recv_buf);

    AI_PROTO_D("recv head len:%d", head_len);
    AI_PROTO_D("recv packet len:%d", packet_len);

    if (packet_len + head_len > sizeof(ai_basic_proto->recv_buf)) {
        PR_ERR("recv packet too long, pkt len:%u, head len:%u", packet_len, head_len);
        goto EXIT;
    }

    uint16_t sequence = UNI_NTOHS(head->sequence);
    if (sequence <= ai_basic_proto->sequence_in) {
        PR_ERR("sequence error, in:%d, pre:%d", sequence, ai_basic_proto->sequence_in);
        goto EXIT;
    }

    ai_basic_proto->sequence_in = sequence;
    if (sequence >= 0xFFFF) {
        ai_basic_proto->sequence_in = 0;
    }

    uint32_t continue_recv_len = 0;
    int offset = recv_len;
    while (packet_len + head_len > offset) {
        continue_recv_len = packet_len + head_len - offset;
        recv_len = tuya_transporter_read(ai_basic_proto->transporter, (uint8_t *)(recv_buf + offset), continue_recv_len,
                                         AI_DEFAULT_TIMEOUT_MS);
        if (recv_len <= 0) {
            if (recv_len == OPRT_RESOURCE_NOT_READY) {
                continue;
            }
            PR_ERR("continue read failed, rt:%d, %d", recv_len, continue_recv_len);
            goto EXIT;
        }
        offset += recv_len;
    }

    rt = __ai_packet_sign(recv_buf, calc_sign);
    if (OPRT_OK != rt) {
        PR_ERR("packet sign failed, rt:%d", rt);
        goto EXIT;
    }

    AI_PROTO_D("sign ok");
    uint32_t payload_len = __ai_get_payload_len(recv_buf);
    char *payload = recv_buf + head_len;
    memcpy(packet_sign, payload + payload_len, AI_SIGN_LEN);
    if (memcmp(calc_sign, packet_sign, sizeof(calc_sign))) {
        PR_ERR("packet sign error");
        // tuya_debug_hex_dump("calc_sign", AI_SIGN_LEN, calc_sign, AI_SIGN_LEN);
        // tuya_debug_hex_dump("packet_sign", AI_SIGN_LEN, packet_sign, AI_SIGN_LEN);
        goto EXIT;
    }

    uint32_t decrypt_len = 0;
    decrypt_buf = Malloc(packet_len + head_len + AI_ADD_PKT_LEN);
    TUYA_CHECK_NULL_RETURN(decrypt_buf, OPRT_MALLOC_FAILED);
    memset(decrypt_buf, 0, packet_len + head_len + AI_ADD_PKT_LEN);
    rt = __ai_decrypt_packet(payload, payload_len, decrypt_buf, &decrypt_len);
    if (OPRT_OK != rt) {
        PR_ERR("decrypt packet failed, rt:%d", rt);
        goto EXIT;
    }
    AI_PROTO_D("decrypt len:%d", decrypt_len);

    AI_FRAG_FLAG current_frag_flag = head->frag_flag;
    AI_FRAG_FLAG last_frag_flag = ai_basic_proto->frag_mng.frag_flag;
    if ((last_frag_flag == AI_PACKET_FRAG_START) || (last_frag_flag == AI_PACKET_FRAG_ING)) {
        if ((current_frag_flag != AI_PACKET_FRAG_ING) && (current_frag_flag != AI_PACKET_FRAG_END)) {
            PR_ERR("recv start frag packet, but not continue %d, %d", current_frag_flag, last_frag_flag);
            goto EXIT;
        }
    }

    AI_PROTO_D("frag flag:%d", head->frag_flag);
    AI_PROTO_D("frag mng info, flag:%d, offset:%d", ai_basic_proto->frag_mng.frag_flag,
               ai_basic_proto->frag_mng.offset);
    if (current_frag_flag == AI_PACKET_FRAG_START) {
        uint32_t origin_len = 0, frag_offset = 0, attr_len = 0, frag_total_len = 0;
        AI_PAYLOAD_HEAD_T *pkt_head = (AI_PAYLOAD_HEAD_T *)decrypt_buf;
        if (pkt_head->attribute_flag == AI_HAS_ATTR) {
            frag_offset = sizeof(AI_PAYLOAD_HEAD_T);
            memcpy(&attr_len, decrypt_buf + frag_offset, sizeof(attr_len));
            frag_offset += sizeof(attr_len);
            attr_len = UNI_NTOHL(attr_len);
            frag_offset += attr_len;
            memcpy(&origin_len, decrypt_buf + frag_offset, sizeof(origin_len));
            origin_len = UNI_NTOHL(origin_len);
            AI_PROTO_D("recv start frag packet with attr, origin len:%d", origin_len);
        } else {
            memcpy(&origin_len, decrypt_buf + sizeof(AI_PAYLOAD_HEAD_T), sizeof(origin_len));
            origin_len = UNI_NTOHL(origin_len);
            AI_PROTO_D("recv start frag packet, origin len:%d", origin_len);
        }
        if (origin_len <= decrypt_len) {
            PR_ERR("origin len error, origin len:%d, decrypt len:%d", origin_len, decrypt_len);
            goto EXIT;
        }
        memset(&ai_basic_proto->frag_mng, 0, sizeof(AI_BASIC_FRAG_MNG_T));
        ai_basic_proto->frag_mng.frag_flag = current_frag_flag;
        frag_total_len = origin_len + frag_offset + AI_ADD_PKT_LEN;
        AI_PROTO_D("frag_total_len %d", frag_total_len);
        ai_basic_proto->frag_mng.data = Malloc(frag_total_len);
        if (!ai_basic_proto->frag_mng.data) {
            PR_ERR("malloc origin data failed len:%d", decrypt_len);
            goto EXIT;
        }
        AI_PROTO_D("malloc frag_mng data addr %p", ai_basic_proto->frag_mng.data);
        memset(ai_basic_proto->frag_mng.data, 0, frag_total_len);
        memcpy(ai_basic_proto->frag_mng.data, decrypt_buf, decrypt_len);
        ai_basic_proto->frag_mng.offset = decrypt_len;
        Free(decrypt_buf);
        decrypt_buf = NULL;
        rt = tuya_ai_basic_pkt_read(out, out_len);
        if (rt != OPRT_OK) {
            PR_ERR("read continue frag packet failed, rt:%d", rt);
            goto EXIT;
        }
    } else if (current_frag_flag == AI_PACKET_FRAG_ING) {
        memcpy(ai_basic_proto->frag_mng.data + ai_basic_proto->frag_mng.offset, decrypt_buf, decrypt_len);
        ai_basic_proto->frag_mng.frag_flag = current_frag_flag;
        ai_basic_proto->frag_mng.offset += decrypt_len;
        Free(decrypt_buf);
        decrypt_buf = NULL;
        rt = tuya_ai_basic_pkt_read(out, out_len);
        if (rt != OPRT_OK) {
            PR_ERR("read continue ing frag packet failed, rt:%d", rt);
            goto EXIT;
        }
    } else if (current_frag_flag == AI_PACKET_FRAG_END) {
        memcpy(ai_basic_proto->frag_mng.data + ai_basic_proto->frag_mng.offset, decrypt_buf, decrypt_len);
        ai_basic_proto->frag_mng.frag_flag = current_frag_flag;
        ai_basic_proto->frag_mng.offset += decrypt_len;
        Free(decrypt_buf);
        decrypt_buf = NULL;
        *out = ai_basic_proto->frag_mng.data;
        *out_len = ai_basic_proto->frag_mng.offset;
    } else {
        *out = decrypt_buf;
        *out_len = decrypt_len;
    }
    AI_PROTO_D("recv packet len:%d", *out_len);
    return rt;

EXIT:
    if (decrypt_buf) {
        Free(decrypt_buf);
        decrypt_buf = NULL;
    }
    if (ai_basic_proto->frag_mng.data) {
        Free(ai_basic_proto->frag_mng.data);
    }
    memset(&ai_basic_proto->frag_mng, 0, sizeof(AI_BASIC_FRAG_MNG_T));
    return recv_len;
}

OPERATE_RET tuya_parse_user_attrs(char *in, uint32_t attr_len, AI_ATTRIBUTE_T **attr_out, uint32_t *attr_num)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    uint32_t idx = 0;

    uint32_t num = tuya_ai_get_attr_num(in, attr_len);
    AI_ATTRIBUTE_T *attr = Malloc(num * sizeof(AI_ATTRIBUTE_T));
    if (!attr) {
        PR_ERR("malloc attr failed");
        return OPRT_MALLOC_FAILED;
    }

    while (offset < attr_len) {
        memset(&attr[idx], 0, sizeof(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(in, &offset, &attr[idx]);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            return rt;
        }
        idx++;
    }

    *attr_out = attr;
    *attr_num = num;
    return rt;
}

OPERATE_RET __ai_parse_auth_resp(char *de_buf, uint32_t attr_len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_ATTRIBUTE_T *attr = NULL;
    uint32_t attr_num = 0, idx = 0, vaild_num = 0;

    rt = tuya_parse_user_attrs(de_buf, attr_len, &attr, &attr_num);
    if (OPRT_OK != rt) {
        PR_ERR("parse user attr failed, rt:%d", rt);
        return rt;
    }

    for (idx = 0; idx < attr_num; idx++) {
        if (attr[idx].type == AI_ATTR_CONNECT_STATUS_CODE) {
            uint16_t status = attr[idx].value.u16;
            if (status == AI_CODE_OK) {
                PR_NOTICE("auth success");
                vaild_num++;
            } else {
                PR_ERR("auth failed, status:%d", status);
                rt = OPRT_AUTHENTICATION_FAIL;
                goto EXIT;
            }
        } else if (attr[idx].type == AI_ATTR_CONNECTION_ID) {
            if (ai_basic_proto->connection_id) {
                Free(ai_basic_proto->connection_id);
                ai_basic_proto->connection_id = NULL;
            }
            ai_basic_proto->connection_id = Malloc(attr[idx].length + 1);
            memset(ai_basic_proto->connection_id, 0, attr[idx].length + 1);
            if (ai_basic_proto->connection_id) {
                memcpy(ai_basic_proto->connection_id, attr[idx].value.str, attr[idx].length);
                PR_NOTICE("connection id:%s", ai_basic_proto->connection_id);
                vaild_num++;
            }
        } else {
            PR_ERR("unknow attr type:%d", attr[idx].type);
        }
    }

    if (vaild_num != 2) {
        PR_ERR("auth resp attr num error:%d", vaild_num);
        rt = OPRT_COM_ERROR;
    }

EXIT:
    Free(attr);
    return rt;
}

OPERATE_RET tuya_ai_parse_conn_close(char *de_buf, uint32_t attr_len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_ATTRIBUTE_T *attr = NULL;
    uint32_t attr_num = 0, idx = 0;

    rt = tuya_parse_user_attrs(de_buf, attr_len, &attr, &attr_num);
    if (OPRT_OK != rt) {
        PR_ERR("parse user attr failed, rt:%d", rt);
        return rt;
    }

    for (idx = 0; idx < attr_num; idx++) {
        if (attr[idx].type == AI_ATTR_CONNECT_CLOSE_ERR_CODE) {
            uint16_t status = attr[idx].value.u16;
            PR_ERR("recv connect close when auth:%d", status);
            rt = OPRT_AUTHENTICATION_FAIL;
        } else {
            PR_ERR("unknow attr type:%d", attr[idx].type);
        }
    }
    Free(attr);
    return rt;
}

OPERATE_RET tuya_ai_auth_resp(void)
{
    OPERATE_RET rt = OPRT_OK;
    char *de_buf = NULL;
    uint32_t de_len = 0;

    rt = tuya_ai_basic_pkt_read(&de_buf, &de_len);
    if (OPRT_OK != rt) {
        PR_ERR("recv auth resp failed, rt:%d", rt);
        return rt;
    }

    AI_PAYLOAD_HEAD_T *packet = (AI_PAYLOAD_HEAD_T *)de_buf;
    if (packet->attribute_flag != AI_HAS_ATTR) {
        PR_ERR("auth resp packet has no attribute");
        Free(de_buf);
        return OPRT_COM_ERROR;
    }

    uint32_t attr_len = 0;
    memcpy(&attr_len, de_buf + sizeof(AI_PAYLOAD_HEAD_T), sizeof(attr_len));
    attr_len = UNI_NTOHL(attr_len);
    uint32_t offset = sizeof(AI_PAYLOAD_HEAD_T) + sizeof(attr_len);
    if (packet->type == AI_PT_AUTH_RESP) {
        rt = __ai_parse_auth_resp(de_buf + offset, attr_len);
    } else if (packet->type == AI_PT_CONN_CLOSE) {
        rt = tuya_ai_parse_conn_close(de_buf + offset, attr_len);
    } else {
        PR_ERR("auth resp packet type error %d", packet->type);
        rt = OPRT_COM_ERROR;
    }
    Free(de_buf);
    return rt;
}

static OPERATE_RET __create_ping_attrs(AI_SEND_PACKET_T *pkt)
{
    uint32_t attr_idx = 0;
    uint64_t ts = tal_time_get_posix_ms();
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_CLIENT_TS, ATTR_PT_U64, &ts, sizeof(uint64_t));
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

OPERATE_RET tuya_ai_basic_ping(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_PING;
    rt = __create_ping_attrs(&pkt);
    if (OPRT_OK != rt) {
        return rt;
    }
    PR_NOTICE("send ai ping");
    rt = tuya_ai_basic_pkt_send(&pkt);
    return rt;
}

OPERATE_RET tuya_ai_basic_connect(void)
{
    OPERATE_RET rt = OPRT_OK;
    ai_basic_proto->transporter = tuya_transporter_create(TRANSPORT_TYPE_TCP, NULL);
    if (!ai_basic_proto->transporter) {
        PR_ERR("create transporter err");
        return OPRT_COM_ERROR;
    }
    uint32_t idx = 0;
    for (idx = 0; idx < ai_basic_proto->config.host_num; idx++) {
        PR_NOTICE("connect to host :%s, port: %d", ai_basic_proto->config.hosts[idx], ai_basic_proto->config.tcp_port);
        rt = tuya_transporter_connect(ai_basic_proto->transporter, ai_basic_proto->config.hosts[idx],
                                      ai_basic_proto->config.tcp_port, AI_DEFAULT_TIMEOUT_MS);
        if (OPRT_OK == rt) {
            ai_basic_proto->connected = true;
            break;
        }
    }
    return rt;
}

AI_PACKET_PT tuya_ai_basic_get_pkt_type(char *buf)
{
    AI_PAYLOAD_HEAD_T *payload = (AI_PAYLOAD_HEAD_T *)buf;
    return payload->type;
}

OPERATE_RET tuya_pack_user_attrs(AI_ATTRIBUTE_T *attr, uint32_t attr_num, uint8_t **out, uint32_t *out_len)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0, idx = 0, attr_len = 0;
    for (idx = 0; idx < attr_num; idx++) {
        attr_len += OFFSOF(AI_ATTRIBUTE_T, value) + attr[idx].length;
    }

    char *usr_buf = Malloc(attr_len);
    TUYA_CHECK_NULL_RETURN(usr_buf, OPRT_MALLOC_FAILED);
    memset(usr_buf, 0, attr_len);

    for (idx = 0; idx < attr_num; idx++) {
        AI_ATTR_TYPE type = UNI_HTONS(attr[idx].type);
        memcpy(usr_buf + offset, &type, sizeof(AI_ATTR_TYPE));
        offset += sizeof(AI_ATTR_TYPE);
        AI_ATTR_PT payload_type = attr[idx].payload_type;
        memcpy(usr_buf + offset, &payload_type, sizeof(AI_ATTR_PT));
        offset += sizeof(AI_ATTR_PT);
        uint32_t attr_idx_len = attr[idx].length;
        uint32_t length = UNI_HTONL(attr_idx_len);
        memcpy(usr_buf + offset, &length, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        if (payload_type == ATTR_PT_U8) {
            memcpy(usr_buf + offset, &attr[idx].value.u8, attr_idx_len);
        } else if (payload_type == ATTR_PT_U16) {
            uint16_t value_u16 = UNI_HTONS(attr[idx].value.u16);
            memcpy(usr_buf + offset, &value_u16, attr_idx_len);
        } else if (payload_type == ATTR_PT_U32) {
            uint32_t value_u32 = UNI_HTONL(attr[idx].value.u32);
            memcpy(usr_buf + offset, &value_u32, attr_idx_len);
        } else if (payload_type == ATTR_PT_U64) {
            uint64_t value_u64 = attr[idx].value.u64;
            UNI_HTONLL(value_u64);
            memcpy(usr_buf + offset, &value_u64, attr_idx_len);
        } else if (payload_type == ATTR_PT_BYTES) {
            memcpy(usr_buf + offset, attr[idx].value.bytes, attr_idx_len);
        } else if (payload_type == ATTR_PT_STR) {
            memcpy(usr_buf + offset, attr[idx].value.str, attr_idx_len);
        } else {
            PR_ERR("invalid payload type %d", payload_type);
            Free(usr_buf);
            return OPRT_COM_ERROR;
        }
        offset += attr_idx_len;
    }

    *out_len = attr_len;
    *out = (uint8_t *)usr_buf;

    return rt;
}

static OPERATE_RET __create_seesion_new_attrs(AI_SEND_PACKET_T *pkt, AI_SESSION_NEW_ATTR_T *session)
{
    uint32_t attr_idx = 0;
    uint32_t bizcode = 0;
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_SESSION_ID, ATTR_PT_STR, session->id, strlen(session->id));
    if (session->biz_code != 0) {
        bizcode = session->biz_code;
    } else {
        bizcode = ai_basic_proto->config.biz_code;
    }
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_BIZ_CODE, ATTR_PT_U32, &bizcode, sizeof(uint32_t));
    uint64_t biz_tag = AI_DEFAULT_BIZ_TAG;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_BIZ_TAG, ATTR_PT_U64, &biz_tag, sizeof(uint64_t));
    if (session->user_data) {
        pkt->attrs[attr_idx++] =
            tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_BYTES, session->user_data, session->user_len);
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

static OPERATE_RET __create_seesion_close_attrs(AI_SEND_PACKET_T *pkt, char *session_id, AI_STATUS_CODE code)
{
    uint32_t attr_idx = 0;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SESSION_ID, ATTR_PT_STR, session_id, strlen(session_id));
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_SESSION_CLOSE_ERR_CODE, ATTR_PT_U16, &code, sizeof(uint16_t));
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

static OPERATE_RET __create_video_attrs(AI_SEND_PACKET_T *pkt, AI_VIDEO_ATTR_T *video)
{
    uint32_t attr_idx = 0;
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_VIDEO_CODEC_TYPE, ATTR_PT_U16, &video->base.codec_type, sizeof(uint16_t));
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_VIDEO_SAMPLE_RATE, ATTR_PT_U32, &video->base.sample_rate, sizeof(uint32_t));
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_VIDEO_WIDTH, ATTR_PT_U16, &video->base.width, sizeof(uint16_t));
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_VIDEO_HEIGHT, ATTR_PT_U16, &video->base.height, sizeof(uint16_t));
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_VIDEO_FPS, ATTR_PT_U16, &video->base.fps, sizeof(uint16_t));
    if (video->option.user_data) {
        pkt->attrs[attr_idx++] =
            tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_BYTES, video->option.user_data, video->option.user_len);
    }
    if (video->option.session_id_list) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(
            AI_ATTR_SESSION_ID_LIST, ATTR_PT_STR, video->option.session_id_list, strlen(video->option.session_id_list));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

static OPERATE_RET __create_audio_attrs(AI_SEND_PACKET_T *pkt, AI_AUDIO_ATTR_T *audio)
{
    uint32_t attr_idx = 0;
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_AUDIO_CODEC_TYPE, ATTR_PT_U16, &audio->base.codec_type, sizeof(uint16_t));
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_AUDIO_SAMPLE_RATE, ATTR_PT_U32, &audio->base.sample_rate, sizeof(uint32_t));
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_AUDIO_CHANNELS, ATTR_PT_U16, &audio->base.channels, sizeof(uint16_t));
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_AUDIO_DEPTH, ATTR_PT_U16, &audio->base.bit_depth, sizeof(uint16_t));
    if (audio->option.user_data) {
        pkt->attrs[attr_idx++] =
            tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_BYTES, audio->option.user_data, audio->option.user_len);
    }
    if (audio->option.session_id_list) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(
            AI_ATTR_SESSION_ID_LIST, ATTR_PT_STR, audio->option.session_id_list, strlen(audio->option.session_id_list));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

static OPERATE_RET __create_image_attrs(AI_SEND_PACKET_T *pkt, AI_IMAGE_ATTR_T *image)
{
    uint32_t attr_idx = 0;
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_IMAGE_FORMAT, ATTR_PT_U8, &image->base.format, sizeof(uint8_t));
    if (image->base.width > 0) {
        pkt->attrs[attr_idx++] =
            tuya_ai_create_attribute(AI_ATTR_IMAGE_WIDTH, ATTR_PT_U16, &image->base.width, sizeof(uint16_t));
    }
    if (image->base.height > 0) {
        pkt->attrs[attr_idx++] =
            tuya_ai_create_attribute(AI_ATTR_IMAGE_HEIGHT, ATTR_PT_U16, &image->base.height, sizeof(uint16_t));
    }
    if (image->option.user_data) {
        pkt->attrs[attr_idx++] =
            tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_BYTES, image->option.user_data, image->option.user_len);
    }
    if (image->option.session_id_list) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(
            AI_ATTR_SESSION_ID_LIST, ATTR_PT_STR, image->option.session_id_list, strlen(image->option.session_id_list));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

static OPERATE_RET __create_file_attrs(AI_SEND_PACKET_T *pkt, AI_FILE_ATTR_T *file)
{
    uint32_t attr_idx = 0;
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_FILE_FORMAT, ATTR_PT_U8, &file->base.format, sizeof(uint8_t));
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_FILE_NAME, ATTR_PT_STR, &file->base.file_name, strlen(file->base.file_name));
    if (file->option.user_data) {
        pkt->attrs[attr_idx++] =
            tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_BYTES, file->option.user_data, file->option.user_len);
    }
    if (file->option.session_id_list) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(
            AI_ATTR_SESSION_ID_LIST, ATTR_PT_STR, file->option.session_id_list, strlen(file->option.session_id_list));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

static OPERATE_RET __create_text_attrs(AI_SEND_PACKET_T *pkt, AI_TEXT_ATTR_T *text)
{
    uint32_t attr_idx = 0;
    if (text->session_id_list) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SESSION_ID_LIST, ATTR_PT_STR, text->session_id_list,
                                                          strlen(text->session_id_list));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

static OPERATE_RET __create_event_attrs(AI_SEND_PACKET_T *pkt, AI_EVENT_ATTR_T *event, AI_EVENT_TYPE type)
{
    uint32_t attr_idx = 0;
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_SESSION_ID, ATTR_PT_STR, event->session_id, strlen(event->session_id));
    pkt->attrs[attr_idx++] =
        tuya_ai_create_attribute(AI_ATTR_EVENT_ID, ATTR_PT_STR, event->event_id, strlen(event->event_id));
    if (type == AI_EVENT_END) {
        uint64_t ts = tal_time_get_posix_ms();
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_EVENT_TS, ATTR_PT_U64, &ts, sizeof(uint64_t));
    }
    if (event->user_data) {
        pkt->attrs[attr_idx++] =
            tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_BYTES, event->user_data, event->user_len);
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

OPERATE_RET tuya_ai_basic_session_new(AI_SESSION_NEW_ATTR_T *session, char *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_SESSION_NEW;
    rt = __create_seesion_new_attrs(&pkt, session);
    if (OPRT_OK != rt) {
        return rt;
    }
    pkt.data = data;
    pkt.len = len;
    AI_PROTO_D("send session new");
    return tuya_ai_basic_pkt_send(&pkt);
}

OPERATE_RET tuya_ai_basic_session_close(char *session_id, AI_STATUS_CODE code)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_SESSION_CLOSE;
    rt = __create_seesion_close_attrs(&pkt, session_id, code);
    if (OPRT_OK != rt) {
        return rt;
    }
    AI_PROTO_D("send session close");
    return tuya_ai_basic_pkt_send(&pkt);
}

OPERATE_RET tuya_ai_basic_video(AI_VIDEO_ATTR_T *video, char *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_VIDEO;
    if (video) {
        rt = __create_video_attrs(&pkt, video);
        if (OPRT_OK != rt) {
            return rt;
        }
    }
    pkt.len = len;
    pkt.data = data;
    AI_PROTO_D("send video");
    return tuya_ai_basic_pkt_send(&pkt);
}

OPERATE_RET tuya_ai_basic_audio(AI_AUDIO_ATTR_T *audio, char *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_AUDIO;
    if (audio) {
        rt = __create_audio_attrs(&pkt, audio);
        if (OPRT_OK != rt) {
            return rt;
        }
    }
    pkt.len = len;
    pkt.data = data;
    AI_PROTO_D("send audio");
    return tuya_ai_basic_pkt_send(&pkt);
}

OPERATE_RET tuya_ai_basic_image(AI_IMAGE_ATTR_T *image, char *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_IMAGE;
    if (image) {
        rt = __create_image_attrs(&pkt, image);
        if (OPRT_OK != rt) {
            return rt;
        }
    }
    pkt.len = len;
    pkt.data = data;
    AI_PROTO_D("send image");
    return tuya_ai_basic_pkt_send(&pkt);
}

OPERATE_RET tuya_ai_basic_file(AI_FILE_ATTR_T *file, char *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_FILE;
    if (file) {
        rt = __create_file_attrs(&pkt, file);
        if (OPRT_OK != rt) {
            return rt;
        }
    }
    pkt.len = len;
    pkt.data = data;
    AI_PROTO_D("send file");
    return tuya_ai_basic_pkt_send(&pkt);
}

OPERATE_RET tuya_ai_basic_text(AI_TEXT_ATTR_T *text, char *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_TEXT;
    if (text) {
        rt = __create_text_attrs(&pkt, text);
        if (OPRT_OK != rt) {
            return rt;
        }
    }
    pkt.len = len;
    pkt.data = data;
    AI_PROTO_D("send text");
    return tuya_ai_basic_pkt_send(&pkt);
}

OPERATE_RET tuya_ai_basic_event(AI_EVENT_ATTR_T *event, char *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_EVENT;

    AI_EVENT_HEAD_T head = {0};
    memcpy(&head, data, sizeof(AI_EVENT_HEAD_T));
    head.type = UNI_NTOHS(head.type);

    if (event) {
        rt = __create_event_attrs(&pkt, event, head.type);
        if (OPRT_OK != rt) {
            return rt;
        }
    }
    pkt.len = len;
    pkt.data = data;
    AI_PROTO_D("send event");
    return tuya_ai_basic_pkt_send(&pkt);
}

// such as f47ac10b-58cc-42d5-0136-4067a8e7d6b3
OPERATE_RET tuya_ai_basic_uuid_v4(char *uuid_str)
{
    if (NULL == uuid_str) {
        PR_ERR("uuid_str is NULL");
        return OPRT_INVALID_PARM;
    }

    memset(uuid_str, 0, AI_UUID_V4_LEN);
    uint8_t uuid[16] = {0};
    uni_random_bytes(uuid, 16);
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    uuid[8] = (uuid[8] & 0x3F) | 0x80;
    snprintf(uuid_str, AI_UUID_V4_LEN, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x", uuid[0],
             uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7], uuid[8], uuid[9], uuid[10], uuid[11],
             uuid[12], uuid[13], uuid[14], uuid[15]);

    return OPRT_OK;
}