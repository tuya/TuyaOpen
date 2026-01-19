/**
 * @file tuya_ai_protocol.c
 * @author tuya
 * @brief ai protocol
 * @version 0.1
 * @date 2025-03-02
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
#include "tuya_transporter.h"
#include "tuya_iot_config.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/chacha20.h"
#include "gw_intf.h"
#include "uni_log.h"
#include "uni_random.h"
#include "tal_system.h"
#include "tal_hash.h"
#include "cipher_wrapper.h"
#include "tal_security.h"
#include "tal_memory.h"
#include "mqc_app.h"
#include "tuya_svc_netmgr_linkage.h"
#include "tuya_ai_protocol.h"
#include "tuya_ai_private.h"
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#define AI_SEND_SOCKET_TIMEOUT (10 * 1000)
#define AI_ADD_PKT_LEN 128
#define AI_RSA_PKT_LEN 256

#ifndef AI_READ_SOCKET_BUF_SIZE
#define AI_READ_SOCKET_BUF_SIZE 0
#endif
#ifndef AI_WRITE_SOCKET_BUF_SIZE
#define AI_WRITE_SOCKET_BUF_SIZE 0
#endif
#ifndef AI_SEND_PKT_TIMEOUT
#define AI_SEND_PKT_TIMEOUT 6
#endif

/**
*
* packet: AI_PACKET_HEAD_T+(iv)+len+payload+sign
* len:payload+sign
* payload: AI_PAYLOAD_HEAD_T+(attr_len+AI_ATTRIBUTE_T)+data
*
https://registry.code.tuya-inc.top/TuyaBEMiddleWare/steam/-/issues/1
https://wiki.tuya-inc.com:7799/page/1935941758466654283
https://registry.code.tuya-inc.top/TuyaBEMiddleWare/steam/-/issues/15
https://wiki.tuya-inc.com:7799/page/1935941995700686914
**/

typedef struct {
    AI_FRAG_FLAG frag_flag;
    uint32_t offset;
    char *data;
} AI_RECV_FRAG_MNG_T;
typedef struct {
    uint32_t offset;
} AI_SEND_FRAG_MNG_T;
typedef struct {
    MUTEX_HANDLE mutex;
    tuya_transporter_t transporter;
    char crypt_key[AI_KEY_LEN + 1];
    char sign_key[AI_KEY_LEN + 1];
    uint16_t sequence_in;
    uint16_t sequence_out;
    char crypt_random[AI_RANDOM_LEN + 1];
    char sign_random[AI_RANDOM_LEN + 1];
    AI_PACKET_SL sl;
    BOOL_T connected;
    char *connection_id;
    char encrypt_iv[AI_IV_LEN + 1];
    char decrypt_iv[AI_IV_LEN + 1];
    char iv_mask[AI_IV_LEN + 1];
    AI_RECV_FRAG_MNG_T recv_frag_mng;
    AI_SEND_FRAG_MNG_T send_frag_mng[5];
    BOOL_T frag_flag;
    char recv_buf[AI_MAX_FRAGMENT_LENGTH + AI_ADD_PKT_LEN];
    char *rsa_public_key;
    uint32_t file_seq;
    uint32_t text_seq;
} AI_BASIC_PROTO_T;
STATIC AI_BASIC_PROTO_T *ai_basic_proto = NULL;

STATIC OPERATE_RET __default_write(AI_PACKET_WRITER_T *writer, VOID *buf, uint32_t buf_len);

STATIC AI_PACKET_WRITER_T s_default_packet_writer = {
    .write = __default_write,
    .user_data = NULL
};

#if defined(AI_VERSION) && (0x02 == AI_VERSION)
STATIC AI_ATTR_TYPE_INFO s_attr_type_array[] = {
    {AI_ATTR_SECURITSUIT_TYPE,          ATTR_PT_BYTES},
    {AI_ATTR_CLIENT_TYPE,               ATTR_PT_U8},
    {AI_ATTR_CLIENT_ID,                 ATTR_PT_STR},
    {AI_ATTR_ENCRYPT_RANDOM,            ATTR_PT_STR},
    {AI_ATTR_SIGN_RANDOM,               ATTR_PT_STR},
    {AI_ATTR_MAX_FRAGMENT_LEN,          ATTR_PT_U32},
    {AI_ATTR_READ_BUFFER_SIZE,          ATTR_PT_U32},
    {AI_ATTR_WRITE_BUFFER_SIZE,         ATTR_PT_U32},
    {AI_ATTR_DERIVED_ALGORITHM,         ATTR_PT_STR},
    {AI_ATTR_DERIVED_IV,                ATTR_PT_STR},
    {AI_ATTR_PING_INTERVAL,             ATTR_PT_U32},
    {AI_ATTR_USER_NAME,                 ATTR_PT_STR},
    {AI_ATTR_PASSWORD,                  ATTR_PT_STR},
    {AI_ATTR_CONNECTION_ID,             ATTR_PT_STR},
    {AI_ATTR_CONNECT_STATUS_CODE,       ATTR_PT_U16},
    {AI_ATTR_LAST_EXPIRE_TS,            ATTR_PT_U64},
    {AI_ATTR_CONNECT_CLOSE_ERR_CODE,    ATTR_PT_U16},
    {AI_ATTR_BIZ_CODE,                  ATTR_PT_U32},
    {AI_ATTR_BIZ_TAG,                   ATTR_PT_U64},
    {AI_ATTR_SESSION_ID,                ATTR_PT_STR},
    {AI_ATTR_SESSION_STATUS_CODE,       ATTR_PT_U16},
    {AI_ATTR_AGENT_TOKEN,               ATTR_PT_STR},
    {AI_ATTR_SESSION_CLOSE_ERR_CODE,    ATTR_PT_U16},
    {AI_ATTR_SESSION_STATE_CHANGE_CODE, ATTR_PT_U16},
    {AI_ATTR_EVENT_ID,                  ATTR_PT_STR},
    {AI_ATTR_EVENT_TS,                  ATTR_PT_U64},
    {AI_ATTR_STREAM_START_TS,           ATTR_PT_U64},
    {AI_ATTR_DATA_IDS,                  ATTR_PT_U16},
    {AI_ATTR_CMD_DATA,                  ATTR_PT_STR},
    {AI_ATTR_PRIORITY,                  ATTR_PT_U8},
    {AI_ATTR_ASSIGN_DATAS,              ATTR_PT_STR},
    {AI_ATTR_UNASSIGN_DATAS,            ATTR_PT_STR},
    {AI_ATTR_VIDEO_PARAMS,              ATTR_PT_STR},
    {AI_ATTR_VIDEO_CODEC_TYPE,          ATTR_PT_U16},
    {AI_ATTR_VIDEO_SAMPLE_RATE,         ATTR_PT_U32},
    {AI_ATTR_VIDEO_WIDTH,               ATTR_PT_U16},
    {AI_ATTR_VIDEO_HEIGHT,              ATTR_PT_U16},
    {AI_ATTR_VIDEO_FPS,                 ATTR_PT_U16},
    {AI_ATTR_AUDIO_PARAMS,              ATTR_PT_STR},
    {AI_ATTR_AUDIO_CODEC_TYPE,          ATTR_PT_U16},
    {AI_ATTR_AUDIO_SAMPLE_RATE,         ATTR_PT_U32},
    {AI_ATTR_AUDIO_CHANNELS,            ATTR_PT_U16},
    {AI_ATTR_AUDIO_DEPTH,               ATTR_PT_U16},
    {AI_ATTR_IMAGE_PARAMS,              ATTR_PT_STR},
    {AI_ATTR_IMAGE_FORMAT,              ATTR_PT_U8},
    {AI_ATTR_IMAGE_WIDTH,               ATTR_PT_U16},
    {AI_ATTR_IMAGE_HEIGHT,              ATTR_PT_U16},
    {AI_ATTR_FILE_PARAMS,               ATTR_PT_STR},
    {AI_ATTR_FILE_FORMAT,               ATTR_PT_U8},
    {AI_ATTR_FILE_NAME,                 ATTR_PT_STR},
    {AI_ATTR_DATA_ID,                   ATTR_PT_U16},
    {AI_ATTR_USER_DATA,                 ATTR_PT_BYTES},
    {AI_ATTR_SESSION_ID_LIST,           ATTR_PT_STR},
    {AI_ATTR_CLIENT_TS,                 ATTR_PT_U64},
    {AI_ATTR_SERVER_TS,                 ATTR_PT_U64}
};
#endif

STATIC VOID xor_ivmask_with_sequence(uint8_t ivmask[16], short sequence)
{
    uint8_t short_bytes[16] = {0};
    sequence = UNI_HTONS(sequence);
    memcpy(short_bytes + 14, &sequence, sizeof(short));
    for (int i = 0; i < 16; i++) {
        ivmask[i] ^= short_bytes[i];
    }
}
#if defined(AI_VERSION) && (0x02 == AI_VERSION)
STATIC AI_ATTR_PT __ai_get_attr_type(AI_ATTR_TYPE type)
{
    for (int i = 0; i < CNTSOF(s_attr_type_array); i++) {
        if (s_attr_type_array[i].type == type) {
            AI_PROTO_D("type %d pt %d", s_attr_type_array[i].type, s_attr_type_array[i].pt);
            return s_attr_type_array[i].pt;
        }

    }
    PR_ERR("type %d not found ", type);
    return OPRT_COM_ERROR;
}
#endif

STATIC OPERATE_RET __ai_generate_crypt_key()
{
    OPERATE_RET rt = OPRT_OK;

    uni_random_string(ai_basic_proto->crypt_random, AI_RANDOM_LEN);

    char* slat = ai_basic_proto->crypt_random;
    size_t salt_len = AI_RANDOM_LEN;

    char* ikm = get_gw_cntl()->gw_actv.local_key;
    size_t ikm_len = strlen(ikm);

    char *info = NULL;
    size_t info_len = 0;

    rt = mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                      (const unsigned char *)slat, salt_len,
                      (const unsigned char *)ikm, ikm_len,
                      (const unsigned char *)info, info_len,
                      (unsigned char *)ai_basic_proto->crypt_key, AI_KEY_LEN);
    memcpy(ai_basic_proto->iv_mask, ai_basic_proto->crypt_key, AI_IV_LEN);
    // tuya_debug_hex_dump("iv_mask ", 64, (uint8_t *)ai_basic_proto->crypt_key, AI_IV_LEN);

    return rt;
}

STATIC char *__ai_get_crypt_key(VOID)
{
    return ai_basic_proto->crypt_key;
}

STATIC OPERATE_RET __ai_generate_sign_key()
{
    OPERATE_RET rt = OPRT_OK;

    uni_random_string(ai_basic_proto->sign_random, AI_RANDOM_LEN);

    char* slat = ai_basic_proto->sign_random;
    size_t salt_len = AI_RANDOM_LEN;

    char* ikm = get_gw_cntl()->gw_actv.local_key;
    size_t ikm_len = strlen(ikm);

    char *info = NULL;
    size_t info_len = 0;

    rt = mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                      (const unsigned char *)slat, salt_len,
                      (const unsigned char *)ikm, ikm_len,
                      (const unsigned char *)info, info_len,
                      (unsigned char *)ai_basic_proto->sign_key, AI_KEY_LEN);
    return rt;
}

STATIC char *__ai_get_sign_key(VOID)
{
    return ai_basic_proto->sign_key;
}

STATIC AI_PACKET_SL __ai_get_sl(AI_SEND_PACKET_T *info, BOOL_T is_decrypt)
{
    if (info && info->writer) {
        // FIXME: fixed to AI_PACKET_SL0 for now, need to support other security level
        return AI_PACKET_SL0;
    }

    if (is_decrypt) {
        return ai_basic_proto->sl;
    } else {
        if (info->type == AI_PT_CLIENT_HELLO) {
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
            return AI_PACKET_SL0;
#else
            return AI_PACKET_RSA;
#endif
        }
        return ai_basic_proto->sl;
    }
}

STATIC VOID __ai_basic_proto_deinit(VOID)
{
    if (ai_basic_proto) {
        if (ai_basic_proto->transporter) {
            tuya_transporter_close(ai_basic_proto->transporter);
            tuya_transporter_destroy(ai_basic_proto->transporter);
            ai_basic_proto->transporter = NULL;
        }
        if (ai_basic_proto->mutex) {
            tal_mutex_release(ai_basic_proto->mutex);
            ai_basic_proto->mutex = NULL;
        }
        if (ai_basic_proto->connection_id) {
            OS_FREE(ai_basic_proto->connection_id);
            ai_basic_proto->connection_id = NULL;
        }
        OS_FREE(ai_basic_proto);
        ai_basic_proto = NULL;
        PR_NOTICE("ai proto deinit success");
    }
    return;
}

STATIC VOID __ai_basic_proto_reinit(VOID)
{
    tal_mutex_lock(ai_basic_proto->mutex);
    if (ai_basic_proto->transporter) {
        tuya_transporter_close(ai_basic_proto->transporter);
        tuya_transporter_destroy(ai_basic_proto->transporter);
        ai_basic_proto->transporter = NULL;
    }
    if (ai_basic_proto->connection_id) {
        OS_FREE(ai_basic_proto->connection_id);
        ai_basic_proto->connection_id = NULL;
    }
    __ai_generate_crypt_key();
    __ai_generate_sign_key();
    ai_basic_proto->connected = FALSE;
    ai_basic_proto->sequence_in = 0;
    ai_basic_proto->sequence_out = 1;
    ai_basic_proto->file_seq = 1;
    ai_basic_proto->text_seq = 1;
    memset(ai_basic_proto->recv_buf, 0, SIZEOF(ai_basic_proto->recv_buf));
    memset(ai_basic_proto->encrypt_iv, 0, AI_IV_LEN);
    uni_random_string(ai_basic_proto->encrypt_iv, AI_IV_LEN);
    ai_basic_proto->sl = AI_PACKET_SECURITY_LEVEL;
    memset(ai_basic_proto->decrypt_iv, 0, AI_IV_LEN);
    memset(&ai_basic_proto->recv_frag_mng, 0, SIZEOF(ai_basic_proto->recv_frag_mng));
    memset(&ai_basic_proto->send_frag_mng, 0, SIZEOF(ai_basic_proto->send_frag_mng));
    tal_mutex_unlock(ai_basic_proto->mutex);
    PR_NOTICE("ai proto reinit success");
    return;
}

STATIC OPERATE_RET __ai_basic_proto_init(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    if (ai_basic_proto) {
        __ai_basic_proto_reinit();
    } else {
        ai_basic_proto = OS_MALLOC(SIZEOF(AI_BASIC_PROTO_T));
        TUYA_CHECK_NULL_RETURN(ai_basic_proto, OPRT_MALLOC_FAILED);
        memset(ai_basic_proto, 0, SIZEOF(AI_BASIC_PROTO_T));
        TUYA_CALL_ERR_GOTO(__ai_generate_crypt_key(), EXIT);
        TUYA_CALL_ERR_GOTO(__ai_generate_sign_key(), EXIT);
        TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&(ai_basic_proto->mutex)), EXIT);
        ai_basic_proto->sequence_out = 1;
        ai_basic_proto->file_seq = 1;
        ai_basic_proto->text_seq = 1;
        uni_random_string(ai_basic_proto->encrypt_iv, AI_IV_LEN);
        ai_basic_proto->sl = AI_PACKET_SECURITY_LEVEL;
        PR_NOTICE("ai proto init success, sl:%d", ai_basic_proto->sl);
    }
    return rt;

EXIT:
    PR_ERR("ai proto init failed, rt:%d", rt);
    __ai_basic_proto_deinit();
    return OPRT_COM_ERROR;
}

OPERATE_RET tuya_ai_basic_setup(VOID)
{
    return __ai_basic_proto_init();
}

STATIC uint32_t __ai_get_head_len(char *buf)
{
    uint32_t head_len = 0;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    AI_PACKET_HEAD_T *head = (AI_PACKET_HEAD_T *)buf;
    head_len = SIZEOF(AI_PACKET_HEAD_T);
    if (head->iv_flag) {
        head_len += AI_IV_LEN;
    }
    head_len += SIZEOF(head_len);
#else
    head_len = SIZEOF(AI_PACKET_HEAD_T_V2);
    head_len += SIZEOF(short);
#endif
    return head_len;
}

STATIC uint32_t __ai_get_packet_len(char *buf)
{
    uint32_t packet_len = 0;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    uint32_t head_len = SIZEOF(AI_PACKET_HEAD_T);
    AI_PACKET_HEAD_T *head = (AI_PACKET_HEAD_T *)buf;
    if (head->iv_flag) {
        memcpy(&packet_len, buf + head_len + AI_IV_LEN, SIZEOF(packet_len));
    } else {
        memcpy(&packet_len, buf + head_len, SIZEOF(packet_len));
    }
    packet_len = UNI_NTOHL(packet_len);
#else
    uint32_t head_len = SIZEOF(AI_PACKET_HEAD_T_V2);
    memcpy(&packet_len, buf + head_len, SIZEOF(short));

    packet_len = UNI_NTOHS(packet_len);
#endif

    return packet_len;
}

STATIC uint32_t __ai_get_payload_len(char *buf)
{
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    return __ai_get_packet_len(buf) - AI_SIGN_LEN;
#else
    return __ai_get_packet_len(buf);
#endif
}

STATIC OPERATE_RET __ai_packet_sign(char *buf, uint8_t *signature)
{
    OPERATE_RET rt = OPRT_OK;
    char *sign_key = __ai_get_sign_key();
    TUYA_CHECK_NULL_RETURN(sign_key, OPRT_COM_ERROR);

    uint32_t head_len = __ai_get_head_len(buf);
    uint32_t payload_len = __ai_get_payload_len(buf);

    //transport first 32 byte and packet last 32 byte, if less than 64 byte,use all packet
    uint8_t sign_data[64] = {0};
    uint32_t sign_len = 0;

    AI_PROTO_D("start sign head_len:%d, payload_len:%d", head_len, payload_len);
    if (head_len + payload_len <= SIZEOF(sign_data)) {
        memcpy(sign_data, buf, head_len + payload_len);
        sign_len = head_len + payload_len;
    } else {
        memcpy(sign_data, buf, 32);
        char *payload = buf + head_len;
        uint32_t offset = (payload_len > 32) ? payload_len - 32 : 0;
        uint32_t copy_len = (payload_len > 32) ? 32 : payload_len;
        memcpy(sign_data + 32, payload + offset, copy_len);
        sign_len = SIZEOF(sign_data);
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
    if (info->count != 0) {
        len += SIZEOF(len);
        for (idx = 0; idx < info->count; idx++) {
            if (info->attrs[idx] == NULL) {
                continue;
            }

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
            len += OFFSOF(AI_ATTRIBUTE_T, value) + info->attrs[idx]->length;
#else
            len += OFFSOF(AI_ATTRIBUTE_T_V2, value) + info->attrs[idx]->length;
#endif
        }
    }
    // AI_PROTO_D("attr len:%d", len);
    return len;
}

BOOL_T tuya_ai_is_need_attr(AI_FRAG_FLAG frag_flag)
{
    if ((frag_flag == AI_PACKET_NO_FRAG) || (frag_flag == AI_PACKET_FRAG_START)) {
        return TRUE;
    }
    return FALSE;
}

uint32_t __ai_get_send_payload_len(AI_SEND_PACKET_T *info, AI_FRAG_FLAG frag_flag)
{
    uint32_t len = 0;
    if (tuya_ai_is_need_attr(frag_flag)) {
        len += SIZEOF(AI_PAYLOAD_HEAD_T);
        len += __ai_get_send_attr_len(info);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        len += SIZEOF(len);
#endif
    }
    len += info->len;
    // AI_PROTO_D("packet len:%d", len);
    return len;
}

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
STATIC BOOL_T __ai_is_need_iv(AI_SEND_PACKET_T *info, AI_FRAG_FLAG frag_flag)
{
    if (info->writer) {
        // FIXME: fixed to AI_PACKET_SL0 for now, need to support other security level
        return FALSE;
    }

    if ((AI_PT_CLIENT_HELLO == info->type) || (AI_PACKET_FRAG_ING == frag_flag) ||
        (AI_PACKET_FRAG_END == frag_flag)) {
        return FALSE;
    }
    return TRUE;
}
#endif

STATIC uint32_t __ai_get_send_pkt_len(AI_SEND_PACKET_T *info, AI_FRAG_FLAG frag_flag)
{
    uint32_t len = 0;
    AI_PACKET_PT type = __ai_get_sl(info, FALSE);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    len = SIZEOF(AI_PACKET_HEAD_T);
    if (__ai_is_need_iv(info, frag_flag)) {
        len += AI_IV_LEN;
    }
    len += SIZEOF(len);
#else
    len = SIZEOF(AI_PACKET_HEAD_T_V2);
    len += SIZEOF(uint16_t);
#endif

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    len += __ai_get_send_payload_len(info, frag_flag);
#else
    if (type == AI_PACKET_RSA) {
        len += (AI_RSA_PKT_LEN + AI_IV_LEN + __ai_get_send_payload_len(info, frag_flag) + AI_GCM_TAG_LEN); //(RSA+IV+AES(BODY+TAG))
    } else {
        len += __ai_get_send_payload_len(info, frag_flag);
    }
#endif

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    len += AI_SIGN_LEN;
#else
    if (type == AI_PACKET_SL0) {
        len += AI_SIGN_LEN;
    }
#endif

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    if (type != AI_PACKET_SL0) {
        len += AI_ADD_PKT_LEN; // for tag and padding
        // if (type == AI_PACKET_SL4) {
        //     len += AI_GCM_TAG_LEN;
        // } else {
        //     len += AI_ADD_PKT_LEN;
        // }
    }
#else
    if (type != AI_PACKET_SL0 && type != AI_PACKET_RSA) {
        len += AI_ADD_PKT_LEN; // for tag and padding
    }
#endif
    AI_PROTO_D("uncrypt len:%d", len);
    return len;
}

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
STATIC int __ai_encrypt_add_pkcs(char *p, uint32_t len)
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
#endif

STATIC OPERATE_RET __ai_rsa_encrypt_info(char *key, char *out, uint32_t *outlen, char *data, uint32_t len)
{
    OPERATE_RET ret = OPRT_OK;
#if defined(AI_VERSION) && (0x02 == AI_VERSION)
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    uint32_t pk_len = strlen(key);
    uint8_t output[pk_len + 1];
    memset(output, 0, sizeof(output));
    uint32_t olen = 0;

    char *pers = "rsa_encrypt";
    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const uint8_t*) pers, strlen((const char *)pers))) != 0) {
        goto end;
    }
    if ((ret = mbedtls_pk_parse_public_key(&pk, (const uint8_t*) key, strlen(key) + 1)) != 0) {
        PR_ERR("Failed in mbedtls_pk_parse_public_key, ret = -0x%04X\n", -ret);
        goto end;
    }

    if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA)) {
        PR_ERR("The key is not an RSA key\n");
        goto end;
    }

    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(pk);
    mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);  // OAEP + SHA-256

    ret = mbedtls_pk_encrypt(&pk, (const uint8_t*)data, len, output, &olen, sizeof(output), mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        PR_ERR("Failed in mbedtls_pk_encrypt, ret = -0x%04X\n", -ret);
        goto end;
    }

    *outlen = olen;
    memcpy(out, output, olen);

end:
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
#endif
    return ret;
}

STATIC OPERATE_RET __ai_encrypt_clint_hello_info(char *pubkey, char *out, uint32_t *outlen, char *data, uint32_t len)
{
    //client hello RSA(AES KEY) + IV + AES(BODY + TAG)
    OPERATE_RET rt = OPRT_OK;
    uint32_t en_len = 0;
    uint32_t positon = 0;
    uint8_t tag[AI_GCM_TAG_LEN] = {0};
    uint8_t iv[AI_IV_LEN] = {0};
    char *aes_p = NULL;
    char *key = __ai_get_crypt_key();

    memcpy(iv, ai_basic_proto->iv_mask, AI_IV_LEN);
    xor_ivmask_with_sequence(iv, (rand() % 255 + 1));

    // tuya_debug_hex_dump("aes key  ", 64, (uint8_t *)key, AI_KEY_LEN);
    // tuya_debug_hex_dump("iv  ", 64, (uint8_t *)iv, AI_IV_LEN);

    if (OPRT_OK != __ai_rsa_encrypt_info(pubkey, out, &positon, key, AI_KEY_LEN)) {
        PR_ERR("RSA encrypt error \r\n");
        return OPRT_COM_ERROR;
    }

    memcpy(out + positon, iv, AI_IV_LEN);
    positon += AI_IV_LEN;
    // tuya_debug_hex_dump("RSA +iv ", 64, (uint8_t *)out, positon);

    aes_p = out + positon;
    memcpy(aes_p, data, len);

    rt = mbedtls_cipher_auth_encrypt_wrapper(
    &(CONST cipher_params_t) {
        .cipher_type = MBEDTLS_CIPHER_AES_256_GCM,
        .key = (unsigned char *)key,
        .key_len = AI_KEY_LEN,
        .nonce = (uint8_t *)iv,
        .nonce_len = AI_IV_LEN,
        .ad = NULL,
        .ad_len = 0,
        .data = (uint8_t *)aes_p,
        .data_len = len
    },
    (uint8_t *)aes_p, (size_t *)&en_len,
    tag, SIZEOF(tag));
    if (rt != OPRT_OK) {
        PR_ERR("aes128_gcm_encode error:%x", rt);
    }
    memcpy(aes_p + en_len, tag, SIZEOF(tag));
    en_len += SIZEOF(tag);

    *outlen = en_len + positon;

    // tuya_debug_hex_dump("encrypt_data", 64, (uint8_t *)out, *outlen);

    return rt;
}

STATIC OPERATE_RET __ai_encrypt_packet(AI_SEND_PACKET_T *info, char *data, uint32_t len, char *output, uint32_t *en_len, uint16_t sequence)
{
    OPERATE_RET rt = OPRT_OK;
    int data_out_len = 0;
    char *key = __ai_get_crypt_key();
    TUYA_CHECK_NULL_RETURN(key, OPRT_COM_ERROR);

    AI_PACKET_SL sl = __ai_get_sl(info, FALSE);
    if (sl == AI_PACKET_SL2) {
#if (AI_PACKET_SECURITY_LEVEL == AI_PACKET_SL2)
        memcpy(output, data, len);
        data_out_len = __ai_encrypt_add_pkcs(output, len);
        char nonce[12] = {0};
        memcpy(nonce, ai_basic_proto->encrypt_iv, SIZEOF(nonce));
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
        rt = tal_aes256_cbc_encode_raw((uint8_t *)output, data_out_len, (uint8_t *)key, (uint8_t *)ai_basic_proto->encrypt_iv, (uint8_t *)output);
        if (OPRT_OK != rt) {
            PR_ERR("aes128_cbc_encode error:%d", rt);
            return rt;
        }
        *en_len = data_out_len;
#endif
    } else if (sl == AI_PACKET_SL4) {
#if (AI_PACKET_SECURITY_LEVEL == AI_PACKET_SL4)

#if defined(AI_VERSION) && (0x02 == AI_VERSION)
        uint8_t iv[AI_IV_LEN] = {0};
        memcpy(iv, ai_basic_proto->iv_mask, AI_IV_LEN);
        xor_ivmask_with_sequence(iv, sequence);
#endif

        uint8_t tag[AI_GCM_TAG_LEN] = {0};
        memcpy(output, data, len);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        data_out_len = __ai_encrypt_add_pkcs(output, len);
#else
        data_out_len = len;
#endif
        rt = mbedtls_cipher_auth_encrypt_wrapper(
        &(CONST cipher_params_t) {
            .cipher_type = MBEDTLS_CIPHER_AES_256_GCM,
            .key = (unsigned char *)key,
            .key_len = AI_KEY_LEN,
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
            .nonce = (uint8_t *)ai_basic_proto->encrypt_iv,
#else
            .nonce = (uint8_t *)iv,
#endif
            .nonce_len = AI_IV_LEN,
            .ad = NULL,
            .ad_len = 0,
            .data = (uint8_t *)output,
            .data_len = data_out_len
        },
        (uint8_t *)output, (size_t *)en_len,
        tag, SIZEOF(tag));
        if (rt != OPRT_OK) {
            PR_ERR("aes128_gcm_encode error:%x", rt);
        }
        memcpy(output + *en_len, tag, SIZEOF(tag));
        *en_len += SIZEOF(tag);
        // tuya_debug_hex_dump("encrypt_data", 64, (uint8_t *)output, *en_len);
#endif
    } else if (sl == AI_PACKET_SL0) {
        AI_PROTO_D("sl:%d do not need crypt", sl);
        memcpy(output, data, len);
        *en_len = len;
    } else if (sl == AI_PACKET_RSA) {
        rt = __ai_encrypt_clint_hello_info(ai_basic_proto->rsa_public_key, output, en_len, data, len);
    } else {
        PR_ERR("sl:%d err", sl);
        rt = OPRT_COM_ERROR;
    }

    return rt;
}

STATIC OPERATE_RET __ai_decrypt_packet(char *data, uint32_t len, char *output, uint32_t *de_len, uint16_t sequence)
{
    OPERATE_RET rt = OPRT_OK;
    char *key = __ai_get_crypt_key();
    TUYA_CHECK_NULL_RETURN(key, OPRT_COM_ERROR);

    AI_PACKET_SL sl = __ai_get_sl(0, TRUE);
    if (sl == AI_PACKET_SL2) {
#if (AI_PACKET_SECURITY_LEVEL == AI_PACKET_SL2)
        char nonce[12] = {0};
        memcpy(nonce, ai_basic_proto->decrypt_iv, SIZEOF(nonce));
        rt = mbedtls_chacha20_crypt((uint8_t *)key, (uint8_t *)nonce, 0, len, (uint8_t *)data, (uint8_t *)output);
        if (OPRT_OK != rt) {
            PR_ERR("chacha20_crypt error:%d", rt);
            return rt;
        }
        *de_len = len - output[len - 1];
#endif
    } else if (sl == AI_PACKET_SL3) {
#if (AI_PACKET_SECURITY_LEVEL == AI_PACKET_SL3)
        rt = tal_aes256_cbc_decode_raw((uint8_t *)data, len, (uint8_t *)key, (uint8_t *)ai_basic_proto->decrypt_iv, (uint8_t *)output);
        if (OPRT_OK != rt) {
            PR_ERR("aes128_cbc_decode error:%d", rt);
            return rt;
        }
        *de_len = len - output[len - 1];
#endif
    } else if (sl == AI_PACKET_SL4) {
#if (AI_PACKET_SECURITY_LEVEL == AI_PACKET_SL4)

#if defined(AI_VERSION) && (0x02 == AI_VERSION)
        uint8_t iv[AI_IV_LEN] = {0};
        memcpy(iv, ai_basic_proto->iv_mask, AI_IV_LEN);
        xor_ivmask_with_sequence(iv, sequence);
#endif

        // tuya_debug_hex_dump("decrypt_data", 64, (uint8_t *)data, len - AI_GCM_TAG_LEN);
        // tuya_debug_hex_dump("decrypt_key", 64, (uint8_t *)key, AI_KEY_LEN);
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        // tuya_debug_hex_dump("decrypt_iv v1", 64, (uint8_t *)ai_basic_proto->decrypt_iv, AI_IV_LEN);
#else
        // tuya_debug_hex_dump("decrypt_iv v2", 64, (uint8_t *)iv, AI_IV_LEN);
#endif
        // tuya_debug_hex_dump("decrypt_tag", 64, (uint8_t *)(data + len - AI_GCM_TAG_LEN), AI_GCM_TAG_LEN);
        rt = mbedtls_cipher_auth_decrypt_wrapper(
        &(CONST cipher_params_t) {
            .cipher_type = MBEDTLS_CIPHER_AES_256_GCM,
            .key = (unsigned char *)key,
            .key_len = AI_KEY_LEN,
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
            .nonce = (uint8_t *)ai_basic_proto->decrypt_iv,
#else
            .nonce = (uint8_t *)iv,
#endif
            .nonce_len = AI_IV_LEN,
            .ad = NULL,
            .ad_len = 0,
            .data = (uint8_t *)data,
            .data_len = len - AI_GCM_TAG_LEN
        },
        (uint8_t *)output, (size_t *)de_len,
        (uint8_t *)(data + len - AI_GCM_TAG_LEN), AI_GCM_TAG_LEN);
        if (rt != OPRT_OK) {
            PR_ERR("aes128_gcm_decode error:%x", rt);
            return rt;
        }
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        *de_len = *de_len - output[*de_len - 1];
#endif

        // PR_DEBUG("aes128_gcm_decode output len:%d", *de_len);
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

STATIC OPERATE_RET __ai_pack_payload(AI_SEND_PACKET_T *info, char *payload_buf, uint32_t *payload_len, AI_FRAG_FLAG frag, uint32_t origin_len, uint16_t sequence)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t idx = 0, attr_len = 0, packet_len = 0;
    uint32_t offset = 0;
    TUYA_CHECK_NULL_RETURN(info, OPRT_INVALID_PARM);
    packet_len = __ai_get_send_payload_len(info, frag);

    char *buf = OS_MALLOC(packet_len);
    TUYA_CHECK_NULL_RETURN(buf, OPRT_MALLOC_FAILED);
    memset(buf, 0, packet_len);

    if (tuya_ai_is_need_attr(frag)) {
        AI_PAYLOAD_HEAD_T payload_head = {0};
        payload_head.type = info->type;
        payload_head.attribute_flag = AI_NO_ATTR;
        offset = SIZEOF(AI_PAYLOAD_HEAD_T);
        if (info->count != 0) {
            payload_head.attribute_flag = AI_HAS_ATTR;
            attr_len = UNI_HTONL(__ai_get_send_attr_len(info) - SIZEOF(attr_len));
            memcpy(buf + offset, &attr_len, SIZEOF(attr_len));
            offset += SIZEOF(attr_len);
            for (idx = 0; idx < info->count; idx++) {
                AI_ATTR_TYPE type = UNI_HTONS(info->attrs[idx]->type);
                memcpy(buf + offset, &type, SIZEOF(AI_ATTR_TYPE));
                offset += SIZEOF(AI_ATTR_TYPE);
                AI_ATTR_PT payload_type = info->attrs[idx]->payload_type;

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
                memcpy(buf + offset, &payload_type, SIZEOF(AI_ATTR_PT));
                offset += SIZEOF(AI_ATTR_PT);
#endif
                uint32_t attr_idx_len = info->attrs[idx]->length;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
                uint32_t length = UNI_HTONL(attr_idx_len);
#else
                uint16_t length = attr_idx_len;
                length = UNI_HTONS(length);
#endif
                memcpy(buf + offset, &length, SIZEOF(length));
                offset += SIZEOF(length);
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
                    OS_FREE(buf);
                    return OPRT_COM_ERROR;
                }
                offset += attr_idx_len;
                AI_PROTO_D("attr idx:%d, len:%d", idx, offset);
            }
        }
        memcpy(buf, &payload_head, SIZEOF(AI_PAYLOAD_HEAD_T));
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        uint32_t info_len = UNI_HTONL(origin_len);
        memcpy(buf + offset, &info_len, SIZEOF(info->len));
        AI_PROTO_D("payload len:%d", origin_len);
        offset += SIZEOF(info->len);
#endif
    }

    memcpy(buf + offset, info->data, info->len);
    offset += info->len;
    AI_PROTO_D("payload len:%d, offset:%d", packet_len, offset);

    // tuya_debug_hex_dump("payload_uncrypt", 64, (uint8_t *)buf, packet_len);
    rt = __ai_encrypt_packet(info, buf, packet_len, payload_buf, payload_len, sequence);
    if (OPRT_OK != rt) {
        PR_ERR("encrypt packet failed, rt:%d", rt);
    }

    OS_FREE(buf);
    return rt;
}

STATIC BOOL_T __ai_check_attr_vaild(AI_ATTRIBUTE_T *attr)
{
    AI_ATTR_TYPE type = attr->type;
    AI_ATTR_PT payload_type = attr->payload_type;
    uint32_t length = attr->length;

    if ((type == AI_ATTR_CONNECT_STATUS_CODE) ||
        (type == AI_ATTR_CONNECT_CLOSE_ERR_CODE)) {
        if (payload_type != ATTR_PT_U16) {
            return FALSE;
        }
    }
    switch (payload_type) {
    case ATTR_PT_U8:
        if (length != SIZEOF(uint8_t)) {
            return FALSE;
        }
        break;
    case ATTR_PT_U16:
        if (length != SIZEOF(uint16_t)) {
            return FALSE;
        }
        break;
    case ATTR_PT_U32:
        if (length != SIZEOF(uint32_t)) {
            return FALSE;
        }
        break;
    case ATTR_PT_U64:
        if (length != SIZEOF(uint64_t)) {
            return FALSE;
        }
        break;
    case ATTR_PT_STR:
    case ATTR_PT_BYTES:
        if (length == 0) {
            return FALSE;
        }
        break;
    }
    return TRUE;
}

uint32_t tuya_ai_get_attr_num(char *de_buf, uint32_t attr_len)
{
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    uint32_t offset = 0, idx = 0, length = 0;
    while (offset < attr_len) {
        offset += SIZEOF(AI_ATTR_TYPE);
        offset += SIZEOF(AI_ATTR_PT);
        memcpy(&length, de_buf + offset, SIZEOF(length));
        offset += SIZEOF(length);
        offset += UNI_NTOHL(length);
        idx++;
    }
#else
    uint16_t offset = 0, idx = 0, length = 0;
    while (offset < attr_len) {
        offset += SIZEOF(AI_ATTR_TYPE);
        memcpy(&length, de_buf + offset, SIZEOF(length));
        offset += SIZEOF(length);
        offset += UNI_NTOHS(length);
        idx++;
    }
#endif
    return idx;
}

OPERATE_RET tuya_ai_get_attr_value(char *de_buf, uint32_t *offset, AI_ATTRIBUTE_T *attr)
{
    OPERATE_RET rt = OPRT_OK;
    AI_ATTR_TYPE type = 0;
    AI_ATTR_PT payload_type = 0;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    uint32_t length = 0;
#else
    uint16_t length = 0;
#endif
    AI_ATTR_VALUE value = {0};
    memcpy(&type, de_buf + *offset, SIZEOF(type));
    attr->type = UNI_NTOHS(type);
    *offset += SIZEOF(type);

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    memcpy(&payload_type, de_buf + *offset, SIZEOF(payload_type));
    attr->payload_type = payload_type;
    *offset += SIZEOF(payload_type);
#else
    attr->payload_type = __ai_get_attr_type(attr->type);
    payload_type = attr->payload_type;

#endif

    memcpy(&length, de_buf + *offset, SIZEOF(length));
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    attr->length = UNI_NTOHL(length);
#else
    attr->length = UNI_NTOHS(length);
#endif
    *offset += SIZEOF(length);

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

STATIC OPERATE_RET __ai_packet_write(AI_SEND_PACKET_T *info, AI_FRAG_FLAG frag, uint32_t origin_len)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t payload_len = 0, offset = 0;
    AI_PACKET_SL sl = __ai_get_sl(info, FALSE);
    uint8_t signature[AI_SIGN_LEN] = {0};
    uint16_t sequence;
    if (info->writer && info->writer->update) {
        rt = info->writer->update(AI_STAGE_PRE_WRITE, NULL, info);
        if (OPRT_OK != rt) {
            PR_ERR("pre write failed, rt:%d", rt);
            return rt;
        }
        info->writer->update(AI_STAGE_GET_SEQUENCE, &sequence, info);
    } else {
        sequence = ai_basic_proto->sequence_out++;
        if (ai_basic_proto->sequence_out == 0) {
            ai_basic_proto->sequence_out = 1;
        }
    }
    AI_PROTO_D("send packet sequence:%d, frag:%d", sequence, frag);

    uint32_t uncrypt_len = __ai_get_send_pkt_len(info, frag);
    if (uncrypt_len > AI_MAX_FRAGMENT_LENGTH) {
        PR_ERR("send packet too long, len: %d", uncrypt_len);
        return OPRT_COM_ERROR;
    }
    char *send_pkt_buf = OS_MALLOC(uncrypt_len);
    TUYA_CHECK_NULL_RETURN(send_pkt_buf, OPRT_MALLOC_FAILED);
    memset(send_pkt_buf, 0, uncrypt_len);

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    uint32_t head_len = SIZEOF(AI_PACKET_HEAD_T);
    AI_PROTO_D("head len:%d", head_len);

    AI_PACKET_HEAD_T head = {0};
    head.version = 0x01;
    head.sequence = UNI_HTONS(sequence);
    head.frag_flag = frag;
    head.security_level = sl;
    head.iv_flag = __ai_is_need_iv(info, frag);
#else
    uint32_t head_len = SIZEOF(AI_PACKET_HEAD_T_V2);
    AI_PROTO_D("head len:%d, version:%d", head_len, AI_VERSION);
    AI_PACKET_HEAD_T_V2 head = {0};
    head.version = 0x02;
    head.sequence = UNI_HTONS(sequence);
    head.frag_flag = frag;
#endif

    memcpy(send_pkt_buf, &head, head_len);
    offset += head_len;

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    if (head.iv_flag) {
        memcpy(send_pkt_buf + offset, ai_basic_proto->encrypt_iv, AI_IV_LEN);
        offset += AI_IV_LEN;
    }
#endif

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    uint32_t length = 0;
#else
    short length = 0;
#endif
    offset += SIZEOF(length);

    rt = __ai_pack_payload(info, send_pkt_buf + offset, &payload_len, frag, origin_len, sequence);
    if (OPRT_OK != rt) {
        goto EXIT;
    }
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    length = UNI_HTONL(payload_len + AI_SIGN_LEN);

    if (head.iv_flag) {
        memcpy(send_pkt_buf + head_len + AI_IV_LEN, &length, SIZEOF(length));
    } else {
        memcpy(send_pkt_buf + head_len, &length, SIZEOF(length));
    }

    rt = __ai_packet_sign(send_pkt_buf, signature);
    if (OPRT_OK != rt) {
        goto EXIT;
    }
    offset += payload_len;
    memcpy(send_pkt_buf + offset, signature, AI_SIGN_LEN);
    offset += AI_SIGN_LEN;
#else

    if (AI_PACKET_SL4 != sl && AI_PACKET_RSA != sl) {
        length = UNI_HTONS((payload_len + AI_SIGN_LEN));
        memcpy(send_pkt_buf + head_len, &length, SIZEOF(length));

        rt = __ai_packet_sign(send_pkt_buf, signature);
        if (OPRT_OK != rt) {
            goto EXIT;
        }
        offset += payload_len;
        memcpy(send_pkt_buf + offset, signature, AI_SIGN_LEN);
        offset += AI_SIGN_LEN;
    } else {
        length = UNI_HTONS(payload_len);
        memcpy(send_pkt_buf + head_len, &length, SIZEOF(length));
        offset += payload_len;
        AI_PROTO_D("use sl4, donot use packet sign");
    }
#endif

    // tuya_debug_hex_dump("send_pkt_buf", 64, (uint8_t *)send_pkt_buf, offset);

    AI_PROTO_D("send packet len:%d", payload_len + AI_SIGN_LEN);
    AI_PROTO_D("send payload len:%d", payload_len);

    AI_PROTO_D("send total len:%d, send_len:%d", offset, uncrypt_len);
    // tuya_debug_hex_dump("send_pkt_buf", 64, (uint8_t *)send_pkt_buf, head_len);
    AI_PACKET_WRITER_T *writer = info->writer;
    // if no writer, use default writer
    if (!writer) {
        writer = &s_default_packet_writer;
        writer->user_data = ai_basic_proto->transporter;
    }
    rt = writer->write(writer, send_pkt_buf, offset);
    if (OPRT_OK != rt) {
        PR_ERR("write packet failed, rt:%d", rt);
        goto EXIT;
    }

EXIT:
    OS_FREE(send_pkt_buf);
    return rt;
}

VOID tuya_ai_free_attribute(AI_ATTRIBUTE_T *attr)
{
    if (!attr) {
        return;
    }
    switch (attr->payload_type) {
    case ATTR_PT_BYTES:
        if (attr->value.bytes) {
            OS_FREE(attr->value.bytes);
        }
        break;
    case ATTR_PT_STR:
        if (attr->value.str) {
            OS_FREE(attr->value.str);
        }
        break;
    default:
        break;
    }
    OS_FREE(attr);
}

VOID tuya_ai_free_attrs(AI_SEND_PACKET_T *pkt)
{
    uint32_t attr_idx = 0;
    for (attr_idx = 0; attr_idx < pkt->count; attr_idx++) {
        if (pkt->attrs[attr_idx]) {
            tuya_ai_free_attribute(pkt->attrs[attr_idx]);
        }
    }
}

STATIC VOID __ai_basic_reset_send_frag(AI_PACKET_PT type)
{
    uint8_t idx = type - AI_PT_VIDEO;
    ai_basic_proto->send_frag_mng[idx].offset = 0;
}

STATIC VOID __ai_basic_get_send_frag(AI_SEND_PACKET_T *info, AI_FRAG_FLAG *frag_flag)
{
    uint32_t *offset = NULL;
    AI_PACKET_PT type = info->type;
    uint32_t frag_len = info->len;
    uint32_t total_len = info->total_len;
    uint32_t biz_head_len = 0;
    uint8_t idx = type - AI_PT_VIDEO;
    uint32_t actual_len = 0, actual_total_len = 0;
#if defined(AI_VERSION) && (0x02 == AI_VERSION)
    uint32_t seq_len = 0;
    char seq_array[10] = {0};
#endif

    if (idx >= SIZEOF(ai_basic_proto->send_frag_mng)) {
        PR_ERR("send frag mng idx err, type:%d", type);
        return;
    }
    AI_PROTO_D("type:%d, idx:%d, frag len:%d", type, idx, frag_len);
    if (type == AI_PT_VIDEO) {
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        biz_head_len = SIZEOF(AI_VIDEO_HEAD_T);
#else
        biz_head_len = SIZEOF(AI_VIDEO_HEAD_T_V2);
#endif
        actual_len = frag_len - biz_head_len;
        actual_total_len = total_len - biz_head_len;
    } else if (type == AI_PT_AUDIO) {
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        biz_head_len = SIZEOF(AI_AUDIO_HEAD_T);
#else
        biz_head_len = SIZEOF(AI_AUDIO_HEAD_T_V2);
#endif
        actual_len = frag_len - biz_head_len;
        actual_total_len = total_len - biz_head_len;
    } else if (type == AI_PT_IMAGE) {
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        biz_head_len = SIZEOF(AI_IMAGE_HEAD_T);
#else
        biz_head_len = SIZEOF(AI_IMAGE_HEAD_T_V2);
#endif
        actual_len = frag_len - biz_head_len;
        actual_total_len = total_len - biz_head_len;
    } else if (type == AI_PT_FILE) {
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        biz_head_len = SIZEOF(AI_FILE_HEAD_T);
#else
        seq_len = tuya_ai_basic_get_var_seq(type, seq_array);
        tuya_ai_basic_update_var_seq(type);
        biz_head_len = SIZEOF(AI_FILE_HEAD_T_V2) + seq_len;
#endif
        actual_len = frag_len - biz_head_len;
        actual_total_len = total_len - biz_head_len;
    } else if (type == AI_PT_TEXT) {
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        biz_head_len = SIZEOF(AI_TEXT_HEAD_T);
#else
        seq_len = tuya_ai_basic_get_var_seq(type, seq_array);
        tuya_ai_basic_update_var_seq(type);
        biz_head_len = SIZEOF(AI_TEXT_HEAD_T_V2) + seq_len;
#endif
        actual_len = frag_len - biz_head_len;
        actual_total_len = total_len - biz_head_len;
    }

    if (info->writer && info->writer->update) {
        info->writer->update(AI_STAGE_GET_FRAG_OFFSET, &offset, info);
    }

    offset = &(ai_basic_proto->send_frag_mng[idx].offset);
    if (*offset == 0) {
        *offset += actual_len;
        *frag_flag = AI_PACKET_FRAG_START;
    } else if ((*offset + actual_len) == actual_total_len) {
        *offset = 0;
        *frag_flag = AI_PACKET_FRAG_END;
        info->data += biz_head_len;
        info->len -= biz_head_len;
    } else if ((*offset + actual_len) < actual_total_len) {
        *offset += actual_len;
        *frag_flag = AI_PACKET_FRAG_ING;
        info->data += biz_head_len;
        info->len -= biz_head_len;
    } else {
        PR_ERR("send packet err, offset:%d, frag_len:%d, total_len:%d", *offset, actual_len, actual_total_len);
        *offset = 0;
        return;
    }
    AI_PROTO_D("send packet offset:%d, frag_len:%d, frag:%d, total_len:%d", *offset, actual_len, *frag_flag, actual_total_len);
    return;
}

OPERATE_RET tuya_ai_basic_pkt_frag_send(AI_SEND_PACKET_T *info)
{
    OPERATE_RET rt = OPRT_OK;
    if (info->len > AI_MAX_FRAGMENT_LENGTH) {
        PR_ERR("send packet too long, len: %d, frag:%d", info->len, AI_MAX_FRAGMENT_LENGTH);
        return OPRT_COM_ERROR;
    }
    AI_FRAG_FLAG frag_flag = AI_PACKET_NO_FRAG;
    if (!ai_basic_proto) {
        tuya_ai_free_attrs(info);
        __ai_basic_reset_send_frag(info->type);
        PR_ERR("ai basic proto was null");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(ai_basic_proto->mutex);
    if (!ai_basic_proto->connected) {
        tuya_ai_free_attrs(info);
        __ai_basic_reset_send_frag(info->type);
        tal_mutex_unlock(ai_basic_proto->mutex);
        PR_ERR("ai proto not connected");
        return OPRT_COM_ERROR;
    }

    __ai_basic_get_send_frag(info, &frag_flag);
    rt = __ai_packet_write(info, frag_flag, info->total_len);
    if (rt != OPRT_OK) {
        __ai_basic_reset_send_frag(info->type);
    }

    tuya_ai_free_attrs(info);
    tal_mutex_unlock(ai_basic_proto->mutex);
    return rt;
}

OPERATE_RET tuya_ai_basic_pkt_send(AI_SEND_PACKET_T *info)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0, frag_len = 0, attr_len = 0;
    uint32_t one_packet_len = 0;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    uint32_t min_pkt_len = SIZEOF(AI_PACKET_HEAD_T) + (2 * AI_ADD_PKT_LEN); // AI_SIGN_LEN + AI_IV_LEN + AI_ADD_PKT_LEN
#else
    uint32_t min_pkt_len = SIZEOF(AI_PACKET_HEAD_T_V2) + (2 * AI_ADD_PKT_LEN); // AI_SIGN_LEN + AI_IV_LEN + AI_ADD_PKT_LEN
#endif

    uint32_t origin_len = info->len;
    char *origin_data = info->data;
    AI_PROTO_D("send payload len:%d", origin_len);

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

STATIC BOOL_T __ai_check_attr_created(AI_SEND_PACKET_T *pkt)
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
        return FALSE;
    }
    return TRUE;
}

AI_ATTRIBUTE_T* tuya_ai_create_attribute(AI_ATTR_TYPE type, AI_ATTR_PT payload_type, VOID *value, uint32_t len)
{
    AI_ATTRIBUTE_T *attr = (AI_ATTRIBUTE_T *)OS_MALLOC(SIZEOF(AI_ATTRIBUTE_T));
    if (!attr) {
        PR_ERR("malloc attr failed");
        return NULL;
    }
    memset(attr, 0, SIZEOF(AI_ATTRIBUTE_T));
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
        attr->value.bytes = OS_MALLOC(len);
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

STATIC OPERATE_RET __create_conn_close_attrs(AI_SEND_PACKET_T *pkt, AI_STATUS_CODE code)
{
    uint32_t attr_idx = 0;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_CONNECT_CLOSE_ERR_CODE, ATTR_PT_U16, &code, SIZEOF(AI_STATUS_CODE));
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __create_auth_req_attrs(AI_SEND_PACKET_T *pkt, AI_SERVER_CFG_INFO_T *cfg)
{
    uint32_t attr_idx = 0;
    char *user_name = cfg->username;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_USER_NAME, ATTR_PT_STR, user_name, strlen(user_name));
    char *password = cfg->credential;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_PASSWORD, ATTR_PT_STR, password, strlen(password));
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __create_clt_hello_attrs(AI_SEND_PACKET_T *pkt, AI_SERVER_CFG_INFO_T *cfg)
{
    uint32_t attr_idx = 0;

    uint8_t client_type = ATTR_CLIENT_TYPE_DEVICE;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_CLIENT_TYPE, ATTR_PT_U8, &client_type, SIZEOF(uint8_t));
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    char *client_id = cfg->client_id;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_CLIENT_ID, ATTR_PT_STR, client_id, strlen(client_id));
    char *derived_algorithm = cfg->derived_algorithm;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_DERIVED_ALGORITHM, ATTR_PT_STR, derived_algorithm, strlen(derived_algorithm));
    char *derived_iv = cfg->derived_iv;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_DERIVED_IV, ATTR_PT_STR, derived_iv, strlen(derived_iv));
    char *crypt_random = ai_basic_proto->crypt_random;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_ENCRYPT_RANDOM, ATTR_PT_STR, crypt_random, AI_RANDOM_LEN);
    char *sign_random = ai_basic_proto->sign_random;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SIGN_RANDOM, ATTR_PT_STR, sign_random, AI_RANDOM_LEN);
#else
    //ClientID
    char *derived_client_id = cfg->derived_client_id;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_CLIENT_ID, ATTR_PT_STR, derived_client_id, strlen(derived_client_id));


    uint8_t security_suit[34] = {0};
    uint8_t pisiton = 0;
    security_suit[pisiton] = AI_PACKET_SL4; //L4
    pisiton += 1;
    security_suit[pisiton] = 0X00; //sign level
    pisiton += 1;
    memcpy(security_suit + 2, ai_basic_proto->crypt_random, AI_RANDOM_LEN);
    pisiton += AI_RANDOM_LEN;

    // tuya_debug_hex_dump("random ", 64, (uint8_t *)ai_basic_proto->crypt_random, AI_RANDOM_LEN);


    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SECURITSUIT_TYPE, ATTR_PT_BYTES, security_suit, sizeof(security_suit));
#endif
    //MaxFragmentLength
    uint32_t max_fragment_len = AI_MAX_FRAGMENT_LENGTH;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_MAX_FRAGMENT_LEN, ATTR_PT_U32, &max_fragment_len, SIZEOF(uint32_t));
    if (AI_READ_SOCKET_BUF_SIZE > 0) {
        uint32_t read_buf_size = AI_READ_SOCKET_BUF_SIZE;
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_READ_BUFFER_SIZE, ATTR_PT_U32, &read_buf_size, SIZEOF(uint32_t));
    }
    if (AI_WRITE_SOCKET_BUF_SIZE > 0) {
        uint32_t write_buf_size = AI_WRITE_SOCKET_BUF_SIZE;
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_WRITE_BUFFER_SIZE, ATTR_PT_U32, &write_buf_size, SIZEOF(uint32_t));
    }
    char *connection_id = ai_basic_proto->connection_id;
    if (connection_id) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_CONNECTION_ID, ATTR_PT_STR, connection_id, strlen(connection_id));
    }
    if (AI_HEARTBEAT_INTERVAL > 0) {
        uint32_t ping_interval = AI_HEARTBEAT_INTERVAL * 1000;
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_PING_INTERVAL, ATTR_PT_U32, &ping_interval, SIZEOF(uint32_t));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __create_refresh_req_attrs(AI_SEND_PACKET_T *pkt)
{
    uint32_t attr_idx = 0;
    char *connection_id = ai_basic_proto->connection_id;
    if (connection_id) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_CONNECTION_ID, ATTR_PT_STR, connection_id, strlen(connection_id));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

OPERATE_RET tuya_ai_basic_refresh_req(VOID)
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
    memcpy(&attr_len, data + SIZEOF(AI_PAYLOAD_HEAD_T), SIZEOF(attr_len));
    attr_len = UNI_NTOHL(attr_len);
    uint32_t offset = SIZEOF(AI_PAYLOAD_HEAD_T) + SIZEOF(attr_len);
    char *attr_data = data + offset;
    offset = 0;

    while (offset < attr_len) {
        memset(&attr, 0, SIZEOF(AI_ATTRIBUTE_T));
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

OPERATE_RET tuya_ai_refresh_resp(char *de_buf, uint32_t attr_len, uint64_t *expire)
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
            *expire = attr[idx].value.u64;
            AI_PROTO_D("refresh expire ts:%llu", *expire);
        } else {
            PR_ERR("unknow attr type:%d", attr[idx].type);
        }
    }
    tuya_free_user_attrs(attr);
    return rt;
}

OPERATE_RET tuya_ai_basic_client_hello(AI_SERVER_CFG_INFO_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_CLIENT_HELLO;

#if defined(AI_VERSION) && (0x02 == AI_VERSION)
    ai_basic_proto->rsa_public_key = mm_strdup(cfg->rsa_public_key);
#endif

    rt = __create_clt_hello_attrs(&pkt, cfg);
    if (OPRT_OK != rt) {
        return rt;
    }
    AI_PROTO_D("send client hello");
    return tuya_ai_basic_pkt_send(&pkt);
}

OPERATE_RET tuya_ai_basic_auth_req(AI_SERVER_CFG_INFO_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_AUTH_REQ;
    rt = __create_auth_req_attrs(&pkt, cfg);
    if (OPRT_OK != rt) {
        return rt;
    }
    AI_PROTO_D("send auth req");
    return tuya_ai_basic_pkt_send(&pkt);
}

VOID tuya_ai_basic_disconnect(VOID)
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
    ai_basic_proto->connected = FALSE;
    // tuya_ai_basic_disconnect();
    return rt;
}

STATIC int __ai_baisc_read_pkt_head(char *recv_buf)
{
    int offset = SIZEOF(AI_PACKET_HEAD_T);
    int need_recv_len = offset;
    int total_recv_len = 0;
    int recv_len = 0;

    while (total_recv_len < need_recv_len) {
        recv_len = tuya_transporter_read(ai_basic_proto->transporter, (uint8_t *)recv_buf + total_recv_len, need_recv_len - total_recv_len, AI_SEND_SOCKET_TIMEOUT);
        if (recv_len <= 0) {
            if (total_recv_len == 0) {
                return recv_len;
            } else {
                PR_ERR("recv head err, rt:%d, %d, %d", recv_len, need_recv_len, total_recv_len);
                return OPRT_COM_ERROR;
            }
        }
        total_recv_len += recv_len;
    }

    need_recv_len = SIZEOF(uint32_t);

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    AI_PACKET_HEAD_T *head = (AI_PACKET_HEAD_T *)recv_buf;
    if (head->iv_flag) {
        need_recv_len += AI_IV_LEN;
    }
#endif

    total_recv_len = 0;
    while (total_recv_len < need_recv_len) {
        recv_len = tuya_transporter_read(ai_basic_proto->transporter, (uint8_t *)recv_buf + offset + total_recv_len, need_recv_len - total_recv_len, AI_SEND_SOCKET_TIMEOUT);
        if (recv_len <= 0) {
            PR_ERR("recv length err, rt:%d, %d", recv_len, need_recv_len);
            return OPRT_COM_ERROR;
        }
        total_recv_len += recv_len;
    }
    offset += need_recv_len;

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    if (head->iv_flag) {
        memcpy(ai_basic_proto->decrypt_iv, recv_buf + SIZEOF(AI_PACKET_HEAD_T), AI_IV_LEN);
    }
#endif
    AI_PROTO_D("recv head len:%d", offset);
    return offset;
}

VOID tuya_ai_basic_pkt_free(char *data)
{
    if (data == ai_basic_proto->recv_frag_mng.data) {
        OS_FREE(data);
        ai_basic_proto->recv_frag_mng.data = NULL;
        memset(&ai_basic_proto->recv_frag_mng, 0, SIZEOF(AI_RECV_FRAG_MNG_T));
    } else {
        OS_FREE(data);
    }
}

VOID tuya_ai_basic_set_frag(BOOL_T flag)
{
    if (!ai_basic_proto) {
        PR_ERR("please set after ai basic proto init");
        return;
    }
    ai_basic_proto->frag_flag = flag;
}

STATIC BOOL_T __ai_basic_get_frag_flag()
{
    return ai_basic_proto->frag_flag;
}

OPERATE_RET tuya_ai_basic_pkt_read(char **out, uint32_t *out_len, AI_FRAG_FLAG *out_frag)
{
    OPERATE_RET rt = OPRT_OK;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    uint8_t calc_sign[AI_SIGN_LEN] = {0};
    uint8_t packet_sign[AI_SIGN_LEN] = {0};
#endif
    char *decrypt_buf = NULL;
    char *recv_buf = ai_basic_proto->recv_buf;
    TUYA_CHECK_NULL_RETURN(recv_buf, OPRT_COM_ERROR);

    memset(recv_buf, 0, SIZEOF(ai_basic_proto->recv_buf));
    AI_PROTO_D("recv packet ing");
    int recv_len = __ai_baisc_read_pkt_head(recv_buf);
    if (recv_len <= 0) {
        goto EXIT;
    }

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    AI_PACKET_HEAD_T *head = (AI_PACKET_HEAD_T *)recv_buf;
    AI_PROTO_D("recv packet ver:%d", head->version);
    AI_PROTO_D("recv packet seq:%d", UNI_NTOHS(head->sequence));
    AI_PROTO_D("recv packet frag:%d", head->frag_flag);
    AI_PROTO_D("recv packet sl:%d", head->security_level);
    AI_PROTO_D("recv packet iv flag:%d", head->iv_flag);
#else
    // tuya_debug_hex_dump("recv ", 64, (uint8_t *)recv_buf, recv_len);
    AI_PACKET_HEAD_T_V2 *head = (AI_PACKET_HEAD_T_V2 *)recv_buf;
    AI_PROTO_D("recv packet ver:%d", head->version);
    AI_PROTO_D("recv packet seq:%d", UNI_NTOHS(head->sequence));
    AI_PROTO_D("recv packet frag:%d", head->frag_flag);
    AI_PROTO_D("recv packet res:%d", head->reserve);

#endif

    uint32_t head_len = __ai_get_head_len(recv_buf);
    uint32_t packet_len = __ai_get_packet_len(recv_buf);

    AI_PROTO_D("recv head len:%d", head_len);
    AI_PROTO_D("recv packet len:%d", packet_len);

    if (packet_len + head_len > SIZEOF(ai_basic_proto->recv_buf)) {
        PR_ERR("recv packet too long, pkt len:%u, head len:%u", packet_len, head_len);
        recv_len = OPRT_RESOURCE_NOT_READY;
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
    BOOL_T read_timeout_flag = FALSE;
    while (packet_len + head_len > offset) {
        continue_recv_len = packet_len + head_len - offset;
        recv_len = tuya_transporter_read(ai_basic_proto->transporter, (uint8_t *)(recv_buf + offset), continue_recv_len, AI_SEND_SOCKET_TIMEOUT);
        if (recv_len <= 0) {
            PR_ERR("read ai pkt body failed, rt:%d, %d, %d", recv_len, continue_recv_len, read_timeout_flag);
            if ((recv_len == OPRT_RESOURCE_NOT_READY) && (!read_timeout_flag)) {
                PR_NOTICE("continue read again, because of resource not ready");
                read_timeout_flag = TRUE;
                tal_system_sleep(100);
                continue;
            }
            recv_len = OPRT_COM_ERROR;
            goto EXIT;
        }
        offset += recv_len;
    }

#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    rt = __ai_packet_sign(recv_buf, calc_sign);
    if (OPRT_OK != rt) {
        PR_ERR("packet sign failed, rt:%d", rt);
        goto EXIT;
    }

    AI_PROTO_D("sign ok");
    uint32_t payload_len = __ai_get_payload_len(recv_buf);
    char *payload = recv_buf + head_len;
    memcpy(packet_sign, payload + payload_len, AI_SIGN_LEN);
    if (memcmp(calc_sign, packet_sign, SIZEOF(calc_sign))) {
        PR_ERR("packet sign error");
        // tuya_debug_hex_dump("calc_sign", AI_SIGN_LEN, calc_sign, AI_SIGN_LEN);
        // tuya_debug_hex_dump("packet_sign", AI_SIGN_LEN, packet_sign, AI_SIGN_LEN);
        recv_len = OPRT_RESOURCE_NOT_READY;
        goto EXIT;
    }
#else
    uint32_t payload_len = __ai_get_payload_len(recv_buf);
    char *payload = recv_buf + head_len;
#endif
    uint32_t decrypt_len = 0;
    decrypt_buf = OS_MALLOC(packet_len + head_len + AI_ADD_PKT_LEN);
    TUYA_CHECK_NULL_RETURN(decrypt_buf, OPRT_MALLOC_FAILED);
    memset(decrypt_buf, 0, packet_len + head_len + AI_ADD_PKT_LEN);
    rt = __ai_decrypt_packet(payload, payload_len, decrypt_buf, &decrypt_len, UNI_NTOHS(head->sequence));
    if (OPRT_OK != rt) {
        PR_ERR("decrypt packet failed, rt:%d", rt);
        goto EXIT;
    }
    AI_PROTO_D("decrypt len:%d", decrypt_len);
    AI_PROTO_D("frag flag:%d, sdk frag flag:%d", head->frag_flag, __ai_basic_get_frag_flag());

    if (!__ai_basic_get_frag_flag()) {
        AI_FRAG_FLAG current_frag_flag = head->frag_flag;
        AI_FRAG_FLAG last_frag_flag = ai_basic_proto->recv_frag_mng.frag_flag;
        if ((last_frag_flag == AI_PACKET_FRAG_START) || (last_frag_flag == AI_PACKET_FRAG_ING)) {
            if ((current_frag_flag != AI_PACKET_FRAG_ING) && (current_frag_flag != AI_PACKET_FRAG_END)) {
                PR_ERR("recv start frag packet, but not continue %d, %d", current_frag_flag, last_frag_flag);
                goto EXIT;
            }
        }

        AI_PROTO_D("frag flag:%d", head->frag_flag);
        AI_PROTO_D("frag mng info, flag:%d, offset:%d", ai_basic_proto->recv_frag_mng.frag_flag, ai_basic_proto->recv_frag_mng.offset);
        if (current_frag_flag == AI_PACKET_FRAG_START) {
            uint32_t origin_len = 0, frag_offset = 0, attr_len = 0, frag_total_len = 0;
            AI_PAYLOAD_HEAD_T *pkt_head = (AI_PAYLOAD_HEAD_T *)decrypt_buf;
            if (pkt_head->attribute_flag == AI_HAS_ATTR) {
                frag_offset = SIZEOF(AI_PAYLOAD_HEAD_T);
                memcpy(&attr_len, decrypt_buf + frag_offset, SIZEOF(attr_len));
                frag_offset += SIZEOF(attr_len);
                attr_len = UNI_NTOHL(attr_len);
                frag_offset += attr_len;
                memcpy(&origin_len, decrypt_buf + frag_offset, SIZEOF(origin_len));
                origin_len = UNI_NTOHL(origin_len);
                AI_PROTO_D("recv start frag packet with attr, origin len:%d", origin_len);
            } else {
                memcpy(&origin_len, decrypt_buf + SIZEOF(AI_PAYLOAD_HEAD_T), SIZEOF(origin_len));
                origin_len = UNI_NTOHL(origin_len);
                AI_PROTO_D("recv start frag packet, origin len:%d", origin_len);
            }
            if (origin_len <= decrypt_len) {
                PR_ERR("origin len error, origin len:%d, decrypt len:%d", origin_len, decrypt_len);
                goto EXIT;
            }
            memset(&ai_basic_proto->recv_frag_mng, 0, SIZEOF(AI_RECV_FRAG_MNG_T));
            ai_basic_proto->recv_frag_mng.frag_flag = current_frag_flag;
            frag_total_len = origin_len + frag_offset + AI_ADD_PKT_LEN;
            AI_PROTO_D("frag_total_len %d", frag_total_len);
            ai_basic_proto->recv_frag_mng.data = OS_MALLOC(frag_total_len);
            if (!ai_basic_proto->recv_frag_mng.data) {
                PR_ERR("malloc origin data failed len:%d", decrypt_len);
                goto EXIT;
            }
            AI_PROTO_D("malloc recv_frag_mng data addr %p", ai_basic_proto->recv_frag_mng.data);
            memset(ai_basic_proto->recv_frag_mng.data, 0, frag_total_len);
            memcpy(ai_basic_proto->recv_frag_mng.data, decrypt_buf, decrypt_len);
            ai_basic_proto->recv_frag_mng.offset = decrypt_len;
            OS_FREE(decrypt_buf);
            decrypt_buf = NULL;
            rt = tuya_ai_basic_pkt_read(out, out_len, out_frag);
            if (rt != OPRT_OK) {
                PR_ERR("read continue frag packet failed, rt:%d", rt);
                goto EXIT;
            }
        } else if (current_frag_flag == AI_PACKET_FRAG_ING) {
            memcpy(ai_basic_proto->recv_frag_mng.data + ai_basic_proto->recv_frag_mng.offset, decrypt_buf, decrypt_len);
            ai_basic_proto->recv_frag_mng.frag_flag = current_frag_flag;
            ai_basic_proto->recv_frag_mng.offset += decrypt_len;
            OS_FREE(decrypt_buf);
            decrypt_buf = NULL;
            rt = tuya_ai_basic_pkt_read(out, out_len, out_frag);
            if (rt != OPRT_OK) {
                PR_ERR("read continue ing frag packet failed, rt:%d", rt);
                goto EXIT;
            }
        } else if (current_frag_flag == AI_PACKET_FRAG_END) {
            memcpy(ai_basic_proto->recv_frag_mng.data + ai_basic_proto->recv_frag_mng.offset, decrypt_buf, decrypt_len);
            ai_basic_proto->recv_frag_mng.frag_flag = current_frag_flag;
            ai_basic_proto->recv_frag_mng.offset += decrypt_len;
            OS_FREE(decrypt_buf);
            decrypt_buf = NULL;
            *out = ai_basic_proto->recv_frag_mng.data;
            *out_len = ai_basic_proto->recv_frag_mng.offset;
            *out_frag = AI_PACKET_NO_FRAG;
        } else {
            *out = decrypt_buf;
            *out_len = decrypt_len;
            *out_frag = AI_PACKET_NO_FRAG;
        }
    } else {
        *out = decrypt_buf;
        *out_len = decrypt_len;
        *out_frag = head->frag_flag;
    }
    AI_PROTO_D("recv packet len:%d", *out_len);
    return rt;

EXIT:
    if (decrypt_buf) {
        OS_FREE(decrypt_buf);
        decrypt_buf = NULL;
    }
    if (ai_basic_proto->recv_frag_mng.data) {
        OS_FREE(ai_basic_proto->recv_frag_mng.data);
    }
    memset(&ai_basic_proto->recv_frag_mng, 0, SIZEOF(AI_RECV_FRAG_MNG_T));
    return recv_len;
}

VOID tuya_free_user_attrs(AI_ATTRIBUTE_T *attr)
{
    if (attr) {
        OS_FREE(attr);
    }
}

OPERATE_RET tuya_parse_user_attrs(char *in, uint32_t attr_len, AI_ATTRIBUTE_T **attr_out, uint32_t *attr_num)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    uint32_t idx = 0;

    uint32_t num = tuya_ai_get_attr_num(in, attr_len);
    AI_ATTRIBUTE_T *attr = OS_MALLOC(num * SIZEOF(AI_ATTRIBUTE_T));
    if (!attr) {
        PR_ERR("malloc attr failed");
        return OPRT_MALLOC_FAILED;
    }

    while (offset < attr_len) {
        memset(&attr[idx], 0, SIZEOF(AI_ATTRIBUTE_T));
        rt = tuya_ai_get_attr_value(in, &offset, &attr[idx]);
        if (OPRT_OK != rt) {
            PR_ERR("get attr value failed, rt:%d", rt);
            OS_FREE(attr);
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
                OS_FREE(ai_basic_proto->connection_id);
                ai_basic_proto->connection_id = NULL;
            }
            ai_basic_proto->connection_id = OS_MALLOC(attr[idx].length + 1);
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
    tuya_free_user_attrs(attr);
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
    tuya_free_user_attrs(attr);
    return rt;
}

OPERATE_RET tuya_ai_auth_resp(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    char *de_buf = NULL;
    uint32_t de_len = 0;
    AI_FRAG_FLAG frag = AI_PACKET_NO_FRAG;

    rt = tuya_ai_basic_pkt_read(&de_buf, &de_len, &frag);
    if (OPRT_OK != rt) {
        PR_ERR("recv auth resp failed, rt:%d", rt);
        return rt;
    } else if (NULL == de_buf) {
        PR_ERR("recv auth resp buf is null");
        return OPRT_COM_ERROR;
    }

    AI_PAYLOAD_HEAD_T *packet = (AI_PAYLOAD_HEAD_T *)de_buf;
    if (packet->attribute_flag != AI_HAS_ATTR) {
        PR_ERR("auth resp packet has no attribute");
        OS_FREE(de_buf);
        return OPRT_COM_ERROR;
    }

    uint32_t attr_len = 0;
    memcpy(&attr_len, de_buf + SIZEOF(AI_PAYLOAD_HEAD_T), SIZEOF(attr_len));
    attr_len = UNI_NTOHL(attr_len);
    uint32_t offset = SIZEOF(AI_PAYLOAD_HEAD_T) + SIZEOF(attr_len);
    if (packet->type == AI_PT_AUTH_RESP) {
        rt = __ai_parse_auth_resp(de_buf + offset, attr_len);
    } else if (packet->type == AI_PT_CONN_CLOSE) {
        rt = tuya_ai_parse_conn_close(de_buf + offset, attr_len);
    } else {
        PR_ERR("auth resp packet type error %d", packet->type);
        rt = OPRT_COM_ERROR;
    }
    OS_FREE(de_buf);
    return rt;
}

STATIC OPERATE_RET __create_ping_attrs(AI_SEND_PACKET_T *pkt)
{
    uint32_t attr_idx = 0;
    uint64_t ts = tal_time_get_posix_ms();
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_CLIENT_TS, ATTR_PT_U64, &ts, SIZEOF(uint64_t));
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

OPERATE_RET tuya_ai_basic_ping(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_PING;
    rt = __create_ping_attrs(&pkt);
    if (OPRT_OK != rt) {
        return rt;
    }
    PR_NOTICE("ai ping");
    rt = tuya_ai_basic_pkt_send(&pkt);
    return rt;
}

OPERATE_RET tuya_ai_basic_connect(AI_SERVER_CFG_INFO_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t idx = 0;
    NW_IP_S ip;

    memset(&ip, 0, sizeof(NW_IP_S));
    netmgr_linkage_t *linkage = NULL;
    mqc_get_connection_linkage(&linkage);
    if (!linkage || linkage->get(LINKAGE_CFG_IP, &ip) != OPRT_OK) {
        PR_ERR("ai get ip failed %d", linkage ? linkage->type : 255);
        return OPRT_COM_ERROR;
    }

    struct socket_config_t sock_config = {0};
    sock_config.isReuse = TRUE;
    sock_config.isDisableNagle = TRUE;
    sock_config.isKeepAlive = TRUE;
    sock_config.keepAliveIdleTime = 100;
    sock_config.keepAliveInterval = 5;
    sock_config.keepAliveCount = 1;
    sock_config.isBlock = TRUE;
    sock_config.bindPort = 50000 + uni_random();
    sock_config.sendTimeoutMs = (AI_SEND_PKT_TIMEOUT * 1000);
#if defined(ENABLE_IPv6) && (ENABLE_IPv6 == 1)
    sock_config.bindAddr = tal_net_str2addr((CONST char *)ip.nwipstr);
#else
    sock_config.bindAddr = tal_net_str2addr((CONST char *)ip.ip);
#endif

    ai_basic_proto->transporter = tuya_transporter_create(TRANSPORT_TYPE_TCP, NULL);
    if (!ai_basic_proto->transporter) {
        PR_ERR("create transporter err");
        return OPRT_COM_ERROR;
    }

    tuya_transporter_ctrl(ai_basic_proto->transporter, TUYA_TRANSPORTER_SET_TCP_CONFIG, &sock_config);
    for (idx = 0; idx < cfg->host_num; idx++) {
        PR_NOTICE("connect to host :%s, port: %d", cfg->hosts[idx], cfg->tcp_port);
        rt = tuya_transporter_connect(ai_basic_proto->transporter, cfg->hosts[idx], cfg->tcp_port, AI_SEND_SOCKET_TIMEOUT);
        if (OPRT_OK == rt) {
            ai_basic_proto->connected = TRUE;
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

    char *usr_buf = OS_MALLOC(attr_len);
    TUYA_CHECK_NULL_RETURN(usr_buf, OPRT_MALLOC_FAILED);
    memset(usr_buf, 0, attr_len);

    for (idx = 0; idx < attr_num; idx++) {
        AI_ATTR_TYPE type = UNI_HTONS(attr[idx].type);
        memcpy(usr_buf + offset, &type, SIZEOF(AI_ATTR_TYPE));
        offset += SIZEOF(AI_ATTR_TYPE);
        AI_ATTR_PT payload_type = attr[idx].payload_type;
        memcpy(usr_buf + offset, &payload_type, SIZEOF(AI_ATTR_PT));
        offset += SIZEOF(AI_ATTR_PT);
        uint32_t attr_idx_len = attr[idx].length;
        uint32_t length = UNI_HTONL(attr_idx_len);
        memcpy(usr_buf + offset, &length, SIZEOF(uint32_t));
        offset += SIZEOF(uint32_t);
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
            OS_FREE(usr_buf);
            return OPRT_COM_ERROR;
        }
        offset += attr_idx_len;
    }

    *out_len = attr_len;
    *out = (uint8_t *)usr_buf;

    return rt;
}

STATIC OPERATE_RET __create_seesion_new_attrs(AI_SEND_PACKET_T *pkt, AI_SESSION_NEW_ATTR_T *session)
{
    uint32_t attr_idx = 0;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SESSION_ID, ATTR_PT_STR, session->id, strlen(session->id));
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_BIZ_CODE, ATTR_PT_U32, &session->biz_code, SIZEOF(uint32_t));
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_BIZ_TAG, ATTR_PT_U64, &session->biz_tag, SIZEOF(uint64_t));
    if (session->token) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_AGENT_TOKEN, ATTR_PT_STR, session->token, strlen(session->token));
    }
    if (session->user_data) {
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_BYTES, session->user_data, session->user_len);
#else
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_STR, session->user_data, session->user_len);
#endif
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __create_seesion_close_attrs(AI_SEND_PACKET_T *pkt, char *session_id, AI_STATUS_CODE code)
{
    uint32_t attr_idx = 0;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SESSION_ID, ATTR_PT_STR, session_id, strlen(session_id));
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SESSION_CLOSE_ERR_CODE, ATTR_PT_U16, &code, SIZEOF(uint16_t));
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __create_video_attrs(AI_SEND_PACKET_T *pkt, AI_VIDEO_ATTR_T *video)
{
    uint32_t attr_idx = 0;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_VIDEO_CODEC_TYPE, ATTR_PT_U16, &video->base.codec_type, SIZEOF(uint16_t));
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_VIDEO_SAMPLE_RATE, ATTR_PT_U32, &video->base.sample_rate, SIZEOF(uint32_t));
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_VIDEO_WIDTH, ATTR_PT_U16, &video->base.width, SIZEOF(uint16_t));
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_VIDEO_HEIGHT, ATTR_PT_U16, &video->base.height, SIZEOF(uint16_t));
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_VIDEO_FPS, ATTR_PT_U16, &video->base.fps, SIZEOF(uint16_t));
#else
    uint8_t video_params[64];
    memset(video_params, 0, sizeof(video_params));
    char video_params_len = snprintf((char *)video_params, sizeof(video_params), "%d %d %d %d %d", video->base.codec_type, video->base.width, video->base.height, video->base.fps, video->base.sample_rate);
    // AI_ATTR_AUDIO_PARAMS
    if (video_params_len > 0) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_VIDEO_PARAMS, ATTR_PT_STR, video_params, video_params_len);
        AI_PROTO_D("video params : %s ", video_params);
    } else {
        PR_ERR("compose video params err");
    }
#endif
    if (video->option.user_data) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_BYTES, video->option.user_data, video->option.user_len);
    }
    if (video->option.session_id_list) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SESSION_ID_LIST, ATTR_PT_STR, video->option.session_id_list, strlen(video->option.session_id_list));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __create_audio_attrs(AI_SEND_PACKET_T *pkt, AI_AUDIO_ATTR_T *audio)
{
    uint32_t attr_idx = 0;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_AUDIO_CODEC_TYPE, ATTR_PT_U16, &audio->base.codec_type, SIZEOF(uint16_t));
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_AUDIO_SAMPLE_RATE, ATTR_PT_U32, &audio->base.sample_rate, SIZEOF(uint32_t));
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_AUDIO_CHANNELS, ATTR_PT_U16, &audio->base.channels, SIZEOF(uint16_t));
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_AUDIO_DEPTH, ATTR_PT_U16, &audio->base.bit_depth, SIZEOF(uint16_t));
    if (audio->base.codec_type == AUDIO_CODEC_SPEEX) {
        if (audio->base.frame_size > 0) {
            pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_AUDIO_FRAME_SIZE, ATTR_PT_U16, &audio->base.frame_size, SIZEOF(uint16_t));
        } else {
            uint16_t speex_frame_size = 42; // default frame size @ wide band, quality 5, 20ms, mono
            pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_AUDIO_FRAME_SIZE, ATTR_PT_U16, &speex_frame_size, SIZEOF(uint16_t));
        }
    }
    if (audio->option.user_data) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_BYTES, audio->option.user_data, audio->option.user_len);
    }
    if (audio->option.session_id_list) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SESSION_ID_LIST, ATTR_PT_STR, audio->option.session_id_list, strlen(audio->option.session_id_list));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
#else
    uint8_t audio_params[64];
    memset(audio_params, 0, sizeof(audio_params));
    char audio_params_len = snprintf((char *)audio_params, sizeof(audio_params), "%d %d %d %d", audio->base.codec_type, audio->base.channels, audio->base.bit_depth, audio->base.sample_rate);
    // AI_ATTR_AUDIO_PARAMS
    if (audio_params_len > 0) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_AUDIO_PARAMS, ATTR_PT_STR, audio_params, audio_params_len);
        AI_PROTO_D("audio params : %s ", audio_params);
    } else {
        PR_ERR("compose audio params err");
    }

    if (audio->option.user_data) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_STR, audio->option.user_data, audio->option.user_len);
    }
    if (audio->option.session_id_list) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SESSION_ID_LIST, ATTR_PT_STR, audio->option.session_id_list, strlen(audio->option.session_id_list));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
#endif

    return OPRT_OK;
}

STATIC OPERATE_RET __create_image_attrs(AI_SEND_PACKET_T *pkt, AI_IMAGE_ATTR_T *image)
{
    uint32_t attr_idx = 0;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_IMAGE_FORMAT, ATTR_PT_U8, &image->base.format, SIZEOF(uint8_t));
    if (image->base.width > 0) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_IMAGE_WIDTH, ATTR_PT_U16, &image->base.width, SIZEOF(uint16_t));
    }
    if (image->base.height > 0) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_IMAGE_HEIGHT, ATTR_PT_U16, &image->base.height, SIZEOF(uint16_t));
    }
#else
    uint8_t image_params[64];
    memset(image_params, 0, sizeof(image_params));
    char image_params_len = snprintf((char *)image_params, sizeof(image_params), "%d %d %d %d", image->base.type, image->base.format, image->base.width, image->base.height);
    // AI_ATTR_AUDIO_PARAMS
    if (image_params_len > 0) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_IMAGE_PARAMS, ATTR_PT_STR, image_params, image_params_len);
        AI_PROTO_D("image params : %s ", image_params);
    } else {
        PR_ERR("compose image params err");
    }
#endif
    if (image->option.user_data) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_BYTES, image->option.user_data, image->option.user_len);
    }
    if (image->option.session_id_list) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SESSION_ID_LIST, ATTR_PT_STR, image->option.session_id_list, strlen(image->option.session_id_list));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __create_file_attrs(AI_SEND_PACKET_T *pkt, AI_FILE_ATTR_T *file)
{
    uint32_t attr_idx = 0;
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_FILE_FORMAT, ATTR_PT_U8, &file->base.format, SIZEOF(uint8_t));
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_FILE_NAME, ATTR_PT_STR, &file->base.file_name, strlen(file->base.file_name));
#else
    uint8_t file_params[512] = {0};
    memset(file_params, 0, sizeof(file_params));
    char file_params_len = snprintf((char *)file_params, sizeof(file_params), "%d %d %s", file->base.type, file->base.format, file->base.file_name);
    // AI_ATTR_AUDIO_PARAMS
    if (file_params_len > 0) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_FILE_PARAMS, ATTR_PT_STR, file_params, file_params_len);
        AI_PROTO_D("file params : %s ", file_params);
    } else {
        PR_ERR("compose file params err");
    }
#endif
    if (file->option.user_data) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_BYTES, file->option.user_data, file->option.user_len);
    }
    if (file->option.session_id_list) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SESSION_ID_LIST, ATTR_PT_STR, file->option.session_id_list, strlen(file->option.session_id_list));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __create_text_attrs(AI_SEND_PACKET_T *pkt, AI_TEXT_ATTR_T *text)
{
    uint32_t attr_idx = 0;
    if (text->session_id_list) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SESSION_ID_LIST, ATTR_PT_STR, text->session_id_list, strlen(text->session_id_list));
    }
    pkt->count = attr_idx;
    if (!__ai_check_attr_created(pkt)) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

STATIC OPERATE_RET __create_event_attrs(AI_SEND_PACKET_T *pkt, AI_EVENT_ATTR_T *event, AI_EVENT_TYPE type)
{
    uint32_t attr_idx = 0;
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SESSION_ID, ATTR_PT_STR, event->session_id, strlen(event->session_id));
    pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_EVENT_ID, ATTR_PT_STR, event->event_id, strlen(event->event_id));
    if (event->cmd_data) {
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_CMD_DATA, ATTR_PT_STR, event->cmd_data, strlen(event->cmd_data));
    }
    if (type == AI_EVENT_END) {
        uint64_t ts = tal_time_get_posix_ms();
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_EVENT_TS, ATTR_PT_U64, &ts, SIZEOF(uint64_t));
    }
    if (event->user_data) {
#if defined(AI_VERSION) && (0x01 == AI_VERSION)
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_BYTES, event->user_data, event->user_len);
#else
        pkt->attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_STR, event->user_data, event->user_len);
#endif
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

OPERATE_RET tuya_ai_basic_video(AI_VIDEO_ATTR_T *video, char *data, uint32_t len, uint32_t total_len, AI_PACKET_WRITER_T *writer)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {.writer = writer};
    pkt.type = AI_PT_VIDEO;
    if (video) {
        rt = __create_video_attrs(&pkt, video);
        if (OPRT_OK != rt) {
            return rt;
        }
    }
    pkt.total_len = total_len;
    pkt.len = len;
    pkt.data = data;
    AI_PROTO_D("send video");
    if (pkt.len == pkt.total_len) {
        return tuya_ai_basic_pkt_send(&pkt);
    } else {
        return tuya_ai_basic_pkt_frag_send(&pkt);
    }
}

OPERATE_RET tuya_ai_basic_audio(AI_AUDIO_ATTR_T *audio, char *data, uint32_t len, uint32_t total_len, AI_PACKET_WRITER_T *writer)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {.writer = writer};
    pkt.type = AI_PT_AUDIO;
    if (audio) {
        rt = __create_audio_attrs(&pkt, audio);
        if (OPRT_OK != rt) {
            return rt;
        }
    }
    pkt.total_len = total_len;
    pkt.len = len;
    pkt.data = data;
    AI_PROTO_D("send audio");
    if (pkt.len == pkt.total_len) {
        return tuya_ai_basic_pkt_send(&pkt);
    } else {
        return tuya_ai_basic_pkt_frag_send(&pkt);
    }
}

OPERATE_RET tuya_ai_basic_image(AI_IMAGE_ATTR_T *image, char *data, uint32_t len, uint32_t total_len, AI_PACKET_WRITER_T *writer)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {.writer = writer};
    pkt.type = AI_PT_IMAGE;
    if (image) {
        rt = __create_image_attrs(&pkt, image);
        if (OPRT_OK != rt) {
            return rt;
        }
    }
    pkt.total_len = total_len;
    pkt.len = len;
    pkt.data = data;
    AI_PROTO_D("send image");
    if (pkt.len == pkt.total_len) {
        return tuya_ai_basic_pkt_send(&pkt);
    } else {
        return tuya_ai_basic_pkt_frag_send(&pkt);
    }
}

OPERATE_RET tuya_ai_basic_file(AI_FILE_ATTR_T *file, char *data, uint32_t len, uint32_t total_len, AI_PACKET_WRITER_T *writer)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {.writer = writer};
    pkt.type = AI_PT_FILE;
    if (file) {
        rt = __create_file_attrs(&pkt, file);
        if (OPRT_OK != rt) {
            return rt;
        }
    }
    pkt.total_len = total_len;
    pkt.len = len;
    pkt.data = data;
    AI_PROTO_D("send file");
    if (pkt.len == pkt.total_len) {
        return tuya_ai_basic_pkt_send(&pkt);
    } else {
        return tuya_ai_basic_pkt_frag_send(&pkt);
    }
}

OPERATE_RET tuya_ai_basic_text(AI_TEXT_ATTR_T *text, char *data, uint32_t len, uint32_t total_len, AI_PACKET_WRITER_T *writer)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {.writer = writer};
    pkt.type = AI_PT_TEXT;
    if (text) {
        rt = __create_text_attrs(&pkt, text);
        if (OPRT_OK != rt) {
            return rt;
        }
    }
    pkt.total_len = total_len;
    pkt.len = len;
    pkt.data = data;
    AI_PROTO_D("send text");
    if (pkt.len == pkt.total_len) {
        return tuya_ai_basic_pkt_send(&pkt);
    } else {
        return tuya_ai_basic_pkt_frag_send(&pkt);
    }
}

OPERATE_RET tuya_ai_basic_event(AI_EVENT_ATTR_T *event, char *data, uint32_t len, AI_PACKET_WRITER_T *writer)
{
    OPERATE_RET rt = OPRT_OK;
    AI_SEND_PACKET_T pkt = {.writer = writer};
    pkt.type = AI_PT_EVENT;

    AI_EVENT_HEAD_T head = {0};
    memcpy(&head, data, SIZEOF(AI_EVENT_HEAD_T));
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

uint32_t tuya_ai_basic_get_var_seq(AI_PACKET_PT type, char *buffer)
{
    uint32_t count = 0, num = 0;
    if (type == AI_PT_FILE) {
        num = ai_basic_proto->file_seq;
    } else if (type == AI_PT_TEXT) {
        num = ai_basic_proto->text_seq;
    }
    AI_PROTO_D("var seq %d", num);
    if (num == 0) {
        buffer[0] = 0;
        return 1;
    }
    while (num > 0) {
        buffer[count] = num & 0x7F;
        num >>= 7;
        if (num > 0) {
            buffer[count] |= 0x80;
        }
        count++;
    }
    return count;
}

VOID tuya_ai_basic_update_var_seq(AI_PACKET_PT type)
{
    if (type == AI_PT_FILE) {
        ai_basic_proto->file_seq++;
    } else if (type == AI_PT_TEXT) {
        ai_basic_proto->text_seq++;
    }
}

//such as f47ac10b-58cc-42d5-0136-4067a8e7d6b3
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
    snprintf(uuid_str, AI_UUID_V4_LEN, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x",
             uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
             uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
    return OPRT_OK;
}

//such as f47ac10b-58cc-42d5-0136
OPERATE_RET tuya_ai_basic_uuid_short(char *uuid_str)
{
    if (NULL == uuid_str) {
        PR_ERR("uuid_str is NULL");
        return OPRT_INVALID_PARM;
    }

    memset(uuid_str, 0, AI_UUID_SHORT_LEN);
    uint8_t uuid[10] = {0};
    uni_random_bytes(uuid, 10);
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    uuid[8] = (uuid[8] & 0x3F) | 0x80;
    snprintf(uuid_str, AI_UUID_SHORT_LEN, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x",
             uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
             uuid[8], uuid[9]);
    return OPRT_OK;
}

STATIC OPERATE_RET __default_write(AI_PACKET_WRITER_T *writer, VOID *buf, uint32_t buf_len)
{
    OPERATE_RET rt = OPRT_OK;
    tuya_transporter_t transporter = (tuya_transporter_t)writer->user_data;
    if ((!transporter) || (!buf) || (buf_len == 0)) {
        PR_ERR("invalid parameter, transporter:%p, buf:%p, buf_len:%d", transporter, buf, buf_len);
        return OPRT_INVALID_PARM;
    }
    uint32_t bytes_sent = 0;
    uint8_t *current_buf_ptr = (uint8_t *)buf;
    uint32_t remaining_len = buf_len;

    while (remaining_len > 0) {
        rt = tuya_transporter_write(transporter, current_buf_ptr, remaining_len, 0);
        if (rt > 0) {
            bytes_sent += rt;
            current_buf_ptr += rt;
            remaining_len -= rt;
            if (remaining_len > 0) {
                PR_DEBUG("partial send, sent:%d, total_sent:%d, remaining:%d, err:%d", rt, bytes_sent, remaining_len, tal_net_get_errno());
                tal_system_sleep(100);
            }
        } else {
            PR_ERR("send to cloud failed, rt:%d, len:%d, err:%d", rt, remaining_len, tal_net_get_errno());
            return OPRT_COM_ERROR;
        }
    }
    PR_DEBUG("send success, total bytes sent: %d, sequence %d", bytes_sent, ai_basic_proto->sequence_out);
    return OPRT_OK;
}