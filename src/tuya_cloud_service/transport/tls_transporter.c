/**
 * @file tls_transporter.c
 * @brief Implementation of TLS transporter for secure communication.
 *
 * This file contains the implementation of the TLS (Transport Layer Security)
 * transporter for Tuya devices, providing secure communication channels over
 * TCP/IP networks. It encapsulates the functionality for establishing TLS
 * connections, handling encryption and decryption of data, and managing TLS
 * sessions. The TLS transporter leverages underlying TCP transport mechanisms
 * while adding a layer of security through TLS protocols.
 *
 * The implementation includes functions for initializing and destroying TLS
 * transporter instances, sending and receiving data over TLS-secured
 * connections, and handling TLS-specific errors. It is designed to integrate
 * seamlessly with the Tuya IoT SDK, ensuring secure data exchange between Tuya
 * devices and the Tuya cloud platform.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_error_code.h"
#include "tal_log.h"
#include "tuya_transporter.h"
#include "tcp_transporter.h"
#include "tls_transporter.h"
#include "tal_network.h"
#include "tal_memory.h"
#include "tuya_tls.h"
#include "mbedtls/ssl.h"

typedef struct tls_transporter_inter_t {
    struct tuya_transporter_inter_t base;
    tuya_transporter_t tcp_transporter;
    tuya_tls_hander tls_handler;

    int socket_fd;
    int write_timeout;
    int read_timeout;
} * tuya_tls_transporter_t;

static int __tls_transporter_send_cb(void *ctx, const unsigned char *buf, size_t len)
{
    tuya_tls_transporter_t tls_transporter = (tuya_tls_transporter_t)ctx;

    return tuya_transporter_write(tls_transporter->tcp_transporter, (uint8_t *)buf, len,
                                  tls_transporter->write_timeout);
}
static int __tls_transporter_recv_cb(void *ctx, unsigned char *buf, size_t len)
{
    tuya_tls_transporter_t tls_transporter = (tuya_tls_transporter_t)ctx;
    int ret = tuya_transporter_read(tls_transporter->tcp_transporter, buf, len, tls_transporter->read_timeout);

    /*
     * A timeout in the TCP transporter is not a TLS transport error.  mbedTLS
     * expects MBEDTLS_ERR_SSL_WANT_READ for this case; returning Tuya's
     * OPRT_RESOURCE_NOT_READY leaks -23 into mbedTLS (and the ESP-IDF dynamic
     * buffer implementation reports it as an error on every idle MQTT read).
     */
    return ret == OPRT_RESOURCE_NOT_READY ? MBEDTLS_ERR_SSL_WANT_READ : ret;
}

/**
 * @brief Establishes a TLS connection with the specified host and port.
 *
 * This function is used to establish a TLS connection with the specified host
 * and port.
 *
 * @param t The Tuya transporter object.
 * @param host The host name or IP address of the server to connect to.
 * @param port The port number of the server to connect to.
 * @param timeout_ms The timeout value in milliseconds for the connection
 * attempt.
 *
 * @return The result of the connection attempt.
 *         - OPRT_OK if the connection was successful.
 *         - Other error codes if the connection failed.
 */
OPERATE_RET tuya_tls_transporter_connect(tuya_transporter_t t, const char *host, int port, int timeout_ms)
{

    OPERATE_RET op_ret = OPRT_OK;
    tuya_tls_transporter_t tls_transporter = (tuya_tls_transporter_t)t;

    op_ret = tuya_transporter_connect(tls_transporter->tcp_transporter, host, port, timeout_ms);
    if (op_ret != OPRT_OK) {
        PR_ERR("tcp transporter connect failed,ret:-0x%x", op_ret * (-1));
        return op_ret;
    }

    tuya_transporter_ctrl(tls_transporter->tcp_transporter, TUYA_TRANSPORTER_GET_TCP_SOCKET,
                          &tls_transporter->socket_fd);

    /* default config tls bio */
    tuya_tls_config_t *config = tuya_tls_config_get(tls_transporter->tls_handler);
    if (config->f_send == NULL || config->f_recv == NULL) {
        config->f_send = __tls_transporter_send_cb;
        config->f_recv = __tls_transporter_recv_cb;
        config->user_data = tls_transporter;
        tuya_tls_transporter_ctrl((tuya_transporter_t)tls_transporter, TUYA_TRANSPORTER_SET_TLS_CONFIG, config);
    }

    op_ret = tuya_tls_connect(tls_transporter->tls_handler, (char *)host, port, tls_transporter->socket_fd, timeout_ms);
    if (OPRT_OK != op_ret) {
        PR_ERR("tls transporter connect err:%d", op_ret);
        tuya_tls_transporter_close(t);
        return OPRT_COM_ERROR; // tbd
    }

    return OPRT_OK;
}

/**
 * @brief Closes the TLS transporter.
 *
 * This function is used to close the TLS transporter associated with the given
 * `tuya_transporter_t` object.
 *
 * @param t The TLS transporter to be closed.
 * @return The result of the operation. Possible values are:
 *         - OPRT_OK: The TLS transporter was closed successfully.
 *         - Other error codes indicating the failure to close the TLS
 * transporter.
 */
OPERATE_RET tuya_tls_transporter_close(tuya_transporter_t t)
{

    tuya_tls_transporter_t tls_transporter = (tuya_tls_transporter_t)t;

    PR_DEBUG("tls transporter close socket fd:%d", tls_transporter->socket_fd);
    if (tls_transporter->socket_fd >= 0) {
        tuya_transporter_close(tls_transporter->tcp_transporter);
        tls_transporter->socket_fd = -1;
    } else {
        PR_DEBUG("tls transporter already closed");
        return OPRT_OK;
    }
    PR_DEBUG("tls transporter close tls handler:%p", tls_transporter->tls_handler);

    if (tls_transporter->tls_handler) {
        tuya_tls_disconnect(tls_transporter->tls_handler);
    }

    return OPRT_OK;
}

/**
 * @brief Reads data from the TLS transporter.
 *
 * This function reads data from the TLS transporter specified by the parameter
 * `t`.
 *
 * @param t The TLS transporter to read data from.
 * @param buf The buffer to store the read data.
 * @param len The maximum number of bytes to read.
 * @param timeout_ms The timeout value in milliseconds.
 * @return The number of bytes read on success, or a negative error code on
 * failure.
 */
OPERATE_RET tuya_tls_transporter_read(tuya_transporter_t t, uint8_t *buf, int len, int timeout_ms)
{

    tuya_tls_transporter_t tls_transporter = (tuya_tls_transporter_t)t;

    tls_transporter->read_timeout = timeout_ms;

    int ret = tuya_tls_read(tls_transporter->tls_handler, buf, len);

    /* Keep the transport API's established no-data result for MQTT callers. */
    return ret == MBEDTLS_ERR_SSL_WANT_READ ? OPRT_RESOURCE_NOT_READY : ret;
}

/**
 * @brief Writes data to the TLS transporter.
 *
 * This function is used to write data to the TLS transporter.
 *
 * @param t The TLS transporter object.
 * @param buf Pointer to the buffer containing the data to be written.
 * @param len The length of the data to be written.
 * @param timeout_ms The timeout value in milliseconds for the write operation.
 *
 * @return The result of the write operation.
 *         - OPRT_OK: Write operation successful.
 *         - Other error codes: Write operation failed.
 */
OPERATE_RET tuya_tls_transporter_write(tuya_transporter_t t, uint8_t *buf, int len, int timeout_ms)
{

    tuya_tls_transporter_t tls_transporter = (tuya_tls_transporter_t)t;

    tls_transporter->write_timeout = timeout_ms;
    return tuya_tls_write(tls_transporter->tls_handler, buf, len);
}

/**
 * @brief Reads data from the TLS transporter.
 *
 * This function reads data from the TLS transporter specified by the parameter
 * `t`. The function will block for up to `timeout_ms` milliseconds waiting for
 * data to be available.
 *
 * @param t The TLS transporter to read data from.
 * @param timeout_ms The timeout value in milliseconds for the read operation.
 * @return An `OPERATE_RET` value indicating the result of the read operation.
 *         - `OPRT_OK` if the read operation was successful.
 *         - Other error codes if the read operation failed.
 */
OPERATE_RET tuya_tls_transporter_poll_read(tuya_transporter_t t, int timeout_ms)
{

    tuya_tls_transporter_t tls_transporter = (tuya_tls_transporter_t)t;

    return tuya_transporter_poll_read(tls_transporter->tcp_transporter, timeout_ms);
}

/**
 * @brief Controls the TLS transporter.
 *
 * This function is used to control the TLS transporter by sending different
 * commands.
 *
 * @param t The TLS transporter to control.
 * @param cmd The command to execute.
 * @param args Additional arguments for the command.
 *
 * @return The result of the operation.
 */
OPERATE_RET tuya_tls_transporter_ctrl(tuya_transporter_t t, uint32_t cmd, void *args)
{
    OPERATE_RET ret = OPRT_OK;
    tuya_tls_transporter_t tls_transporter = (tuya_tls_transporter_t)t;

    switch (cmd) {
    case TUYA_TRANSPORTER_SET_TLS_CERT: {
        char *cert_data = (char *)args;
        tuya_tls_config_t *config = tuya_tls_config_get(tls_transporter->tls_handler);
        config->ca_cert = cert_data;
        config->ca_cert_size = strlen(cert_data) + 1;
        tuya_tls_config_set(tls_transporter->tls_handler, config);
        break;
    }
    case TUYA_TRANSPORTER_SET_TLS_CONFIG: {
        tuya_tls_config_set(tls_transporter->tls_handler, (tuya_tls_config_t *)args);
        break;
    }
    case TUYA_TRANSPORTER_GET_TLS_CONFIG: {
        tuya_tls_config_t *config = tuya_tls_config_get(tls_transporter->tls_handler);
        tuya_tls_config_t **s = args;
        *s = (void *)config;
        break;
    }

    default: {
        ret = tuya_transporter_ctrl(tls_transporter->tcp_transporter, cmd, args);
        break;
    }
    }

    return ret;
}

/**
 * @brief Creates a Tuya TLS transporter.
 *
 * This function creates and initializes a Tuya TLS transporter object.
 *
 * @return The created Tuya transporter object.
 */
tuya_transporter_t tuya_tls_transporter_create(void)
{

    tuya_tls_transporter_t t = tal_malloc(sizeof(struct tls_transporter_inter_t));
    if (t == NULL) {
        PR_ERR("websocket transporter malloc fail");
        return NULL;
    }
    memset(t, 0, sizeof(struct tls_transporter_inter_t));
    t->socket_fd = -1;

    tuya_transporter_set_func((tuya_transporter_t)&t->base, tuya_tls_transporter_connect, tuya_tls_transporter_close,
                              tuya_tls_transporter_read, tuya_tls_transporter_write, tuya_tls_transporter_poll_read,
                              NULL, tuya_tls_transporter_destroy, tuya_tls_transporter_ctrl);
    t->tcp_transporter = tuya_tcp_transporter_create();
    t->tls_handler = tuya_tls_connect_create();
    if (t->tls_handler == NULL) {
        tuya_tcp_transporter_destroy(t->tcp_transporter);
        return NULL;
    }

    /* Default config */
    tuya_tls_transporter_ctrl((tuya_transporter_t)&t->base, TUYA_TRANSPORTER_SET_TLS_CONFIG,
                              &(tuya_tls_config_t){
                                  .mode = TUYA_TLS_SERVER_CERT_MODE,
                                  .verify = true,
                              });

    return &t->base;
}

/**
 * @brief Destroys a TLS transporter.
 *
 * This function is used to destroy a TLS transporter object.
 *
 * @param transporter The TLS transporter to be destroyed.
 * @return The result of the operation.
 *         - OPRT_OK: The TLS transporter was successfully destroyed.
 *         - Other error codes may be returned to indicate specific errors.
 */
OPERATE_RET tuya_tls_transporter_destroy(tuya_transporter_t transporter)
{
    tuya_tls_transporter_t t = (tuya_tls_transporter_t)transporter;
    tuya_tls_connect_destroy(t->tls_handler);
    t->tls_handler = NULL;

    tuya_tcp_transporter_destroy(t->tcp_transporter);
    if (t) {
        tal_free(t);
    }

    return OPRT_OK;
}
