/**
 * @file tal_network.h
 * @brief Network utilities interface for Tuya SDK.
 *
 * This header file defines the network utilities interface for the Tuya SDK,
 * providing essential network functionalities such as IP address definitions
 * and network error handling. It includes definitions for loopback address, any
 * address, and broadcast address in the context of the Tuya SDK. Additionally,
 * it offers an API for retrieving the last network error code, which is crucial
 * for diagnosing network-related issues in Tuya-based applications.
 *
 * The interface abstracts underlying network operations, offering a simplified
 * and unified way of handling network communications across different platforms
 * supported by the Tuya SDK. This ensures that applications built on the Tuya
 * platform can manage network operations efficiently and consistently.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It requires the Tuya Cloud Types
 * definitions for proper functionality.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TAL_NETWORK_H__
#define __TAL_NETWORK_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* tuya sdk definition of 127.0.0.1 */
#ifndef TY_IPADDR_LOOPBACK
#define TY_IPADDR_LOOPBACK ((uint32_t)0x7f000001UL)
#endif
/* tuya sdk definition of 0.0.0.0 */
#ifndef TY_IPADDR_ANY
#define TY_IPADDR_ANY ((uint32_t)0x00000000UL)
#endif
/* tuya sdk definition of 255.255.255.255 */
#ifndef TY_IPADDR_BROADCAST
#define TY_IPADDR_BROADCAST ((uint32_t)0xffffffffUL)
#endif

/*
 * Active-link source address binding
 *
 * A target with more than one interface UP at once - Wi-Fi and cellular, say -
 * has a single routing table and no policy routing, so a flow only leaves the
 * interface netmgr selected if its socket is bound to that interface's address.
 * tal_network does this for outbound connections on its own, but it cannot do it
 * everywhere. The boundary below is deliberate; the gaps are real and are the
 * reason this note exists rather than being left implicit.
 *
 * Bound automatically to the active link source address:
 *  - everything that goes through tal_net_connect(), which is every outbound TCP
 *    flow in the SDK (TLS, HTTP, MQTT, ATOP) and any UDP socket the caller
 *    connect()s. A socket the caller already bound is left alone, so an explicit
 *    tal_net_bind() always wins, and a failed bind is logged and ignored rather
 *    than failing the connect.
 *
 * NOT bound, and what that means:
 *  - tal_net_gethostbyname(): resolution runs inside lwIP or the platform
 *    resolver and there is no socket to bind. Steering a DNS query is an lwIP-level
 *    setting, out of reach from this layer. So with two interfaces UP a DNS query
 *    may leave the one netmgr did not select, and can time out or resolve against
 *    the wrong network's DNS server even though the connection that follows does
 *    leave the right interface. A resolve failure while the link itself is healthy
 *    is the symptom to expect.
 *  - tal_net_send_to() and connectionless UDP in general: there is no connect() to
 *    hook, and pinning a source address on a datagram socket would break the
 *    broadcast paths that bind TY_IPADDR_ANY on purpose (LAN discovery does
 *    exactly that). Callers needing a specific source on a datagram socket must
 *    call tal_net_bind() themselves.
 *  - tal_net_connect_raw(): the destination is an opaque platform sockaddr that
 *    this layer cannot portably inspect.
 *  - Server sockets: bind/listen/accept are untouched. Binding an inbound socket
 *    to one interface's unicast address would stop it serving the others.
 *  - Linux host builds: skipped entirely, the host's own routing decides.
 */

/**
 * @brief Get error code of network
 *
 * @param void
 *
 * @note This API is used for getting error code of network.
 *
 * @return UNW_SUCCESS on success. Others on error, please refer to
 * tuya_os_adapter_error_code.h
 */
TUYA_ERRNO tal_net_get_errno(void);

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
OPERATE_RET tal_net_fd_set(int fd, TUYA_FD_SET_T *fds);

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
OPERATE_RET tal_net_fd_clear(int fd, TUYA_FD_SET_T *fds);

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
OPERATE_RET tal_net_fd_isset(int fd, TUYA_FD_SET_T *fds);

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
OPERATE_RET tal_net_fd_zero(TUYA_FD_SET_T *fds);

// Add file descriptor to set
#define TAL_FD_SET(n, p) tal_net_fd_set(n, p)
// Clear file descriptor from set
#define TAL_FD_CLR(n, p) tal_net_fd_clear(n, p)
// Check file descriptor is in set
#define TAL_FD_ISSET(n, p) tal_net_fd_isset(n, p)
// Clear all descriptor in set
#define TAL_FD_ZERO(p) tal_net_fd_zero(p)

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
 * @return >0 the count of available file descriptors, <=0 error.
 */
int tal_net_select(const int maxfd, TUYA_FD_SET_T *readfds, TUYA_FD_SET_T *writefds, TUYA_FD_SET_T *errorfds,
                   const uint32_t ms_timeout);

/**
 * @brief Get no block file descriptors
 *
 * @param[in] fd: file descriptor
 *
 * @note This API is used to get no block file descriptors.
 *
 * @return >0 the count of no block file descriptors, <=0 error.
 */
int tal_net_get_nonblock(const int fd);

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
OPERATE_RET tal_net_set_block(const int fd, const BOOL_T block);

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
TUYA_ERRNO tal_net_close(const int fd);

/**
 * @brief Disable send and/or receive on a socket
 *
 * @param[in] fd: file descriptor
 * @param[in] how: 0 to shut down receive, 1 to shut down send, 2 for both
 *
 * @note This API is used to shut down part of a full-duplex connection while
 * the file descriptor stays open.
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
TUYA_ERRNO tal_net_shutdown(const int fd, const int how);

/**
 * @brief Create a tcp/udp socket
 *
 * @param[in] type: protocol type, tcp or udp
 *
 * @note This API is used for creating a tcp/udp socket.
 *
 * @return file descriptor
 */
int tal_net_socket_create(const TUYA_PROTOCOL_TYPE_E type);

/**
 * @brief Connect to network
 *
 * @param[in] fd: file descriptor
 * @param[in] addr: address information of server
 * @param[in] port: port information of server
 *
 * @note This API is used for connecting to network.
 *
 * @note Unless @a fd is already bound, it is bound here to the source address of
 * the active link so the flow leaves the interface netmgr selected. A caller that
 * wants a specific local address or port must call tal_net_bind() before this and
 * that binding is kept. See "Active-link source address binding" above.
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
TUYA_ERRNO tal_net_connect(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port);

/**
 * @brief Connect to network with raw data
 *
 * @param[in] fd: file descriptor
 * @param[in] p_socket: raw socket data
 * @param[in] len: data lenth
 *
 * @note This API is used for connecting to network with raw data.
 *
 * @note Outside the active-link binding described above: the destination is an
 * opaque platform sockaddr this layer cannot inspect, so no source binding is
 * applied. Call tal_net_bind() first if the source matters.
 *
 * @return 0 on success. Others on error, please refer to the error no of the
 * target system
 */
TUYA_ERRNO tal_net_connect_raw(const int fd, void *p_socket, const int len);

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
TUYA_ERRNO tal_net_bind(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port);

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
TUYA_ERRNO tal_net_listen(const int fd, const int backlog);

/**
 * @brief Send data to network
 *
 * @param[in] fd: file descriptor
 * @param[in] buf: send data buffer
 * @param[in] nbytes: buffer lenth
 *
 * @note This API is used for sending data to network
 *
 * @return >0 on num of send, <0 please refer to the error no of the target
 * system
 */
TUYA_ERRNO tal_net_send(const int fd, const void *buf, const uint32_t nbytes);

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
 * @note No active-link source binding is applied on this path, by design: there
 * is no connect() to hook and pinning a source address would break the broadcast
 * senders that bind TY_IPADDR_ANY on purpose. On a multi-interface target the
 * datagram leaves whichever interface the stack routes it to unless the caller
 * bound the socket itself. See "Active-link source address binding" above.
 *
 * @return >0 on num of send, <0 please refer to the error no of the target
 * system
 */
TUYA_ERRNO tal_net_send_to(const int fd, const void *buf, const uint32_t nbytes, const TUYA_IP_ADDR_T addr,
                           const uint16_t port);

/**
 * @brief Accept the coming socket connection of the server fd
 *
 * @param[in] fd: file descriptor
 * @param[in] addr: address information of server
 * @param[in] port: port information of server
 *
 * @note This API is used for accepting the coming socket connection of the
 * server fd.
 *
 * @return >0 the file descriptor, <=0 means failed
 */
int tal_net_accept(const int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);

/**
 * @brief Receive data from network
 *
 * @param[in] fd: file descriptor
 * @param[in] buf: receive data buffer
 * @param[in] nbytes: buffer lenth
 *
 * @note This API is used for receiving data from network
 *
 * @return >0 on num of recv, <0 please refer to the error no of the target
 * system
 */
TUYA_ERRNO tal_net_recv(const int fd, void *buf, const uint32_t nbytes);

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
int tal_net_recv_nd_size(const int fd, void *buf, const uint32_t buf_size, const uint32_t nd_size);

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
 * @return >0 on num of recv, <0 please refer to the error no of the target
 * system
 */
TUYA_ERRNO tal_net_recvfrom(const int fd, void *buf, const uint32_t nbytes, TUYA_IP_ADDR_T *addr, uint16_t *port);

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
OPERATE_RET tal_net_set_timeout(const int fd, const int ms_timeout, const TUYA_TRANS_TYPE_E type);

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
OPERATE_RET tal_net_set_bufsize(const int fd, const int buf_size, const TUYA_TRANS_TYPE_E type);

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
OPERATE_RET tal_net_set_reuse(const int fd);

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
OPERATE_RET tal_net_disable_nagle(const int fd);

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
OPERATE_RET tal_net_set_broadcast(const int fd);

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
OPERATE_RET tal_net_gethostbyname(const char *domain, TUYA_IP_ADDR_T *addr);

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
                                  const uint32_t cnt);

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
OPERATE_RET tal_net_get_socket_ip(int fd, TUYA_IP_ADDR_T *addr);

/**
 * @brief Get the local address a socket fd is bound to
 *
 * @param[in] fd: file descriptor
 * @param[out] addr: local ip address in host byte order, may be NULL
 * @param[out] port: local port in host byte order, may be NULL
 *
 * @note Unlike tal_net_get_socket_ip() this also reports the port, which is
 * the only way to learn the port the stack picked when binding to port 0.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_getsockname(int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);

/**
 * @brief Get the address of the peer connected to a socket fd
 *
 * @param[in] fd: file descriptor
 * @param[out] addr: peer ip address in host byte order, may be NULL
 * @param[out] port: peer port in host byte order, may be NULL
 *
 * @note This API is used for getting the remote name of a connected socket.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_net_getpeername(int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);

/**
 * @brief Change ip string to address
 *
 * @param[in] ip_str: ip string
 *
 * @note This API is used to change ip string to address.
 *
 * @return ip address
 */
TUYA_IP_ADDR_T tal_net_str2addr(const char *ip_str);

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
char *tal_net_addr2str(TUYA_IP_ADDR_T ipaddr);

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
                               const void *optval, const int optlen);

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
                               int *optlen);

#ifdef __cplusplus
}
#endif

#endif // __TAL_NETWORK_H__
