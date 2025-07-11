/**
 * @file tcp_transporter.c
 * @brief Implementation of TCP transporter for Tuya Cloud service.
 *
 * This file provides the implementation of TCP transporter functions,
 * including connection management, data transmission, and configuration
 * for Tuya Cloud communication over TCP.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-10   yangjie     Support multiple network interfaces
 *
 */

#include "tuya_cloud_types.h"
#include "netmgr.h"
#include "tal_api.h"
#include "tal_network.h"
#include "tuya_transporter.h"
#include "tcp_transporter.h"
#include "tal_network.h"

typedef struct tcp_transporter_inter_t {
    struct tuya_transporter_inter_t base;
    tuya_tcp_config_t config;
    int socket_fd;
} *tuya_tcp_transporter_t;

/**
 * @brief Connects to a TCP server using the Tuya transporter.
 *
 * This function establishes a TCP connection to the specified host and port
 * using the Tuya transporter.
 *
 * @param t The Tuya transporter object.
 * @param host The host address to connect to.
 * @param port The port number to connect to.
 * @param timeout_ms The timeout value in milliseconds for the connection
 * attempt.
 *
 * @return The result of the connection attempt.
 *         Possible return values:
 *         - OPRT_OK: Connection successful.
 *         - OPRT_INVALID_PARM: Invalid parameter(s) passed.
 *         - OPRT_TIMEOUT: Connection attempt timed out.
 *         - OPRT_TCP_CONNECT_FAILED: TCP connection failed.
 *         - OPRT_TCP_CONNECT_CLOSED: TCP connection closed.
 *         - OPRT_TCP_CONNECT_UNKNOWN: Unknown TCP connection error.
 */
OPERATE_RET tuya_tcp_transporter_connect(tuya_transporter_t t, const char *host, int port, int timeout_ms)
{

    OPERATE_RET op_ret = OPRT_OK;
    tuya_tcp_transporter_t tcp_transporter = (tuya_tcp_transporter_t)t;

    /*resolve ip addr of host*/
    TUYA_IP_ADDR_T hostaddr;
    op_ret = tal_net_gethostbyname(host, &hostaddr);
    if (op_ret != OPRT_OK) {
        PR_ERR("DNS parser host %s failed %d", host, op_ret);
        return OPRT_MID_TRANSPORT_DNS_PARSED_FAILED;
    }

    tcp_transporter->socket_fd = tal_net_socket_create(PROTOCOL_TCP);
    if (tcp_transporter->socket_fd < 0) {
        op_ret = OPRT_MID_TRANSPORT_SOCK_CREAT_FAILED;
        goto err_out;
    }
    // reuse socket port
    if (tcp_transporter->config.isReuse && (OPRT_OK != tal_net_set_reuse(tcp_transporter->socket_fd))) {
        op_ret = OPRT_MID_TRANSPORT_SOCK_SET_REUSE_FAILED;
        goto err_out;
    }
    // disable Nagle Algorithm
    if (tcp_transporter->config.isDisableNagle && (OPRT_OK != tal_net_disable_nagle(tcp_transporter->socket_fd))) {
        op_ret = OPRT_MID_TRANSPORT_SOCK_SET_DISABLE_NAGLE_FAILED;
        goto err_out;
    }
    // keepalive ,idle time, interval, count setting
    if (tcp_transporter->config.isKeepAlive &&
        (OPRT_OK != tal_net_set_keepalive(tcp_transporter->socket_fd, TRUE, tcp_transporter->config.keepAliveIdleTime,
                                          tcp_transporter->config.keepAliveInterval,
                                          tcp_transporter->config.keepAliveCount))) {
        op_ret = OPRT_MID_TRANSPORT_SOCK_SET_KEEP_ALIVE_FAILED;
        goto err_out;
    }
    // block socket port
    if (tcp_transporter->config.isBlock && (OPRT_OK != tal_net_set_block(tcp_transporter->socket_fd, TRUE))) {
        op_ret = OPRT_MID_TRANSPORT_SOCK_SET_BLOCK_FAILED;
        goto err_out;
    }

    // socket bind random port
    NW_IP_S nw_ip = {0};
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_IP, &nw_ip);
    tcp_transporter->config.bindAddr = tal_net_str2addr(nw_ip.ip);

    if ((tcp_transporter->config.bindPort || tcp_transporter->config.bindAddr) &&
        (OPRT_OK != tal_net_bind(tcp_transporter->socket_fd, tcp_transporter->config.bindAddr,
                                 tcp_transporter->config.bindPort))) { // socket bind port
        op_ret = OPRT_MID_TRANSPORT_SOCK_NET_BIND_FAILED;
        goto err_out;
    } else {
        PR_DEBUG("bind ip:%08x port:%d ok", tcp_transporter->config.bindAddr, tcp_transporter->config.bindPort);
    }

    if (tcp_transporter->config.sendTimeoutMs &&
        (OPRT_OK !=
         tal_net_set_timeout(tcp_transporter->socket_fd, tcp_transporter->config.sendTimeoutMs, TRANS_SEND))) {
        // PR_DEBUG("socket fd set sendTimeout:%d
        // failed",tcp_transporter->config.sendTimeoutMs); op_ret =
        // OPRT_MID_TRANSPORT_SOCK_SET_TIMEOUT_FAILED; goto err_out;
    }

    if (tcp_transporter->config.recvTimeoutMs &&
        (OPRT_OK !=
         tal_net_set_timeout(tcp_transporter->socket_fd, tcp_transporter->config.recvTimeoutMs, TRANS_RECV))) {
        // op_ret = OPRT_MID_TRANSPORT_SOCK_SET_TIMEOUT_FAILED;
        // goto err_out;
    }

    if (tal_net_connect(tcp_transporter->socket_fd, hostaddr, port) < 0) {
        op_ret = OPRT_MID_TRANSPORT_TCP_CONNECD_FAILED;
        goto err_out;
    }

    return OPRT_OK;
err_out:
    if (tcp_transporter->socket_fd >= 0) {
        tal_net_close(tcp_transporter->socket_fd);
        tcp_transporter->socket_fd = -1;
    }
    return op_ret; // tbd
}

/**
 * @brief Controls the TCP transporter.
 *
 * This function is used to control the TCP transporter by sending specific
 * commands and arguments.
 *
 * @param t The TCP transporter to control.
 * @param cmd The command to send.
 * @param args The arguments associated with the command.
 *
 * @return The result of the operation.
 */
OPERATE_RET tuya_tcp_transporter_ctrl(tuya_transporter_t t, uint32_t cmd, void *args)
{
    OPERATE_RET ret = OPRT_OK;
    tuya_tcp_transporter_t tcp_transporter = (tuya_tcp_transporter_t)t;

    switch (cmd) {
    case TUYA_TRANSPORTER_SET_TCP_CONFIG: {
        tuya_tcp_config_t *pConfig = (tuya_tcp_config_t *)args;
        if (pConfig) {
            memcpy(&tcp_transporter->config, pConfig, sizeof(tuya_tcp_config_t));
        } else {
            ret = OPRT_INVALID_PARM;
        }
        break;
    }
    case TUYA_TRANSPORTER_GET_TCP_SOCKET: {
        int *fd = (int *)args;
        if (fd && tcp_transporter->socket_fd >= 0) {
            *fd = tcp_transporter->socket_fd;
        } else {
            ret = OPRT_INVALID_PARM;
        }
        break;
    }
    default: {
        break;
    }
    }

    return ret;
}

/**
 * @brief Closes the TCP transporter.
 *
 * This function closes the TCP transporter by closing the socket file
 * descriptor associated with it. It sets the socket file descriptor to -1 to
 * indicate that the transporter is closed.
 *
 * @param t The TCP transporter to close.
 * @return The result of the operation. Returns OPRT_OK if the transporter is
 *         successfully closed, or an error code if an error occurs.
 */
OPERATE_RET tuya_tcp_transporter_close(tuya_transporter_t t)
{
    tuya_tcp_transporter_t tcp_transporter = (tuya_tcp_transporter_t)t;

    if (tcp_transporter->socket_fd >= 0) {
        PR_DEBUG("tcp transporter close socket fd:%d", tcp_transporter->socket_fd);
        tal_net_close(tcp_transporter->socket_fd);
    }
    tcp_transporter->socket_fd = -1;

    return OPRT_OK;
}

/**
 * @brief Polls for incoming data on a TCP transporter.
 *
 * This function is used to check if there is any incoming data available to be
 * read from the TCP transporter.
 *
 * @param t The TCP transporter to poll for incoming data.
 * @param timeout_ms The timeout value in milliseconds for the polling
 * operation. If set to 0, the function will return immediately.
 *
 * @return The result of the operation. Possible return values are:
 *         - OPRT_OK: The operation was successful.
 *         - OPRT_TIMEOUT: The polling operation timed out.
 *         - Other error codes indicating a failure.
 */
OPERATE_RET tuya_tcp_transporter_poll_read(tuya_transporter_t t, int timeout_ms)
{
    int ret = 0;
    TUYA_FD_SET_T readfd;
    TUYA_FD_SET_T errfd;
    tuya_tcp_transporter_t tcp_transporter = (tuya_tcp_transporter_t)t;
    int socket_fd = tcp_transporter->socket_fd;

    if (socket_fd < 0) {
        PR_ERR("socket fd:%d", socket_fd);
        return OPRT_INVALID_PARM;
    }

    tal_net_fd_zero(&readfd);
    tal_net_fd_zero(&errfd);
    tal_net_fd_set(socket_fd, &readfd);
    tal_net_fd_set(socket_fd, &errfd);

    ret = tal_net_select(socket_fd + 1, &readfd, NULL, &errfd, timeout_ms);
    if ((ret > 0) && tal_net_fd_isset(socket_fd, &errfd)) {
        ret = -1; // socket is fault
        PR_ERR("socket fd %d is fault", socket_fd);
    }

    return ret;
}

/**
 * @brief Polls for write availability on a TCP transporter.
 *
 * This function is used to check if the TCP transporter is ready for writing
 * data.
 *
 * @param t The TCP transporter to poll.
 * @param timeout_ms The timeout value in milliseconds.
 * @return The result of the operation.
 *         - OPRT_OK: The operation was successful.
 *         - Other error codes indicating the failure of the operation.
 */
OPERATE_RET tuya_tcp_transporter_poll_write(tuya_transporter_t t, int timeout_ms)
{

    TUYA_FD_SET_T writefd;
    tuya_tcp_transporter_t tcp_transporter = (tuya_tcp_transporter_t)t;

    tal_net_fd_zero(&writefd);
    tal_net_fd_set(tcp_transporter->socket_fd, &writefd);

    return tal_net_select(tcp_transporter->socket_fd + 1, NULL, &writefd, NULL, timeout_ms);
}

/**
 * @brief Reads data from the TCP transporter.
 *
 * This function reads data from the TCP transporter and stores it in the
 * provided buffer.
 *
 * @param t The TCP transporter.
 * @param buf The buffer to store the read data.
 * @param len The length of the buffer.
 * @param timeout_ms The timeout value in milliseconds.
 * @return The result of the operation.
 */
OPERATE_RET tuya_tcp_transporter_read(tuya_transporter_t t, uint8_t *buf, int len, int timeout_ms)
{
    tuya_tcp_transporter_t tcp_transporter = (tuya_tcp_transporter_t)t;
    if (tcp_transporter->socket_fd < 0) {
        PR_ERR("socket fd:%d", tcp_transporter->socket_fd);
        return OPRT_INVALID_PARM;
    }

    int ret = 0;
    if (timeout_ms > 0) {
        ret = tuya_tcp_transporter_poll_read(t, timeout_ms);
    }

    if (ret < 0) {
        return ret;
    }

    if ((timeout_ms > 0) && (ret == 0)) {
        return OPRT_RESOURCE_NOT_READY;
    }

    return tal_net_recv(tcp_transporter->socket_fd, buf, len);
}

/**
 * @brief Writes data to the TCP transporter.
 *
 * This function is used to write data to the TCP transporter.
 *
 * @param t The TCP transporter.
 * @param buf The buffer containing the data to be written.
 * @param len The length of the data to be written.
 * @param timeout_ms The timeout value in milliseconds.
 * @return The result of the operation.
 */
OPERATE_RET tuya_tcp_transporter_write(tuya_transporter_t t, uint8_t *buf, int len, int timeout_ms)
{
    int ret = OPRT_COM_ERROR;
    tuya_tcp_transporter_t tcp_transporter = (tuya_tcp_transporter_t)t;
    if (tcp_transporter->socket_fd < 0) {
        PR_ERR("socket fd:%d", tcp_transporter->socket_fd);
        return OPRT_INVALID_PARM;
    }

    if (timeout_ms > 0 && tuya_tcp_transporter_poll_write(t, timeout_ms) <= 0) {
        return OPRT_RESOURCE_NOT_READY;
    }

    ret = tal_net_send(tcp_transporter->socket_fd, buf, len);
    if (ret < 0) {
        if ((tal_net_get_errno() == UNW_EINTR) || (tal_net_get_errno() == UNW_EAGAIN)) {
            tal_system_sleep(30);
            ret = tal_net_send(tcp_transporter->socket_fd, buf, len);
        }
    }

    return ret;
}

/**
 * @brief Destroys a TCP transporter.
 *
 * This function is used to destroy a TCP transporter object.
 *
 * @param transporter The TCP transporter to be destroyed.
 * @return The result of the operation.
 *         - OPRT_OK: The TCP transporter was successfully destroyed.
 *         - Other error codes may be returned in case of failure.
 */
OPERATE_RET tuya_tcp_transporter_destroy(tuya_transporter_t transporter)
{
    if (transporter) {
        tal_free(transporter);
    }
    return OPRT_OK;
}

/**
 * @brief Creates a TCP transporter for Tuya Cloud service.
 *
 * This function creates and initializes a TCP transporter for Tuya Cloud
 * service.
 *
 * @return The created TCP transporter.
 */
tuya_transporter_t tuya_tcp_transporter_create(void)
{

    tuya_tcp_transporter_t t = tal_malloc(sizeof(struct tcp_transporter_inter_t));
    if (t == NULL) {
        PR_ERR("websocket transporter malloc fail");
        return NULL;
    }
    memset(t, 0, sizeof(struct tcp_transporter_inter_t));

    t->socket_fd = -1;

    tuya_transporter_set_func((tuya_transporter_t)&t->base, tuya_tcp_transporter_connect, tuya_tcp_transporter_close,
                              tuya_tcp_transporter_read, tuya_tcp_transporter_write, tuya_tcp_transporter_poll_read,
                              tuya_tcp_transporter_poll_write, tuya_tcp_transporter_destroy, tuya_tcp_transporter_ctrl);

    return &t->base;
}
