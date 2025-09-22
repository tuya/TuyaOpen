/**
 * @file tuya_tls.h
 * @brief Header file for Tuya TLS operations.
 *
 * This file defines the structures, enums, and callback function types used for
 * managing TLS (Transport Layer Security) operations within the Tuya IoT SDK.
 * It includes definitions for initializing TLS sessions, handling TLS handshake
 * and application data phases, and performing data send/receive operations over
 * TLS-secured connections. The file is part of Tuya's efforts to ensure secure
 * communication between IoT devices and the Tuya cloud platform.
 *
 * Note: mbedtls is only used for encrypting the session, not for creating the
 * session.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef TUYA_TLS_H
#define TUYA_TLS_H

// mbedtls only used to encryption the seesion,not used to create the seesion
#include "tuya_cloud_types.h"
// #include "ssl.h"
// #include "tuya_cert_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *tuya_tls_hander;

typedef enum {
    TSS_INIT = 0,
    TSS_START,
    TSS_ACCEPT,
    TSS_TLS_HAND,
    TSS_TLS_APP,
} TLS_TCP_STAT_E;

typedef void (*tuya_tls_pre_conn_cb)(const char *hostname, const tuya_tls_hander p_tls_hander);
typedef int (*tuya_tls_send_cb)(void *p_custom_net_ctx, const uint8_t *buf, size_t len);
typedef int (*tuya_tls_recv_cb)(void *p_custom_net_ctx, uint8_t *buf, size_t len);

typedef enum {
    TUYA_TLS_PSK_MODE,
    TUYA_TLS_SERVER_CERT_MODE,
    TUYA_TLS_MUTUAL_CERT_MODE,
    TUYA_TLS_HARDWARE_CERT_MODE,
    // TUYA_TLS_AWS_FFS_CERT_MODE,
} tuya_tls_mode_t;

typedef enum {
    TUYA_TLS_CERT_EXPIRED,
} tuya_tls_event_t;
/**
 * @brief tls event cb
 *
 * @param[in] event event id
 * @param[in] p_args cb args
 *
 */
typedef void (*tuya_tls_event_cb)(tuya_tls_event_t event, void *p_args);

typedef struct {
    tuya_tls_mode_t mode;
    char *hostname;
    uint16_t port;
    uint32_t timeout;

    char *psk_key;
    uint32_t psk_key_size;
    char *psk_id;
    int psk_id_size;

    bool verify;
    char *ca_cert;
    int ca_cert_size;

    char *client_cert;
    int client_cert_size;
    char *client_pkey;
    int client_pkey_size;

    size_t in_content_len;
    size_t out_content_len;

    tuya_tls_send_cb f_send;
    tuya_tls_recv_cb f_recv;
    tuya_tls_event_cb exception_cb;
    void *user_data;
} tuya_tls_config_t;

/**
 * @brief Get mbedtls random data in the specified length
 *
 * @param output
 * @param output_len
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_tls_random(unsigned char *output, size_t output_len);

/**
 * @brief tls register x509 ca
 *
 * @param[in] p_ctx ca content
 * @param[in] p_der ca
 * @param[in] der_len ca len
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_tls_register_x509_crt_der(void *p_ctx, uint8_t *p_der, uint32_t der_len);

/**
 * @brief register cb invoked before tls handshake
 *
 * @param[in] pre_conn callback
 */
void tuya_tls_register_pre_conn_cb(tuya_tls_pre_conn_cb pre_conn);

/**
 * @brief tls init
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_tls_init();

/**
 * @brief tls hander create
 *
 * @return tuya_tls_hander*
 */
tuya_tls_hander *tuya_tls_connect_create(void);

/**
 * @brief
 *
 * @param[in/out] p_tls_hander
 */
void tuya_tls_connect_destroy(tuya_tls_hander p_tls_hander);

/**
 * @brief
 *
 * @param[in/out] p_tls_handler
 * @param[in/out] config
 * @return OPERATE_RET
 */
OPERATE_RET tuya_tls_config_set(tuya_tls_hander p_tls_handler, tuya_tls_config_t *config);

/**
 * @brief
 *
 * @param[in/out] p_tls_handler
 * @return tuya_tls_config_t*
 */
tuya_tls_config_t *tuya_tls_config_get(tuya_tls_hander p_tls_handler);

/**
 * @brief tls connect
 *
 * @param[in] p_tls_handler refer to tuya_tls_hander
 * @param[in] hostname url
 * @param[in] port_num port
 * @param[in] socket_fd fd
 * @param[in] overtime_s connect timeout
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_tls_connect(tuya_tls_hander p_tls_handler, char *hostname, int port_num, int socket_fd,
                             int overtime_s);

/**
 * @brief tls write
 *
 * @param[in] tls_handler refer to tuya_tls_hander
 * @param[in] buf write data
 * @param[in] len write length
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_tls_write(tuya_tls_hander tls_handler, uint8_t *buf, uint32_t len);

/**
 * @brief tls read
 *
 * @param[in] tls_handler refer to tuya_tls_hander
 * @param[out] buf read data
 * @param[in] len read length
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_tls_read(tuya_tls_hander tls_handler, uint8_t *buf, uint32_t len);

/**
 * @brief generated random
 *
 * @param[in] tls_handler refer to tuya_tls_hander
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_tls_disconnect(tuya_tls_hander tls_handler);

/**
 * @brief Retrieves the configuration for the Tuya TLS PSK mode.
 *
 * This function returns a pointer to the `tuya_tls_config_t` structure that
 * contains the configuration for the Tuya TLS PSK mode. The configuration
 * includes parameters such as the PSK (Pre-Shared Key), cipher suites, and
 * other TLS settings.
 *
 * @return A pointer to the `tuya_tls_config_t` structure containing the Tuya
 * TLS PSK mode configuration.
 */
const tuya_tls_config_t *tuya_tls_psk_mode_config_get(void);

/**
 * Retrieves the callback function for Tuya TLS events.
 *
 * This function returns the callback function that is registered to handle Tuya
 * TLS events.
 *
 * @return The callback function for Tuya TLS events.
 */
tuya_tls_event_cb tuya_cert_get_tls_event_cb(void);

#ifdef __cplusplus
}

#endif
#endif
