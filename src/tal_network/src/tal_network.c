/**
 * @file tal_network.c
 * @brief Network utilities implementation for Tuya SDK.
 *
 * This source file implements the network utilities for the Tuya SDK, providing
 * a layer of abstraction over different network operations such as socket
 * creation, configuration, and communication. It supports various operating
 * systems and networking libraries, including POSIX-compliant systems and
 * systems that use the LwIP networking stack.
 *
 * The file includes conditional compilation sections to include different
 * header files and define macros based on the operating system or the
 * networking library in use. For POSIX-compliant systems, standard networking
 * headers like <sys/socket.h>, <netinet/in.h>, and <arpa/inet.h> are included.
 * For systems using the LwIP networking stack, relevant LwIP headers like
 * "lwip/netdb.h" and "lwip/dns.h" are included.
 *
 * Additionally, the file defines macros to facilitate network operations across
 * different platforms, such as `ENABLE_BIND_INTERFACE` for binding operations
 * and `NET_USING_POSIX` to indicate the use of POSIX networking APIs. It aims
 * to provide a consistent and efficient networking interface for Tuya-based
 * applications across different hardware and software environments.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It requires configuration through
 * "tuya_iot_config.h" and interfaces with other parts of the Tuya SDK through
 * "tal_api.h".
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_iot_config.h"
#include "tal_api.h"

#include "tal_network_register.h"

/***********************************************************
************************macro define************************
***********************************************************/

/**
 * @brief Macro to execute network operation with error handling
 *
 * This macro simplifies the repetitive pattern of getting network ops,
 * checking if the operation exists, and calling it with parameters.
 *
 * @param op_name: The operation name in TAL_NETWORK_OPS_T struct
 * @param default_ret: Default return value if operation is not available
 * @param ...: Parameters to pass to the operation function
 */
#define TAL_NET_EXEC_OP(op_name, default_ret, ...)                                                                     \
    do {                                                                                                               \
        TAL_NETWORK_OPS_T *ops = tal_network_get_active_ops();                                                         \
        if (NULL != ops && ops->op_name) {                                                                             \
            return ops->op_name(__VA_ARGS__);                                                                          \
        }                                                                                                              \
        PR_ERR("Network operation %s not available", #op_name);                                                        \
        return default_ret;                                                                                            \
    } while (0)

/**
 * @brief Macro to execute network operation that returns void
 *
 * @param op_name: The operation name in TAL_NETWORK_OPS_T struct
 * @param ...: Parameters to pass to the operation function
 */
#define TAL_NET_EXEC_OP_VOID(op_name, ...)                                                                             \
    do {                                                                                                               \
        TAL_NETWORK_OPS_T *ops = tal_network_get_active_ops();                                                         \
        if (NULL != ops && ops->op_name) {                                                                             \
            ops->op_name(__VA_ARGS__);                                                                                 \
        }                                                                                                              \
    } while (0)

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Get error code of network
 *
 * @param void
 *
 * @note This API is used for getting error code of network.
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
TUYA_ERRNO tal_net_get_errno(void)
{
    TAL_NET_EXEC_OP(get_errno, -100);
}

/**
 * @brief Add file descriptor to set
 *
 * @param[in] fd: file descriptor
 * @param[in] fds: set of file descriptor
 *
 * @note This API is used to add file descriptor to set.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_fd_set(int fd, TUYA_FD_SET_T *fds)
{
    if ((fd < 0) || (fds == NULL)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(fd_set, OPRT_COM_ERROR, fd, fds);
}

/**
 * @brief Clear file descriptor from set
 *
 * @param[in] fd: file descriptor
 * @param[in] fds: set of file descriptor
 *
 * @note This API is used to clear file descriptor from set.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_fd_clear(int fd, TUYA_FD_SET_T *fds)
{
    if ((fd < 0) || (fds == NULL)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(fd_clear, OPRT_COM_ERROR, fd, fds);
}

/**
 * @brief Check file descriptor is in set
 *
 * @param[in] fd: file descriptor
 * @param[in] fds: set of file descriptor
 *
 * @note This API is used to check the file descriptor is in set.
 *
 * @return TRUE or FALSE
 */
OPERATE_RET tal_net_fd_isset(int fd, TUYA_FD_SET_T *fds)
{
    if ((fd < 0) || (fds == NULL)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(fd_isset, OPRT_COM_ERROR, fd, fds);
}

/**
 * @brief Clear all file descriptor in set
 *
 * @param[in] fds: set of file descriptor
 *
 * @note This API is used to clear all file descriptor in set.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_fd_zero(TUYA_FD_SET_T *fds)
{
    if (fds == NULL) {
        return -1;
    }

    TAL_NET_EXEC_OP(fd_zero, OPRT_COM_ERROR, fds);
}

/**
 * @brief Get available file descriptors
 *
 * @param[in] maxfd: max count of file descriptor
 * @param[out] readfds: a set of readalbe file descriptor
 * @param[out] writefds: a set of writable file descriptor
 * @param[out] errorfds: a set of except file descriptor
 * @param[in] ms_timeout: time out
 *
 * @note This API is used to get available file descriptors.
 *
 * @return the count of available file descriptors.
 */
int tal_net_select(const int maxfd, TUYA_FD_SET_T *readfds, TUYA_FD_SET_T *writefds, TUYA_FD_SET_T *errorfds,
                   const uint32_t ms_timeout)
{
    TAL_NET_EXEC_OP(select, -1, maxfd, readfds, writefds, errorfds, ms_timeout);
}

/**
 * @brief Get no block file descriptors
 *
 * @param[in] fd: file descriptor
 *
 * @note This API is used to get no block file descriptors.
 *
 * @return the count of no block file descriptors.
 */
int tal_net_get_nonblock(const int fd)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(get_nonblock, 0, fd);
}

/**
 * @brief Set block flag for file descriptors
 *
 * @param[in] fd: file descriptor
 * @param[in] block: block flag
 *
 * @note This API is used to set block flag for file descriptors.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_set_block(const int fd, const BOOL_T block)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(set_block, OPRT_COM_ERROR, fd, block);
}

/**
 * @brief Close file descriptors
 *
 * @param[in] fd: file descriptor
 *
 * @note This API is used to close file descriptors.
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
TUYA_ERRNO tal_net_close(const int fd)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(close, -1, fd);
}

/**
 * @brief Create a tcp/udp socket
 *
 * @param[in] type: protocol type, tcp or udp
 *
 * @note This API is used for creating a tcp/udp socket.
 *
 * @return file descriptor
 */
int tal_net_socket_create(const TUYA_PROTOCOL_TYPE_E type)
{
    TAL_NET_EXEC_OP(socket_create, -1, type);
}

/**
 * @brief Connect to network
 *
 * @param[in] fd: file descriptor
 * @param[in] addr: address information of server
 * @param[in] port: port information of server
 *
 * @note This API is used for connecting to network.
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
TUYA_ERRNO tal_net_connect(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(connect, -1, fd, addr, port);
}

/**
 * @brief Connect to network with raw data
 *
 * @param[in] fd: file descriptor
 * @param[in] p_socket: raw socket data
 * @param[in] len: data lenth
 *
 * @note This API is used for connecting to network with raw data.
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
TUYA_ERRNO tal_net_connect_raw(const int fd, void *p_socket_addr, const int len)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(connect_raw, -1, fd, p_socket_addr, len);
}

/**
 * @brief Bind to network
 *
 * @param[in] fd: file descriptor
 * @param[in] addr: address information of server
 * @param[in] port: port information of server
 *
 * @note This API is used for binding to network.
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
TUYA_ERRNO tal_net_bind(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(bind, -1, fd, addr, port);
}

/**
 * @brief Listen to network
 *
 * @param[in] fd: file descriptor
 * @param[in] backlog: max count of backlog connection
 *
 * @note This API is used for listening to network.
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
TUYA_ERRNO tal_net_listen(const int fd, const int backlog)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(listen, -1, fd, backlog);
}

/**
 * @brief Listen to network
 *
 * @param[in] fd: file descriptor
 * @param[out] addr: the accept ip addr
 * @param[out] port: the accept port number
 *
 * @note This API is used for listening to network.
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
int tal_net_accept(const int fd, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(accept, -1, fd, addr, port);
}

/**
 * @brief Send data to network
 *
 * @param[in] fd: file descriptor
 * @param[in] buf: send data buffer
 * @param[in] nbytes: buffer lenth
 *
 * @note This API is used for sending data to network
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
TUYA_ERRNO tal_net_send(const int fd, const void *buf, const uint32_t nbytes)
{
    if ((fd < 0) || (buf == NULL) || (nbytes == 0)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(send, -1, fd, buf, nbytes);
}

/**
 * @brief Send data to specified server
 *
 * @param[in] fd: file descriptor
 * @param[in] buf: send data buffer
 * @param[in] nbytes: buffer lenth
 * @param[in] addr: address information of server
 * @param[in] port: port information of server
 *
 * @note This API is used for sending data to network
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
TUYA_ERRNO tal_net_send_to(const int fd, const void *buf, const uint32_t nbytes, const TUYA_IP_ADDR_T addr,
                           const uint16_t port)
{
    if ((fd < 0) || (buf == NULL) || (nbytes == 0)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(send_to, -1, fd, buf, nbytes, addr, port);
}

/**
 * @brief Receive data from network
 *
 * @param[in] fd: file descriptor
 * @param[in] buf: receive data buffer
 * @param[in] nbytes: buffer lenth
 *
 * @note This API is used for receiving data from network
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
TUYA_ERRNO tal_net_recv(const int fd, void *buf, const uint32_t nbytes)
{
    if ((fd < 0) || (buf == NULL) || (nbytes == 0)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(recv, -1, fd, buf, nbytes);
}

/**
 * @brief Receive data from network with need size
 *
 * @param[in] fd: file descriptor
 * @param[in] buf: receive data buffer
 * @param[in] nbytes: buffer lenth
 * @param[in] nd_size: the need size
 *
 * @note This API is used for receiving data from network with need size
 *
 * @return >0 on success. Others on error
 */
int tal_net_recv_nd_size(const int fd, void *buf, const uint32_t buf_size, const uint32_t nd_size)
{
    if ((fd < 0) || (NULL == buf) || (buf_size == 0) || (nd_size == 0) || (buf_size < nd_size)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(recv_nd_size, -1, fd, buf, buf_size, nd_size);
}

/**
 * @brief Receive data from specified server
 *
 * @param[in] fd: file descriptor
 * @param[in] buf: receive data buffer
 * @param[in] nbytes: buffer lenth
 * @param[in] addr: address information of server
 * @param[in] port: port information of server
 *
 * @note This API is used for receiving data from specified server
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
TUYA_ERRNO tal_net_recvfrom(const int fd, void *buf, const uint32_t nbytes, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    if ((fd < 0) || (buf == NULL) || (nbytes == 0)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(recvfrom, -1, fd, buf, nbytes, addr, port);
}

/**
 * @brief Set socket options
 *
 * @param[in] fd: file descriptor
 * @param[in] level: setting level
 * @param[in] optname: the name of the option
 * @param[in] optval: the value of option
 * @param[in] optlen: the length of the option value
 *
 * @note This API is used for setting socket options.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_setsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname,
                               const void *optval, const int optlen)
{
    TAL_NET_EXEC_OP(setsockopt, OPRT_COM_ERROR, fd, level, optname, optval, optlen);
}

/**
 * @brief Get socket options
 *
 * @param[in] fd: file descriptor
 * @param[in] level: getting level
 * @param[in] optname: the name of the option
 * @param[out] optval: the value of option
 * @param[out] optlen: the length of the option value
 *
 * @note This API is used for getting socket options.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_getsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname, void *optval,
                               int *optlen)
{
    TAL_NET_EXEC_OP(getsockopt, OPRT_COM_ERROR, fd, level, optname, optval, optlen);
}

/**
 * @brief Set timeout option of socket fd
 *
 * @param[in] fd: file descriptor
 * @param[in] ms_timeout: timeout in ms
 * @param[in] type: transfer type, receive or send
 *
 * @note This API is used for setting timeout option of socket fd.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_set_timeout(const int fd, const int ms_timeout, const TUYA_TRANS_TYPE_E type)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(set_timeout, OPRT_COM_ERROR, fd, ms_timeout, type);
}

/**
 * @brief Set buffer_size option of socket fd
 *
 * @param[in] fd: file descriptor
 * @param[in] buf_size: buffer size in byte
 * @param[in] type: transfer type, receive or send
 *
 * @note This API is used for setting buffer_size option of socket fd.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_set_bufsize(const int fd, const int buf_size, const TUYA_TRANS_TYPE_E type)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(set_bufsize, OPRT_COM_ERROR, fd, buf_size, type);
}

/**
 * @brief Enable reuse option of socket fd
 *
 * @param[in] fd: file descriptor
 *
 * @note This API is used to enable reuse option of socket fd.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_set_reuse(const int fd)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(set_reuse, OPRT_COM_ERROR, fd);
}

/**
 * @brief Disable nagle option of socket fd
 *
 * @param[in] fd: file descriptor
 *
 * @note This API is used to disable nagle option of socket fd.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_disable_nagle(const int fd)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(disable_nagle, OPRT_COM_ERROR, fd);
}

/**
 * @brief Enable broadcast option of socket fd
 *
 * @param[in] fd: file descriptor
 *
 * @note This API is used to enable broadcast option of socket fd.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_set_broadcast(const int fd)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(set_broadcast, OPRT_COM_ERROR, fd);
}

/**
 * @brief Get address information by domain
 *
 * @param[in] domain: domain information
 * @param[in] addr: address information
 *
 * @note This API is used for getting address information by domain.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_gethostbyname(const char *domain, TUYA_IP_ADDR_T *addr)
{
    if ((domain == NULL) || (addr == NULL)) {
        return -2;
    }

    TAL_NET_EXEC_OP(gethostbyname, OPRT_COM_ERROR, domain, addr);
}

/**
 * @brief Set keepalive option of socket fd to monitor the connection
 *
 * @param[in] fd: file descriptor
 * @param[in] alive: keepalive option, enable or disable option
 * @param[in] idle: keep idle option, if the connection has no data exchange
 * with the idle time(in seconds), start probe.
 * @param[in] intr: keep interval option, the probe time interval.
 * @param[in] cnt: keep count option, probe count.
 *
 * @note This API is used to set keepalive option of socket fd to monitor the
 * connection.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_set_keepalive(int fd, const BOOL_T alive, const uint32_t idle, const uint32_t intr,
                                  const uint32_t cnt)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(set_keepalive, OPRT_COM_ERROR, fd, alive, idle, intr, cnt);
}

/**
 * @brief Get ip address by socket fd
 *
 * @param[in] fd: file descriptor
 * @param[out] addr: ip address
 *
 * @note This API is used for getting ip address by socket fd.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_get_socket_ip(int fd, TUYA_IP_ADDR_T *addr)
{
    TAL_NET_EXEC_OP(get_socket_ip, OPRT_COM_ERROR, fd, addr);
}

/**
 * @brief Change ip string to address
 *
 * @param[in] ip_str: ip string
 *
 * @note This API is used to change ip string to address.
 *
 * @return ip address
 */
TUYA_IP_ADDR_T tal_net_str2addr(const char *ip_str)
{
    TAL_NET_EXEC_OP(str2addr, 0, ip_str);
}

/**
 * @brief Change ip address to string
 *
 * @param[in] ipaddr: ip address
 *
 * @note This API is used to change ip address(in host byte order) to string(in
 * IPv4 numbers-and-dots(xx.xx.xx.xx) notion).
 *
 * @return ip string
 */
char *tal_net_addr2str(TUYA_IP_ADDR_T ipaddr)
{
    TAL_NET_EXEC_OP(addr2str, NULL, ipaddr);
}
