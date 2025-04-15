/**
 * @file websocket_netio.c
 * @brief Implements network I/O operations for WebSocket communication
 *
 * This source file provides the implementation of low-level network I/O
 * functionality for WebSocket communications. It includes functions for
 * socket operations, TLS/SSL handling, raw data transmission, and connection
 * management. The implementation supports both secure and non-secure
 * connections, timeout handling, connection state management, and error
 * recovery mechanisms. This file is essential for developers working on IoT
 * applications that require reliable network communication with proper
 * error handling and security features through the WebSocket protocol.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "websocket_utils.h"
#include "websocket_netio.h"
#include "tal_system.h"
#include "tal_network.h"
#include "tuya_tls.h"
#include "iotdns.h"

static int __websocket_send_raw(int fd, uint8_t *buf, size_t len)
{
    TUYA_ERRNO err_no = UNW_SUCCESS;
    int send_len = 0;
    WS_CHECK_NULL_RET(buf);

    send_len = tal_net_send(fd, buf, len);
    if (send_len < 0) {
        err_no = tal_net_get_errno();
        PR_ERR("websocket raw tal_net_send error, fd: %d, send_len: %d, err_no: %d", fd, send_len, err_no);
        if (UNW_EINTR == err_no || UNW_EAGAIN == err_no) {

            tal_system_sleep(/* 100 */ 10);
            send_len = tal_net_send(fd, buf, len);
            if (send_len < 0) {
                err_no = tal_net_get_errno();
                PR_ERR("websocket raw tal_net_send error, fd: %d, send_len: %d, err_no: %d", fd, send_len, err_no);
            }
        }
    }
    return send_len;
}

static int __websocket_recv_raw(WEBSOCKET_S *ws, uint8_t *buf, size_t size, int *recv_len)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_ERRNO err_no = UNW_SUCCESS;
    WS_CHECK_NULL_RET(ws);
    WS_CHECK_NULL_RET(buf);
    WS_CHECK_NULL_RET(recv_len);
    WS_CHECK_BOOL_RET_VAL(0 == size, OPRT_INVALID_PARM);

    if (WS_RUN_STATE_CONNECT == ws->run_state) {
        /**
         * in handshake recv stage
         */
        rt = websocket_netio_select_read(ws, ws->handshake_recv_timeout);
        if (rt < 0) {
            err_no = tal_net_get_errno();
            PR_ERR("websocket %p select error, rt:%d, err_no:%d", ws, rt, err_no);
            return OPRT_COM_ERROR;
        } else if (rt == 0) {
            err_no = tal_net_get_errno();
            PR_ERR("websocket %p select timeout, rt:%d, err_no:%d", ws, rt, err_no);
            return OPRT_COM_ERROR;
        }

        rt = tal_net_recv(ws->sockfd, buf, size);
        if (rt < 0) {
            err_no = tal_net_get_errno();
            PR_ERR("websocket %p tal_net_recv, fd: %d, rt:%d, err_no:%d", ws, ws->sockfd, rt, err_no);
            return OPRT_COM_ERROR;
        }
    } else {
        /**
         * in frame recv stage
         */

#define WEBSOCKET_SELECT_TIMEOUT (1000 * 10) // ms
        while (1) {
            rt = websocket_netio_select_read(ws, WEBSOCKET_SELECT_TIMEOUT);
            if (rt < 0) {
                err_no = tal_net_get_errno();
                PR_ERR("websocket %p select error, rt:%d, err_no:%d", ws, rt, err_no);
                return OPRT_COM_ERROR;
            } else if (rt == 0) {
                PR_ERR("websocket %p select timeout, rt:%d", ws, rt);
                continue;
            }
            break;
        }
#define RETRY_TIMEOUT (100) /** unit ms */
        int retry_times = 0;
    RECV_RETRY:
        rt = tal_net_recv(ws->sockfd, buf, size);
        if (rt < 0) {
            err_no = tal_net_get_errno();
            if (UNW_EWOULDBLOCK == err_no || UNW_EINTR == err_no || UNW_EAGAIN == err_no) {
                PR_ERR("websocket %p tal_net_recv, fd: %d, rt:%d, err_no:%d", ws, ws->sockfd, rt, err_no);
                tal_system_sleep(10);

                /** NOTE: retry timeout */
                retry_times++;
                if (retry_times >= (RETRY_TIMEOUT / 10)) {
                    PR_ERR("websocket %p tal_net_recv, fd: %d, rt:%d, err_no:%d, retry times %d", ws, ws->sockfd, rt,
                           err_no, retry_times);
                    return OPRT_COM_ERROR;
                }

                goto RECV_RETRY;
            }
            PR_ERR("websocket %p tal_net_recv, fd: %d, rt:%d, err_no:%d", ws, ws->sockfd, rt, err_no);
            return OPRT_COM_ERROR;
        }
    }

    *recv_len = rt;
    return OPRT_OK;
}

static int __websocket_tls_send_cb(void *ctx, const uint8_t *buf, size_t len)
{
    WEBSOCKET_S *ws = (WEBSOCKET_S *)ctx;
    WS_CHECK_NULL_RET_VAL(ws, -1);

    WS_DEBUG("websocket %p, TLS write", ws);

    return __websocket_send_raw(ws->sockfd, (uint8_t *)buf, len);
}

static int __websocket_tls_recv_cb(void *ctx, uint8_t *buf, size_t len)
{
    int recv_len = 0;
    OPERATE_RET rt = OPRT_OK;
    WEBSOCKET_S *ws = (WEBSOCKET_S *)ctx;
    WS_CHECK_NULL_RET_VAL(ws, -1);

    rt = __websocket_recv_raw(ws, buf, len, &recv_len);
    WS_DEBUG("websocket %p, TLS read, rt:%d, recv_len:%d", ws, rt, recv_len);
    if (OPRT_OK != rt) {
        PR_ERR("websocket %p, __websocket_recv_raw, rt:%d", ws, rt);
        return rt;
    }

    return recv_len;
}

/**
 * @brief Create and initialize a websocket network connection
 *
 * This function creates a TCP socket and configures it with appropriate settings for websocket communication.
 * It sets up socket options including port reuse, disabling Nagle algorithm, blocking mode, and keepalive if specified.
 *
 * @param[in] ws Pointer to the websocket structure
 *
 * @return OPERATE_RET
 * @retval OPRT_OK Success
 * @retval OPRT_SOCK_ERR Socket creation failed
 * @retval OPRT_SET_SOCK_ERR Socket configuration failed
 */
OPERATE_RET websocket_netio_open(WEBSOCKET_S *ws)
{
    WS_CHECK_NULL_RET(ws);

    ws->sockfd = tal_net_socket_create(PROTOCOL_TCP);
    if (ws->sockfd < 0) {
        PR_ERR("websocket %p tal_net_socket_create error, err_no: %d", ws, tal_net_get_errno());
        return OPRT_SOCK_ERR;
    }

    /* Set port reuse */
    if (UNW_SUCCESS != tal_net_set_reuse(ws->sockfd)) {
        PR_ERR("websocket %p tuya_hal_net_set_reuse error, err_no: %d", ws, tal_net_get_errno());
        return OPRT_SET_SOCK_ERR;
    }

    /* Disable Nagle algorithm */
    if (UNW_SUCCESS != tal_net_disable_nagle(ws->sockfd)) {
        PR_ERR("websocket %p tuya_hal_net_disable_nagle error, err_no: %d", ws, tal_net_get_errno());
        return OPRT_SET_SOCK_ERR;
    }

    /* Set blocking mode */
    if (UNW_SUCCESS != tal_net_set_block(ws->sockfd, TRUE)) {
        PR_ERR("websocket %p tuya_hal_net_set_block error, err_no: %d", ws, tal_net_get_errno());
        return OPRT_SET_SOCK_ERR;
    }

    if (ws->handshake_recv_timeout) {
        if (UNW_SUCCESS !=
            tal_net_set_keepalive(ws->sockfd, TRUE, ws->keep_alive_time + ws->handshake_recv_timeout, 5, 1)) {
            PR_WARN("websocket %p tal_net_set_keepalive error, err_no: %d", ws, tal_net_get_errno());
        }
    }

    return OPRT_OK;
}

/**
 * @brief Establish a websocket network connection
 *
 * This function establishes a network connection for the websocket. It first creates a TCP connection
 * to the specified host and port. If TLS is enabled, it will also establish a secure TLS connection
 * with the appropriate security configuration based on the TUYA_SECURITY_LEVEL.
 *
 * @param[in] ws Pointer to the websocket structure containing connection parameters
 *
 * @return OPERATE_RET
 * @retval OPRT_OK Connection established successfully
 * @retval OPRT_SOCK_CONN_ERR Connection failed (either TCP or TLS)
 */
OPERATE_RET websocket_netio_conn(WEBSOCKET_S *ws)
{
    OPERATE_RET rt = OPRT_OK;
    WS_CHECK_NULL_RET(ws);

    tal_net_set_timeout(ws->sockfd, 3000, TRANS_SEND);
    /** socket connect */
    if (tal_net_connect(ws->sockfd, ws->hostaddr, ws->port) < 0) {
        PR_ERR("websocket %p tal_net_connect error, fd: %d, %d.%d.%d.%d:%d, err_no: %d", ws, ws->sockfd,
               (ws->hostaddr >> 24) & 0xff, (ws->hostaddr >> 16) & 0xff, (ws->hostaddr >> 8) & 0xff,
               ws->hostaddr & 0xff, ws->port, tal_net_get_errno());
        return OPRT_SOCK_CONN_ERR;
    }
    WS_DEBUG("websocket %p raw connect success", ws);

    /** TLS connect */
    tuya_tls_config_t tls_config;
    memset(&tls_config, 0, sizeof(tls_config));
    if (ws->tls_enable) {

        if (ws->tls_hander) {
            tuya_tls_disconnect(ws->tls_hander);
            tuya_tls_connect_destroy(ws->tls_hander);
            ws->tls_hander = NULL;
        }
        ws->tls_hander = tuya_tls_connect_create();
        tls_config.f_recv = __websocket_tls_recv_cb;
        tls_config.f_send = __websocket_tls_send_cb;
        tls_config.user_data = ws;
#if (TUYA_SECURITY_LEVEL == TUYA_SL_0)
        const client_psk_info_t *tuya_psk = tuya_client_psk_get();
        if (tuya_psk) {
            tls_config.mode = TUYA_TLS_PSK_MODE;
            tls_config.psk_id = tuya_psk->psk_id;
            tls_config.psk_id_size = tuya_psk->psk_id_size;
            tls_config.psk_key = tuya_psk->psk_key;
            tls_config.psk_key_size = tuya_psk->psk_key_size;
        } else {
            tls_config.mode = TUYA_TLS_PSK_MODE;
        }
#elif (TUYA_SECURITY_LEVEL == TUYA_SL_1)

        /* HTTPS cert */
        tls_config.mode = TUYA_TLS_SERVER_CERT_MODE;
        tls_config.verify = true;
        TUYA_CALL_ERR_GOTO(tuya_iotdns_query_domain_certs(ws->uri, (uint8_t **)&tls_config.ca_cert,
                                                          (uint16_t *)&tls_config.ca_cert_size),
                           err_exit);

#elif (TUYA_SECURITY_LEVEL == TUYA_SL_2)
        const client_cert_info_t *cert = tuya_client_cert_get();
        tls_config.mode = TUYA_TLS_MUTUAL_CERT_MODE;
        tls_config.verify = true;
        tls_config.client_cert = (char *)cert->cert;
        tls_config.client_cert_size = cert->cert_len;
        tls_config.client_pkey = (char *)cert->private_key;
        tls_config.client_pkey_size = cert->private_key_len;
        tls_config.exception_cb = tuya_cert_get_tls_event_cb();
#endif
        tuya_tls_config_set(ws->tls_hander, &tls_config);
        WS_DEBUG("tls_config.mode=%d", tuya_tls_config_get(ws->tls_hander)->mode);
        rt = tuya_tls_connect(ws->tls_hander, ws->host, ws->port, ws->sockfd, ws->handshake_conn_timeout);
        if (OPRT_OK != rt || NULL == ws->tls_hander) {
            PR_ERR("websocket %p tuya_tls_connect %p error, fd: %d, rt:%d, err_no:%d", ws, ws->tls_hander, ws->sockfd,
                   rt, tal_net_get_errno());
            rt = OPRT_SOCK_CONN_ERR;
            goto err_exit;
        }
        WS_DEBUG("websocket %p tls connect success", ws);
    } else {
        PR_WARN("websocket %p tls %p disable, will only do raw connect", ws, ws->tls_hander);
    }

err_exit:
    if (tls_config.ca_cert) {
        tal_free(tls_config.ca_cert);
    }

    return rt;
}

/**
 * @brief Monitor websocket socket for read events
 *
 * This function uses select() to monitor the websocket socket for read events.
 * It waits for data to be available for reading within the specified timeout period.
 *
 * @param[in] ws Pointer to the websocket structure
 * @param[in] timeout_ms Timeout value in milliseconds
 *
 * @return OPERATE_RET
 * @retval OPRT_OK Select operation successful
 * @retval OPRT_SOCK_ERR Invalid socket
 */
OPERATE_RET websocket_netio_select_read(WEBSOCKET_S *ws, uint32_t timeout_ms)
{
    TUYA_FD_SET_T readfd;
    WS_CHECK_NULL_RET(ws);

    if (ws->sockfd < 0) {
        return OPRT_SOCK_ERR;
    }

    TAL_FD_ZERO(&readfd);
    TAL_FD_SET(ws->sockfd, &readfd);

    return tal_net_select(ws->sockfd + 1, &readfd, NULL, NULL, timeout_ms);
}

/**
 * @brief Send data through websocket connection
 *
 * This function sends data through the websocket connection. If TLS is enabled,
 * it will use TLS write operation; otherwise, it will use raw socket send.
 *
 * @param[in] ws Pointer to the websocket structure
 * @param[in] buf Buffer containing data to send
 * @param[in] len Length of data to send
 *
 * @return OPERATE_RET
 * @retval >0 Number of bytes sent
 * @retval <=0 Send operation failed
 */
OPERATE_RET websocket_netio_send(WEBSOCKET_S *ws, uint8_t *buf, size_t len)
{
    WS_CHECK_NULL_RET(ws);
    WS_CHECK_NULL_RET(buf);

    /** TLS tal_net_send */
    if (ws->tls_enable) {
        WS_CHECK_NULL_RET(ws->tls_hander);
        return tuya_tls_write(ws->tls_hander, buf, len);
    } else {
        WS_DEBUG("websocket %p tls %p disable, will only do raw tal_net_send!", ws, ws->tls_hander);
    }

    /** RAW tal_net_send */
    return __websocket_send_raw(ws->sockfd, buf, len);
}

/**
 * @brief Extended send function to ensure complete data transmission
 *
 * This function ensures that all data is sent by repeatedly calling websocket_netio_send
 * until all bytes are transmitted or an error occurs.
 *
 * @param[in] ws Pointer to the websocket structure
 * @param[in] data Buffer containing data to send
 * @param[in] len Length of data to send
 *
 * @return OPERATE_RET
 * @retval OPRT_OK All data sent successfully
 * @retval OPRT_SEND_ERR Send operation failed
 */
OPERATE_RET websocket_netio_send_ext(WEBSOCKET_S *ws, void *data, size_t len)
{
    OPERATE_RET rt = OPRT_OK;
    size_t sended_len = 0;
    uint8_t *buffer = (uint8_t *)data;
    WS_CHECK_NULL_RET(ws);
    WS_CHECK_NULL_RET(data);

    while (sended_len != len) {
        rt = websocket_netio_send(ws, buffer + sended_len, len - sended_len);
        if (rt <= 0) {
            PR_ERR("websocket %p websocket_netio_send failed, rt:%d, err_no:%d", ws, rt, tal_net_get_errno());
            return OPRT_SEND_ERR;
        }
        sended_len = sended_len + rt;
    }

    return OPRT_OK;
}

/**
 * @brief Thread-safe send function with mutex protection
 *
 * This function provides a thread-safe way to send data by using mutex locking.
 * It also checks connection status before sending data.
 *
 * @param[in] ws Pointer to the websocket structure
 * @param[in] data Buffer containing data to send
 * @param[in] len Length of data to send
 *
 * @return OPERATE_RET
 * @retval OPRT_OK Send operation successful
 * @retval OPRT_SOCK_CONN_ERR Connection lost
 */
OPERATE_RET websocket_netio_send_lock(WEBSOCKET_S *ws, void *data, size_t len)
{
    OPERATE_RET rt = OPRT_OK;
    WS_CHECK_NULL_RET(ws);
    WS_CHECK_NULL_RET(ws->mutex);
    WS_CHECK_NULL_RET(data);

    WS_ASSERT(OPRT_OK == tal_mutex_lock(ws->mutex));
    if (ws != NULL && !ws->is_connected) {
        PR_ERR("websocket %p is disconnected, tal_net_send failed", ws);
        WS_ASSERT(OPRT_OK == tal_mutex_unlock(ws->mutex));
        return OPRT_SOCK_CONN_ERR;
    }
    rt = websocket_netio_send_ext(ws, data, len);
    WS_ASSERT(OPRT_OK == tal_mutex_unlock(ws->mutex));

    return rt;
}

/**
 * @brief Receive data from websocket connection
 *
 * This function receives data from the websocket connection. If TLS is enabled,
 * it will use TLS read operation; otherwise, it will use raw socket receive.
 *
 * @param[in] ws Pointer to the websocket structure
 * @param[out] buf Buffer to store received data
 * @param[in] size Size of the buffer
 * @param[out] recv_len Pointer to store the number of bytes received
 *
 * @return OPERATE_RET
 * @retval OPRT_OK Receive operation successful
 * @retval OPRT_COM_ERROR TLS read error
 * @retval OPRT_INVALID_PARM Invalid parameters
 */
OPERATE_RET websocket_netio_recv(WEBSOCKET_S *ws, uint8_t *buf, size_t size, size_t *recv_len)
{
    OPERATE_RET rt = OPRT_OK;
    WS_CHECK_NULL_RET(ws);
    WS_CHECK_NULL_RET(buf);
    WS_CHECK_BOOL_RET_VAL(size == 0, OPRT_INVALID_PARM);
    WS_CHECK_NULL_RET(recv_len);

    /** TLS resv */
    if (ws->tls_enable) {
        WS_CHECK_NULL_RET(ws->tls_hander);
        rt = tuya_tls_read(ws->tls_hander, buf, size);
        WS_DEBUG("websocket %p tuya_tls_read %p, rt:%d", ws, ws->tls_hander, rt);
        if (rt < 0) {
            PR_ERR("websocket %p tuya_tls_read %p, rt:%d", ws, ws->tls_hander, rt);
            return OPRT_COM_ERROR;
        }
        *recv_len = rt;
        return OPRT_OK;
    } else {
        WS_DEBUG("websocket %p tls %p disable, will only do raw recv!", ws, ws->tls_hander);
    }

    /** RAW resv */
    return __websocket_recv_raw(ws, buf, size, (int *)recv_len);
}

/**
 * @brief Extended receive function to ensure complete data reception
 *
 * This function ensures that the specified amount of data is received by repeatedly
 * calling websocket_netio_recv until all bytes are received or an error occurs.
 *
 * @param[in] ws Pointer to the websocket structure
 * @param[out] buf Buffer to store received data
 * @param[in] len Expected length of data to receive
 *
 * @return OPERATE_RET
 * @retval OPRT_OK All data received successfully
 * @retval OPRT_RECV_ERR Receive operation failed
 */
OPERATE_RET websocket_netio_recv_ext(WEBSOCKET_S *ws, uint8_t *buf, size_t len)
{
    size_t total_recv_len = 0, once_recv_len = 0;
    OPERATE_RET rt = OPRT_OK;
    WS_CHECK_NULL_RET(ws);
    WS_CHECK_NULL_RET(buf);

    while (total_recv_len != len) {
        rt = websocket_netio_recv(ws, buf + total_recv_len, len - total_recv_len, &once_recv_len);
        if (OPRT_OK != rt) {
            PR_ERR("websocket %p websocket_netio_recv error, rt:%d", ws, rt);
            return OPRT_RECV_ERR;
        }
        total_recv_len = total_recv_len + once_recv_len;
    }

    return OPRT_OK;
}

/**
 * @brief Close the WebSocket connection gracefully
 *
 * Performs a clean WebSocket closure by:
 * 1. Sending a close frame with optional status code and reason
 * 2. Waiting for the close frame response from the server
 * 3. Closing the underlying TCP connection
 *
 * @param[in] ws_handle Handle to the WebSocket instance
 * @param[in] status_code Status code indicating close reason (optional)
 * @param[in] reason Text description of close reason (optional)
 *
 * @return OPERATE_RET
 *         - OPRT_OK: Connection closed successfully
 *         - OPRT_INVALID_PARM: Invalid WebSocket handle
 *         - OPRT_NOT_CONNECTED: WebSocket already disconnected
 *         - OPRT_CLOSE_FAILED: Error during closure process
 *         - Others: Other closure errors
 *
 * @note This function will wait for the server's close frame or timeout
 *       before returning
 */
OPERATE_RET websocket_netio_close(WEBSOCKET_S *ws)
{
    WS_CHECK_NULL_RET(ws);

    if (ws->sockfd > 0) {
#if defined(SHUTDOWN_MODE) && (SHUTDOWN_MODE == 1)
        PR_WARN("websocket %p fd %d shutdown", ws, ws->sockfd);
        tuya_hal_net_shutdown(ws->sockfd, UNW_SHUT_RDWR);
#endif
        PR_WARN("websocket %p fd %d close", ws, ws->sockfd);
        tal_net_close(ws->sockfd);
        ws->sockfd = -1;
    }

    return OPRT_OK;
}
