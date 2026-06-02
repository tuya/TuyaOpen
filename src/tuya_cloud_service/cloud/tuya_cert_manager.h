/**
 * @file tuya_cert_manager.h
 * @brief Tuya Certificate Manager
 *
 * This header file defines the structures, callbacks, and function prototypes
 * for managing TLS certificates, including client certificates, PSK
 * authentication, and third-party cloud CA certificates.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_CERT_MANAGER_H__
#define __TUYA_CERT_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tuya_tls.h"
#include "cJSON.h"

/**
 * Definition of client cert info
 */
typedef struct {
    uint8_t *cert;
    uint32_t cert_len;
    uint8_t *private_key;
    uint32_t private_key_len;
} client_cert_info_t;

/**
 * Definition of client psk info
 */
typedef struct {
    char   *psk_key;
    int32_t psk_key_size;
    char   *psk_id;
    int32_t psk_id_size;
} client_psk_info_t;

/**
 * @brief according url get third cloud ca
 *
 * @param[in] url Third cloud url
 *
 * @return
 */
void tuya_iot_get_third_cloud_ca(char *p_url);

/**
 * @brief Store third-party cloud CA certificate (local or remote)
 *
 * @param[in] p_url URL associated with the certificate
 * @param[in] ca_data CA certificate data (base64 encoded or raw DER format)
 * @param[in] ca_data_len Length of CA data
 * @param[in] need_base64_decode Whether base64 decoding is needed (TRUE for base64, FALSE for raw)
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_iot_store_third_cloud_ca(char *p_url, const uint8_t *ca_data, uint32_t ca_data_len,
                                          bool need_base64_decode);

/**
 * @brief root ca write
 *
 * @param[in] value CA value
 * @param[in] len CA len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_client_cert_write(const uint8_t *value, const uint32_t len);

/**
 * @brief client private key write
 *
 * @param[in] value private key value
 * @param[in] len private key len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_client_private_key_write(const uint8_t *value, const uint32_t len);

/**
 * @brief get client cert
 *
 * @return const client_cert_info_t*
 */
const client_cert_info_t *tuya_client_cert_get(void);

/**
 * @brief cert manager init
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_cert_manager_init(void);

/**
 * @brief cert manager deinit
 *
 */
void tuya_cert_manager_deinit(void);

/**
 * @brief cert get according one url
 * @param[out] result cert result
 * @param[in] url_msg url
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET httpc_domain_certs_get(cJSON **result, const char *url_msg);

/**
 * @brief cert parse cb
 *
 * @param[in] ctx contex
 * @param[in] cert ca cert
 * @param[in] cert_len ca cert len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET (*CERT_PARSE_CB)(void *ctx, uint8_t *cert, uint32_t cert_len);

/**
 * @brief cert load
 *
 * @param[in] url hostname
 * @param[in] cb cert parse cb
 * @param[in] p_ctx contex
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_cert_manager_load(char *url, CERT_PARSE_CB cb, void *p_ctx);

/**
 * @brief save cert to kv
 *
 * @param[in] result cert info
 *
 */
void tuya_cert_save_to_kv(cJSON *result);

/**
 * @brief cert resume from kv to ram
 *
 */
void tuya_cert_kv_to_ram(void);

/**
 * @brief get tls event cb
 *
 * @return event cb
 */
tuya_tls_event_cb tuya_cert_get_tls_event_cb(void);

/**
 * @brief get client psk
 *
 * @return const client_psk_info_t*
 */
const client_psk_info_t *tuya_client_psk_get(void);

/**
 * @brief Definition of domain's IP and cert structure
 */
typedef struct {
    /** Host name*/
    char *host;
    /** Host port */
    int32_t port;
    /** is certs required */
    bool need_ca;
    /** is ip required */
    bool need_ip;
} DNS_QUERY_S;

/**
 * @brief get root ca
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET http_iot_dns_get_root_ca(void);

/**
 * cert save cb
 *
 * @param[in] url url
 * @param[in] ca ca
 * @param[in] len len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 *
 */
typedef OPERATE_RET (*TUYA_CERT_SAVE_CB)(char *url, uint8_t *ca, uint32_t len);

/**
 * cert restore cb
 *
 * @param[in] p_ctx tls ctx
 * @param[in] url url
 *
 * @return TRUE/FALSE
 *
 */
typedef BOOL_T (*TUYA_CERT_RESTORE_CB)(void *p_ctx, char *url);

/**
 * reg save and restore cb
 *
 */
void tuya_cert_manager_reg(TUYA_CERT_SAVE_CB scb, TUYA_CERT_RESTORE_CB rcb);

tuya_tls_event_cb tuya_cert_get_tls_event_cb(void);

#ifdef __cplusplus
}
#endif

#endif //__TUYA_CERT_MANAGER_H__
