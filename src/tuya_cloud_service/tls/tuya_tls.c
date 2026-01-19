/**
 * @file tuya_tls.c
 * @brief This file provides the implementation of Tuya's TLS functions,
 * including initialization and cleanup of TLS context, mutex operations for
 * thread safety, memory management, random number generation, and handling of
 * TLS events. It also includes the implementation of sending and receiving data
 * over TLS secured connections, handling of X.509 certificates for TLS, and
 * logging for TLS operations.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_tls.h"

#if !defined(MBEDTLS_CONFIG_FILE)
#include "tuya_tls_config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
#endif
#include <string.h>
#include "tal_api.h"
#include "tal_kv.h"
#include "tal_network.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/oid.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/aes.h"

#define TLS_URL_LEN (128 + 16)

typedef struct {
    tuya_tls_config_t config;
    mbedtls_ssl_context ssl_ctx;
    mbedtls_ssl_config conf_ctx;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt client_cert;
    mbedtls_pk_context client_pkey;
    int socket_fd;
    int overtime_s;
    MUTEX_HANDLE mutex;
    MUTEX_HANDLE read_mutex;
} tuya_mbedtls_context_t;

#define TLS_HANDSHAKE_TIMEOUT (18) // s

static tuya_tls_pre_conn_cb s_pre_conn_cb = NULL;
static mbedtls_entropy_context ty_entropy;
static mbedtls_ctr_drbg_context ty_ctr_drbg;

/* -------------------------------------------------------------------------- */
/*                                  TLS Mutex                                 */
/* -------------------------------------------------------------------------- */

static void __tuya_tls_event_cb(tuya_tls_event_t event, void *p_args)
{
    const char *p_url = (char *)p_args;
    if (NULL == p_url) {
        PR_ERR("url was NULL");
        return;
    }
    if (event == TUYA_TLS_CERT_EXPIRED) {
        PR_DEBUG("tls cert expired");
        // tuya_iotdns_query_domain_certs(BAIDU_TOKEN_URL, &cacert,
        // &cacert_len);
        // TODO..
        return;
    }
}

static void __tuya_tls_mutex_init(mbedtls_threading_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }

    mutex->is_valid = tal_mutex_create_init(&mutex->mutex) == 0;
}

static void __tuya_tls_mutex_free(mbedtls_threading_mutex_t *mutex)
{
    if (mutex == NULL || !mutex->is_valid) {
        return;
    }
    (void)tal_mutex_release(mutex->mutex);
    mutex->is_valid = 0;
}

static int __tuya_tls_mutex_lock(mbedtls_threading_mutex_t *mutex)
{
    if (mutex == NULL || !mutex->is_valid) {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }

    if (tal_mutex_lock(mutex->mutex) != 0) {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }

    return 0;
}

static int __tuya_tls_mutex_unlock(mbedtls_threading_mutex_t *mutex)
{
    if (mutex == NULL || !mutex->is_valid) {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }

    if (tal_mutex_unlock(mutex->mutex) != 0) {
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/*                                   Calloc                                   */
/* -------------------------------------------------------------------------- */
static __attribute__((unused)) void *__tuya_tls_calloc(size_t nmemb, size_t size)
{
    size_t mem_size = nmemb * size;
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM)
    void *ptr = tal_psram_malloc(mem_size);
#else
    void *ptr = tal_malloc(mem_size);
#endif
    if (ptr != NULL) {
        memset(ptr, 0, mem_size);
    } else {
        PR_ERR("------- alloc failed,size:%d", size);
    }
    return ptr;
}

#if defined(ENABLE_MBEDTLS_DEBUG) && (ENABLE_MBEDTLS_DEBUG == 1)
static void __tuya_tls_export_keys(void *p_expkey, mbedtls_ssl_key_export_type type, const unsigned char *secret,
                                   size_t secret_len, const unsigned char client_random[32],
                                   const unsigned char server_random[32], mbedtls_tls_prf_types tls_prf_type)
{
    int i = 0;
    tal_log_print_raw("CLIENT_RANDOM ");
    for (i = 0; i < 32; i++) {
        tal_log_print_raw("%02X", client_random[i]);
    }
    tal_log_print_raw(" ");
    for (i = 0; i < 48; i++) {
        tal_log_print_raw("%02X", secret[i]);
    }
    tal_log_print_raw("\n");
}
#endif

/* -------------------------------------------------------------------------- */
/*                                 TLS Randowm                                */
/* -------------------------------------------------------------------------- */
int tuya_tls_random(unsigned char *output, size_t output_len)
{
    return mbedtls_ctr_drbg_random(&ty_ctr_drbg, output, output_len);
}

int __tuya_tls_random(void *p_rng, unsigned char *output, size_t output_len)
{
    (void)p_rng;

    return tuya_tls_random(output, output_len);
}

/* -------------------------------------------------------------------------- */
/*                                 TLS NV seed                                */
/* -------------------------------------------------------------------------- */
#define TY_RANDOM_SEED      "tuya_seed"
#define TY_RANDOM_HKDF_INFO "entropy nv seed"

/**
 * @brief The function pointers for reading from a seed file to
 * Non-Volatile storage (NV) in a platform-independent way
 *
 * @param buf  buffer to read
 * @param buf_len buffer length
 * @return On success, the number of bytes read is returned.
 * On error, OPRT_COM_ERROR is returned.
 */
int __tuya_tls_nv_seed_read(unsigned char *buf, size_t buf_len)
{
    int ret;
    size_t len;
    uint8_t *seed;

    // /* fetch seed */
    ret = tal_kv_get(TY_RANDOM_SEED, &seed, &len);
    if (OPRT_OK != ret) {
        tuya_tls_random(buf, buf_len);
        tal_kv_set(TY_RANDOM_SEED, buf, buf_len);
        return buf_len;
    }
    memcpy(buf, seed, buf_len);
    tal_kv_free(seed);

    return buf_len;
}

/**
 * @brief The function pointers for writing a seed file to
 * Non-Volatile storage (NV) in a platform-independent way
 *
 * @param buf buffer to write
 * @param buf_len buffer length
 * @return On success, the number of bytes written is returned.
 * On error, OPRT_COM_ERROR is returned.
 */
int __tuya_tls_nv_seed_write(unsigned char *buf, size_t buf_len)
{
    int ret;

    ret = tal_kv_set(TY_RANDOM_SEED, buf, buf_len);
    if (ret) {
        return OPRT_COM_ERROR;
    }

    return buf_len;
}

/**
 * @brief Registers an X.509 certificate in DER format.
 *
 * This function is used to register an X.509 certificate in DER format.
 *
 * @param p_ctx Pointer to the context.
 * @param p_der Pointer to the DER encoded certificate.
 * @param der_len Length of the DER encoded certificate.
 *
 * @return Returns an int32_t value indicating the success or failure of the
 * operation.
 */
int tuya_tls_register_x509_crt_der(void *p_ctx, uint8_t *p_der, uint32_t der_len)
{
    mbedtls_x509_crt *p_cert_ctx = (mbedtls_x509_crt *)p_ctx;
    return mbedtls_x509_crt_parse(p_cert_ctx, (const unsigned char *)p_der, der_len);
}

static void __tuya_tls_log(void *ctx, int level, const char *file, int line, const char *str)
{
    switch (level) {
    case 1: {
        PR_DEBUG_RAW("%s %d:%s\r\n", "", line, str);
        break;
    }
    case 2: {
        PR_DEBUG_RAW("%s %d:%s\r\n", "", line, str);
        break;
    }
    case 3:
    default: {
        PR_DEBUG_RAW("%s %d:%s\r\n", "", line, str);
        break;
    }
    }
}

static int __tuya_tls_socket_send_cb(void *ctx, const unsigned char *buf, size_t len)
{
    tuya_mbedtls_context_t *tls_context = (tuya_mbedtls_context_t *)ctx;

    int send_len = tal_net_send(tls_context->socket_fd, buf, len);
    if (send_len < 0) {
        PR_ERR("__tuya_tls_socket_send_cb errr %d %d", send_len, tal_net_get_errno());
        if ((tal_net_get_errno() == UNW_EINTR) || (tal_net_get_errno() == UNW_EAGAIN)) {
            tal_system_sleep(100);
            send_len = tal_net_send(tls_context->socket_fd, buf, len);
            if (send_len < 0) {
                PR_ERR("__tuya_tls_socket_send_cb errr %d %d", send_len, tal_net_get_errno());
            }
        }
    }

    return send_len;
}

static int __tuya_tls_socket_recv_cb(void *ctx, unsigned char *buf, size_t len)
{
    tuya_mbedtls_context_t *tls_context = (tuya_mbedtls_context_t *)ctx;

    int non_block = tal_net_get_nonblock(tls_context->socket_fd);
    if (non_block == 0) {
        tal_net_set_block(tls_context->socket_fd, FALSE);
    }

    TUYA_FD_SET_T readfds;
    memset(&readfds, 0, sizeof(TUYA_FD_SET_T));
    tal_net_fd_set(tls_context->socket_fd, &readfds);
    int activefds_cnt =
        tal_net_select(tls_context->socket_fd + 1, &readfds, NULL, NULL, tls_context->overtime_s * 1000);
    if (activefds_cnt <= 0) {
        tal_net_set_block(tls_context->socket_fd, 1 - non_block);
        PR_ERR("select fail.%d", activefds_cnt);
        return -100 + activefds_cnt;
    }

    int rv = tal_net_recv(tls_context->socket_fd, buf, len);
    tal_net_set_block(tls_context->socket_fd, 1 - non_block);

    return rv;
}

static int tuya_tls_ciphersuite_list_PSK[] = {MBEDTLS_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256, 0};

static void mbedtls_cert_pkey_free(tuya_tls_hander p_tls_handler)
{
    tuya_mbedtls_context_t *tls_context = (tuya_mbedtls_context_t *)p_tls_handler;
    tuya_tls_config_t *config = &tls_context->config;

    PR_DEBUG("mbedtls_cert_pkey_free.");

    if (config->ca_cert) {
        mbedtls_x509_crt_free(&tls_context->cacert);
    } else if (config->client_cert && config->client_pkey) {
        mbedtls_x509_crt_free(&tls_context->cacert);
        mbedtls_x509_crt_free(&tls_context->client_cert);
        mbedtls_pk_free(&tls_context->client_pkey);
    }
}

static OPERATE_RET mbedtls_cert_pkey_parse(tuya_tls_hander p_tls_handler)
{
    OPERATE_RET op_ret;
    tuya_mbedtls_context_t *tls_context = (tuya_mbedtls_context_t *)p_tls_handler;
    tuya_tls_config_t *config = &tls_context->config;

    if (config->verify) {
        PR_DEBUG("mbedtls authmode: MBEDTLS_SSL_VERIFY_REQUIRED");
        mbedtls_ssl_conf_authmode(&(tls_context->conf_ctx), MBEDTLS_SSL_VERIFY_REQUIRED);
    } else {

        PR_DEBUG("mbedtls authmode: MBEDTLS_SSL_VERIFY_NONE");
        mbedtls_ssl_conf_authmode(&(tls_context->conf_ctx), MBEDTLS_SSL_VERIFY_NONE);
    }

    mbedtls_x509_crt *p_cert_ctx = &(tls_context->cacert);
    mbedtls_x509_crt_init(p_cert_ctx);

    // parse ca cert
    if (config->ca_cert) {
        PR_DEBUG("load root ca cert.");
        op_ret = mbedtls_x509_crt_parse(p_cert_ctx, (const unsigned char *)config->ca_cert, config->ca_cert_size);
        if (op_ret != OPRT_OK) {
            PR_ERR("mbedtls_x509_crt_parse Fail. 0x%x %d", -op_ret, op_ret);
            return op_ret;
        }
        mbedtls_ssl_conf_ca_chain(&(tls_context->conf_ctx), p_cert_ctx, NULL);
    }

    /* parse client own cert */
    mbedtls_x509_crt *client_cert = NULL;
    mbedtls_pk_context *client_pkey = NULL;

    if (config->client_cert && config->client_pkey) {
        PR_DEBUG("Loading the client cert. and key...");
        client_cert = &(tls_context->client_cert);
        mbedtls_x509_crt_init(client_cert);
        op_ret = mbedtls_x509_crt_parse(client_cert, (const unsigned char *)tls_context->config.client_cert,
                                        tls_context->config.client_cert_size);
        if (op_ret != OPRT_OK) {
            PR_ERR("client cert parse fail. ret: 0x%x", -op_ret);
            return op_ret;
        }
        client_pkey = &(tls_context->client_pkey);
        mbedtls_pk_init(client_pkey);
        op_ret = mbedtls_pk_parse_key(client_pkey, (const unsigned char *)tls_context->config.client_pkey,
                                      tls_context->config.client_pkey_size, NULL, 0, NULL, 0);
        if (op_ret != 0) {
            PR_ERR("client pkey parse fail. ret: %d", op_ret);
            return op_ret;
        }

        op_ret = mbedtls_ssl_conf_own_cert(&(tls_context->conf_ctx), client_cert, client_pkey);
        if (op_ret != 0) {
            PR_ERR("set client cert && pkey fail ret: %d", op_ret);
            return op_ret;
        }
    }

    return OPRT_OK;
}

static int tuya_tls_ciphersuite_list[] = {MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
                                          MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
                                          MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0};

/**
 * @brief Initializes the Tuya TLS module.
 *
 * This function initializes the Tuya TLS module and prepares it for use.
 *
 * @return The result of the operation.
 *         - OPRT_OK: The operation was successful.
 *         - Other error codes: The operation failed.
 */
OPERATE_RET tuya_tls_init(void)
{
    OPERATE_RET op_ret = OPRT_OK;

    mbedtls_threading_set_alt(__tuya_tls_mutex_init, __tuya_tls_mutex_free, __tuya_tls_mutex_lock,
                              __tuya_tls_mutex_unlock);

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM)
    op_ret = mbedtls_platform_set_calloc_free(tal_psram_calloc, tal_psram_free);
#else 
    op_ret = mbedtls_platform_set_calloc_free(tal_calloc, tal_free);
#endif 
    if (op_ret != 0) {
        PR_ERR("mbedtls_platform_set_calloc_free Fail. %x", op_ret);
        return op_ret;
    }

    /* init entropy and seed random */
    mbedtls_ctr_drbg_init(&ty_ctr_drbg);
    mbedtls_entropy_init(&ty_entropy); // init and add entropy sources
    op_ret = mbedtls_ctr_drbg_seed(&ty_ctr_drbg, mbedtls_entropy_func, &ty_entropy, (const unsigned char *)"TUYA", 4);
    if (op_ret) {
        PR_ERR("mbedtls_ctr_drbg_seed fail. %d", op_ret);
        goto exit;
    }
    mbedtls_ctr_drbg_set_prediction_resistance(&ty_ctr_drbg, MBEDTLS_CTR_DRBG_PR_OFF);

    PR_NOTICE("tuya_tls_init ok!");

    return OPRT_OK;

exit:
    mbedtls_ctr_drbg_free(&ty_ctr_drbg);
    mbedtls_entropy_free(&ty_entropy);
    return op_ret;
}

/**
 * @brief register cb invoked before tls handshake
 *
 * @param[in] pre_conn callback
 */
void tuya_tls_register_pre_conn_cb(tuya_tls_pre_conn_cb pre_conn)
{
    s_pre_conn_cb = pre_conn;
}

tuya_tls_hander *tuya_tls_connect_create(void)
{
    OPERATE_RET ret = OPRT_OK;
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM)
    tuya_mbedtls_context_t *p_tls_conn = tal_psram_malloc(sizeof(tuya_mbedtls_context_t));
#else
    tuya_mbedtls_context_t *p_tls_conn = tal_malloc(sizeof(tuya_mbedtls_context_t));
#endif
    if (p_tls_conn == NULL) {
        PR_ERR("tuya_tls_connect_create Fail. %d", OPRT_MALLOC_FAILED);
        return NULL;
    }
    memset(p_tls_conn, 0, sizeof(tuya_mbedtls_context_t));

    ret = tal_mutex_create_init(&p_tls_conn->mutex);
    if (ret != OPRT_OK) {
        PR_ERR("mutex create Fail. %d", ret);
        goto __err_exit;
    }

    ret = tal_mutex_create_init(&p_tls_conn->read_mutex);
    if (ret != OPRT_OK) {
        PR_ERR("read_mutex create Fail. %d", ret);
        goto __err_exit;
    }

    return (tuya_tls_hander *)p_tls_conn;

__err_exit:
    if (p_tls_conn->mutex) {
        tal_mutex_release(p_tls_conn->mutex);
    }

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM)
    tal_psram_free(p_tls_conn);
#else
    tal_free(p_tls_conn);
#endif

    return NULL;
}

/**
 * @brief Destroys a TLS connection.
 *
 * This function is used to destroy a TLS connection identified by the given
 * handler.
 *
 * @param p_tls_hander The handler of the TLS connection to be destroyed.
 */
void tuya_tls_connect_destroy(tuya_tls_hander p_tls_hander)
{
    if (p_tls_hander == NULL) {
        return;
    }
    PR_DEBUG("tuya_tls_connect_destroy.");
    tuya_mbedtls_context_t *tls_context = (tuya_mbedtls_context_t *)p_tls_hander;
    tal_mutex_release(tls_context->mutex);
    tal_mutex_release(tls_context->read_mutex);
    
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM)
    tal_psram_free(p_tls_hander);
#else
    tal_free(p_tls_hander);
#endif
}

/**
 * @brief Sets the TLS configuration for the Tuya TLS handler.
 *
 * This function is used to set the TLS configuration for the Tuya TLS handler.
 *
 * @param p_tls_handler Pointer to the Tuya TLS handler.
 * @param config Pointer to the TLS configuration structure.
 * @return OPERATE_RET Returns OPRT_OK if the TLS configuration is set
 * successfully, or an error code if it fails.
 */
OPERATE_RET tuya_tls_config_set(tuya_tls_hander p_tls_handler, tuya_tls_config_t *config)
{
    tuya_mbedtls_context_t *tls_context = (tuya_mbedtls_context_t *)p_tls_handler;
    // tls_context->config = *config;
    memcpy(&tls_context->config, config, sizeof(tuya_tls_config_t));

    return OPRT_OK;
}

/**
 * @brief Retrieves the TLS configuration associated with the given TLS handler.
 *
 * @param p_tls_handler The TLS handler for which to retrieve the configuration.
 * @return A pointer to the TLS configuration structure.
 */
tuya_tls_config_t *tuya_tls_config_get(tuya_tls_hander p_tls_handler)
{
    tuya_mbedtls_context_t *tls_context = (tuya_mbedtls_context_t *)p_tls_handler;
    return &(tls_context->config);
}

/**
 * @brief Establishes a TLS connection with the specified hostname and port
 * number.
 *
 * This function is used to establish a TLS connection with the specified
 * hostname and port number.
 *
 * @param p_tls_handler Pointer to the TLS handler structure.
 * @param hostname The hostname of the server to connect to.
 * @param port_num The port number of the server to connect to.
 * @param socket_fd The file descriptor of the socket to use for the connection.
 * @param overtime_s The timeout value in seconds for the connection attempt.
 *
 * @return OPERATE_RET Returns OPRT_OK if the connection is established
 * successfully, or an error code if the connection fails.
 */
OPERATE_RET tuya_tls_connect(tuya_tls_hander p_tls_handler, char *hostname, int port_num, int socket_fd, int overtime_s)
{
    OPERATE_RET op_ret;
    tuya_mbedtls_context_t *tls_context = (tuya_mbedtls_context_t *)p_tls_handler;

    if (NULL == p_tls_handler || socket_fd < 0) {
        PR_ERR("INPUT INVALID PARM");
        return OPRT_INVALID_PARM;
    }

    tls_context->config.hostname = hostname;
    tls_context->config.port = port_num;
    tls_context->config.timeout = overtime_s;
    tls_context->config.exception_cb =
        tls_context->config.exception_cb == NULL ? __tuya_tls_event_cb : tls_context->config.exception_cb;

#if defined(TLS_MEM_DEBUG) && (TLS_MEM_DEBUG == 1) && (OPERATING_SYSTEM != SYSTEM_LINUX)
    PR_NOTICE("xPortGetFreeHeapSize=%d,xPortGetMinimumEverFreeHeapSize=%d\n", xPortGetFreeHeapSize(),
              xPortGetMinimumEverFreeHeapSize());
#endif
    PR_DEBUG("TUYA_TLS Begin Connect %s:%d", (hostname ? hostname : ""), port_num);

    mbedtls_ssl_context *p_ssl_ctx = &(tls_context->ssl_ctx);
    mbedtls_ssl_config *p_conf_ctx = &(tls_context->conf_ctx);

    mbedtls_ssl_init(p_ssl_ctx);
    mbedtls_ssl_config_init(p_conf_ctx);

    mbedtls_ssl_conf_dbg(p_conf_ctx, __tuya_tls_log, NULL);
    mbedtls_ssl_conf_rng(p_conf_ctx, __tuya_tls_random, NULL);

#if defined(ENABLE_MBEDTLS_DEBUG) && (ENABLE_MBEDTLS_DEBUG == 1)
    mbedtls_debug_set_threshold(3);
    mbedtls_ssl_set_export_keys_cb(p_ssl_ctx, __tuya_tls_export_keys, NULL);
#endif

    op_ret = mbedtls_ssl_config_defaults(p_conf_ctx, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                         MBEDTLS_SSL_PRESET_DEFAULT);
    if (op_ret != 0) {
        PR_ERR("mbedtls_ssl_config_defaults Fail. %x %d", op_ret, op_ret);
        goto tuya_tls_connect_EXIT;
    }

#if defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH)
#if (MBEDTLS_SSL_MAX_CONTENT_LEN >= 4096)
    mbedtls_ssl_conf_max_frag_len(p_conf_ctx, MBEDTLS_SSL_MAX_FRAG_LEN_4096);
#else
    mbedtls_ssl_conf_max_frag_len(p_conf_ctx, MBEDTLS_SSL_MAX_FRAG_LEN_1024);
#endif
#endif
    if (s_pre_conn_cb) {
        PR_DEBUG("s_pre_conn_cb  %08x", s_pre_conn_cb);
        s_pre_conn_cb(hostname, (tuya_tls_hander *)tls_context);
    }
    if (tls_context->config.psk_key_size > 0 && tls_context->config.psk_id_size > 0) {
        mbedtls_ssl_conf_psk(p_conf_ctx, (const unsigned char *)tls_context->config.psk_key,
                             tls_context->config.psk_key_size, (const unsigned char *)tls_context->config.psk_id,
                             tls_context->config.psk_id_size);
        mbedtls_ssl_conf_ciphersuites(p_conf_ctx, tuya_tls_ciphersuite_list_PSK);
    } else {
        op_ret = mbedtls_cert_pkey_parse(p_tls_handler);
        if (op_ret != 0) {
            PR_ERR("mbedtls_cert_parse_process Fail. 0x%x %d", -op_ret, op_ret);
            mbedtls_cert_pkey_free(p_tls_handler);
            return op_ret;
        }
        if (hostname) {
            op_ret = mbedtls_ssl_set_hostname(p_ssl_ctx, hostname);
            if (op_ret != 0) {
                PR_ERR("mbedtls_ssl_set_hostname Fail. 0x%x", -op_ret);
                mbedtls_cert_pkey_free(p_tls_handler);
                return op_ret;
            }
        }
        mbedtls_ssl_conf_ciphersuites(p_conf_ctx, tuya_tls_ciphersuite_list);
    }
    /* Setup */
    op_ret = mbedtls_ssl_setup(p_ssl_ctx, p_conf_ctx);
    if (op_ret != 0) {
        PR_ERR("mbedtls_ssl_setup Fail. 0x%x", op_ret);
        if (tls_context->config.mode != TUYA_TLS_PSK_MODE) {
            mbedtls_cert_pkey_free(p_tls_handler);
        }
        goto tuya_tls_connect_EXIT;
    }

    /* BIO default config */
    tls_context->socket_fd = socket_fd;
    tls_context->overtime_s = overtime_s;
    tal_net_set_timeout(tls_context->socket_fd, overtime_s * 1000, TRANS_SEND);
    mbedtls_ssl_set_bio(p_ssl_ctx, tls_context, __tuya_tls_socket_send_cb, __tuya_tls_socket_recv_cb, NULL);
    PR_DEBUG("socket fd is set. set to inner send/recv to handshake");

    TIME_T cur_time = tal_time_get_posix();

    while ((op_ret = mbedtls_ssl_handshake(p_ssl_ctx)) != 0) {
        if (op_ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
            PR_NOTICE("tls handshake :%d .require new certs.", op_ret);
            if (tls_context->config.exception_cb != NULL) {
                char whole_hostname[TLS_URL_LEN] = {0};
                if (port_num != 443) {
                    snprintf(whole_hostname, sizeof(whole_hostname), "%s:%d", hostname, port_num);
                } else {
                    snprintf(whole_hostname, sizeof(whole_hostname), "%s", hostname);
                }
                tls_context->config.exception_cb(TUYA_TLS_CERT_EXPIRED, whole_hostname);
                break;
            }
        }
        if ((op_ret != MBEDTLS_ERR_SSL_WANT_READ && op_ret != MBEDTLS_ERR_SSL_WANT_WRITE &&
             op_ret != (MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS)) ||
            (tal_time_get_posix() >= (cur_time + TLS_HANDSHAKE_TIMEOUT))) {
            PR_ERR("mbedtls_ssl_handshake returned 0x%x", -op_ret);
            break;
        }
    }

    if (tls_context->config.mode != TUYA_TLS_PSK_MODE) {
        mbedtls_cert_pkey_free(p_tls_handler);

        uint32_t handshake_flags = 0;
        /* In real life, we probably want to bail out when ret != 0 */
        if ((handshake_flags = mbedtls_ssl_get_verify_result(p_ssl_ctx)) != 0) {
            PR_ERR("mbedtls_ssl_get_verify_result failed, flag %d", handshake_flags);
            goto tuya_tls_connect_EXIT;
        }
    }

    if (op_ret != OPRT_OK) {
        goto tuya_tls_connect_EXIT;
    }

    PR_DEBUG("handshake finish for %s. set send/recv to user set", (hostname ? hostname : ""));
    if (tls_context->config.f_send && tls_context->config.f_recv) {
        mbedtls_ssl_set_bio(p_ssl_ctx, tls_context->config.user_data, tls_context->config.f_send,
                            tls_context->config.f_recv, NULL);
    }

    PR_DEBUG("TUYA_TLS Success Connect %s:%d Suit:%s", (hostname ? hostname : ""), port_num,
             mbedtls_ssl_get_ciphersuite(p_ssl_ctx));

    return OPRT_OK;

tuya_tls_connect_EXIT:

    PR_ERR("TUYA_TLS faild Connect %s:%d", (hostname ? hostname : ""), port_num);

    return op_ret;
}

/**
 * @brief Writes data to the TLS connection.
 *
 * This function writes the specified data to the TLS connection associated with
 * the given TLS handler.
 *
 * @param tls_handler The TLS handler.
 * @param buf The buffer containing the data to be written.
 * @param len The length of the data to be written.
 * @return The number of bytes written on success, or a negative error code on
 * failure.
 */
int tuya_tls_write(tuya_tls_hander tls_handler, uint8_t *buf, uint32_t len)
{
    if ((tls_handler == NULL) || (buf == NULL) || (len == 0)) {
        PR_ERR("Input Invalid");
        return OPRT_INVALID_PARM;
    }

    tuya_mbedtls_context_t *tls_context = (tuya_mbedtls_context_t *)tls_handler;
    int ret = -1;
    size_t written_len = 0;

    OPERATE_RET mu_ret = OPRT_OK;
    mu_ret = tal_mutex_lock(tls_context->mutex);
    if (OPRT_OK != mu_ret) {
        PR_ERR("tuya_hal_mutex_lock err %d", mu_ret);
        return mu_ret;
    }

    while (written_len < len) {
        ret = mbedtls_ssl_write(&(tls_context->ssl_ctx), (buf + written_len), (len - written_len));
        if (ret > 0) {
            written_len += ret;
            continue;
        }

        if ((ret == MBEDTLS_ERR_SSL_WANT_READ) || (ret == MBEDTLS_ERR_SSL_WANT_WRITE)) {
            continue;
        }

        // PR_ERR("mbedtls_ssl_write returned %d errno %d", ret,
        // tal_net_get_errno());
        mu_ret = tal_mutex_unlock(tls_context->mutex);
        if (OPRT_OK != mu_ret) {
            PR_ERR("tal_mutex_lock err %d", mu_ret);
            return mu_ret;
        }
        return ret;
    }

    mu_ret = tal_mutex_unlock(tls_context->mutex);
    if (OPRT_OK != mu_ret) {
        PR_ERR("tal_mutex_lock err %d", mu_ret);
        return mu_ret;
    }
    return written_len;
}

/**
 * @brief Reads data from the TLS connection.
 *
 * This function reads data from the TLS connection specified by the
 * `tls_handler` parameter.
 *
 * @param[in] tls_handler The TLS handler.
 * @param[out] buf The buffer to store the read data.
 * @param[in] len The length of the buffer.
 *
 * @return The number of bytes read on success, or a negative error code on
 * failure.
 */
int tuya_tls_read(tuya_tls_hander tls_handler, uint8_t *buf, uint32_t len)
{
    if ((tls_handler == NULL) || (buf == NULL) || (len == 0)) {
        PR_ERR("Input Invalid");
        return OPRT_INVALID_PARM;
    }

    tuya_mbedtls_context_t *tls_context = (tuya_mbedtls_context_t *)tls_handler;
    tal_mutex_lock(tls_context->read_mutex);
    int value = mbedtls_ssl_read(&(tls_context->ssl_ctx), buf, len);
    tal_mutex_unlock(tls_context->read_mutex);

    return value;
}

/**
 * @brief generated random
 *
 * @param[in] tls_handler refer to tuya_tls_hander
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_tls_disconnect(tuya_tls_hander tls_handler)
{
    OPERATE_RET mu_ret = OPRT_OK;

    PR_DEBUG("TUYA_TLS Disconnect ENTER");

    if (tls_handler == NULL) {
        PR_ERR("Input Invalid");
        return OPRT_INVALID_PARM;
    }

    tuya_mbedtls_context_t *tls_context = (tuya_mbedtls_context_t *)tls_handler;
    // wait for tls read quit,then free tls memory
    mu_ret = tal_mutex_lock(tls_context->read_mutex);
    if (OPRT_OK != mu_ret) {
        PR_ERR("read_mutex lock err %d", mu_ret);
    }

    mbedtls_ssl_context *p_ssl_ctx = &(tls_context->ssl_ctx);
    mbedtls_ssl_config *p_conf_ctx = &(tls_context->conf_ctx);

    mbedtls_ssl_free(p_ssl_ctx);
    mbedtls_ssl_config_free(p_conf_ctx);

    mu_ret = tal_mutex_unlock(tls_context->read_mutex);
    if (OPRT_OK != mu_ret) {
        PR_ERR("read_mutex unlock err %d", mu_ret);
    }

    PR_DEBUG("TUYA_TLS Disconnect Success");
    return OPRT_OK;
}

/**
 * Retrieves the callback function for Tuya TLS events.
 *
 * This function returns the callback function that is registered to handle Tuya
 * TLS events.
 *
 * @return The callback function for Tuya TLS events.
 */
tuya_tls_event_cb tuya_cert_get_tls_event_cb(void)
{
    return __tuya_tls_event_cb;
}