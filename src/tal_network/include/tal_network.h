/**
 * @file tal_network.h
 * @brief POSIX-shaped socket interface for Tuya SDK: send/recv/bind/listen/
 *        accept/select and friends. Most of it needs no more explanation than
 *        the POSIX call it mirrors, so this file documents only where
 *        TuyaOpen deviates: TUYA_ERRNO's convention (see
 *        tal_net_get_errno()), active-link source address binding (below),
 *        tal_net_select()'s millisecond timeout, tal_net_recv_nd_size() (not
 *        POSIX at all), and tal_net_getsockname()'s per-platform reliability.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
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
 * A target with more than one interface UP at once (Wi-Fi and cellular, say)
 * has one routing table and no policy routing, so a flow leaves the interface
 * netmgr selected only if its socket is bound to that interface's address.
 * tal_network does this automatically for outbound connections, but not
 * everywhere - the gaps below are real, not oversights.
 *
 * Bound automatically:
 *  - everything through tal_net_connect(): every outbound TCP flow in the SDK
 *    (TLS, HTTP, MQTT, ATOP) and any UDP socket the caller connect()s. A
 *    socket the caller already bound is left alone, and a failed bind is
 *    logged and ignored rather than failing the connect.
 *
 * NOT bound:
 *  - tal_net_gethostbyname(): resolution runs inside lwIP or the platform
 *    resolver, with no socket to bind - a DNS query can leave the inactive
 *    interface even though the connection that follows leaves the right one;
 *  - tal_net_send_to() and connectionless UDP in general: there is no
 *    connect() to hook, and pinning a source would break the broadcast paths
 *    that bind TY_IPADDR_ANY on purpose. Call tal_net_bind() first if the
 *    source matters;
 *  - tal_net_connect_raw(): the destination is an opaque platform sockaddr
 *    this layer cannot inspect;
 *  - server sockets: bind/listen/accept are untouched;
 *  - Linux host builds: skipped entirely, the host's own routing decides.
 */

/**
 * @brief The last network error, in TuyaOpen's own error space.
 *
 * Not POSIX errno: there is no global variable to read, and the values are
 * this SDK's own UNW_* constants (tuya_cloud_types.h - UNW_EAGAIN, UNW_EINTR,
 * ...), which do not share POSIX's numeric assignments. Compare against
 * UNW_*, never against <errno.h>'s EAGAIN/EINTR.
 *
 * When to call it: a function typed TUYA_ERRNO that returns a plain 0-or-
 * error (tal_net_close(), tal_net_connect(), tal_net_bind(), ...) already
 * carries the code in its own return value. A function that returns a byte
 * count or a file descriptor (tal_net_send(), tal_net_recv(),
 * tal_net_accept(), ...) does not - call this right after it fails to learn
 * why, the same shape as checking errno after a POSIX call, minus the global.
 *
 * @return UNW_SUCCESS on success, else an UNW_* code.
 */
TUYA_ERRNO tal_net_get_errno(void);

/** @brief POSIX FD_SET(), backed by TUYA_FD_SET_T. See TAL_FD_SET() below.
 *  @return OPRT_OK on success, else tuya_error_code.h. */
OPERATE_RET tal_net_fd_set(int fd, TUYA_FD_SET_T *fds);

/** @brief POSIX FD_CLR(). See TAL_FD_CLR() below.
 *  @return OPRT_OK on success, else tuya_error_code.h. */
OPERATE_RET tal_net_fd_clear(int fd, TUYA_FD_SET_T *fds);

/** @brief POSIX FD_ISSET(). See TAL_FD_ISSET() below. @return TRUE or FALSE */
OPERATE_RET tal_net_fd_isset(int fd, TUYA_FD_SET_T *fds);

/** @brief POSIX FD_ZERO(). See TAL_FD_ZERO() below.
 *  @return OPRT_OK on success, else tuya_error_code.h. */
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
 * @brief POSIX select(), with one difference: @a ms_timeout is a plain
 *        millisecond count, not a struct timeval.
 *
 * @return >0 the count of available file descriptors, <=0 error.
 */
int tal_net_select(const int maxfd, TUYA_FD_SET_T *readfds, TUYA_FD_SET_T *writefds, TUYA_FD_SET_T *errorfds,
                   const uint32_t ms_timeout);

/** @brief Whether @a fd is currently in non-blocking mode.
 *  @return >0 the count of non-block descriptors, <=0 error. */
int tal_net_get_nonblock(const int fd);

/** @brief Set or clear non-blocking mode on @a fd.
 *  @return OPRT_OK on success, else tuya_error_code.h. */
OPERATE_RET tal_net_set_block(const int fd, const BOOL_T block);

/** @brief POSIX close(). @return 0 on success, else the target's error no. */
TUYA_ERRNO tal_net_close(const int fd);

/**
 * @brief POSIX shutdown(): disable send and/or receive while @a fd stays
 *        open. @a how: 0 = receive, 1 = send, 2 = both.
 * @return 0 on success, else the target's error no.
 */
TUYA_ERRNO tal_net_shutdown(const int fd, const int how);

/** @brief POSIX socket(), collapsed to TCP-or-UDP; family/protocol are fixed
 *         by the platform layer. @return the file descriptor */
int tal_net_socket_create(const TUYA_PROTOCOL_TYPE_E type);

/**
 * @brief POSIX connect().
 *
 * @note Unless @a fd is already bound, it is bound here to the source
 * address of the active link so the flow leaves the interface netmgr
 * selected. An explicit tal_net_bind() before this call always wins. See
 * "Active-link source address binding" above.
 *
 * @return 0 on success, else the target's error no.
 */
TUYA_ERRNO tal_net_connect(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port);

/**
 * @brief connect() taking a raw, already-built platform sockaddr instead of
 *        a TUYA_IP_ADDR_T + port pair. Not part of POSIX.
 *
 * @note No active-link source binding: the destination is an opaque platform
 * sockaddr this layer cannot inspect. Call tal_net_bind() first if the
 * source matters.
 *
 * @return 0 on success, else the target's error no.
 */
TUYA_ERRNO tal_net_connect_raw(const int fd, void *p_socket, const int len);

/** @brief POSIX bind(). @return 0 on success, else the target's error no. */
TUYA_ERRNO tal_net_bind(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port);

/** @brief POSIX listen(). @return 0 on success, else the target's error no. */
TUYA_ERRNO tal_net_listen(const int fd, const int backlog);

/**
 * @brief POSIX send().
 * @return >0 on num of send, <0 please refer to the error no of the target
 * system
 */
TUYA_ERRNO tal_net_send(const int fd, const void *buf, const uint32_t nbytes);

/**
 * @brief POSIX sendto().
 *
 * @note No active-link source binding, by design: there is no connect() to
 * hook, and pinning a source would break the broadcast senders that bind
 * TY_IPADDR_ANY on purpose. See "Active-link source address binding" above.
 *
 * @return >0 on num of send, <0 please refer to the error no of the target
 * system
 */
TUYA_ERRNO tal_net_send_to(const int fd, const void *buf, const uint32_t nbytes, const TUYA_IP_ADDR_T addr,
                           const uint16_t port);

/** @brief POSIX accept(). @return >0 the file descriptor, <=0 means failed */
int tal_net_accept(const int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);

/**
 * @brief POSIX recv().
 * @return >0 on num of recv, <0 please refer to the error no of the target
 * system
 */
TUYA_ERRNO tal_net_recv(const int fd, void *buf, const uint32_t nbytes);

/**
 * @brief Not POSIX: loops internally (retrying on a transient error, sleeping
 *        between attempts) until exactly @a nd_size bytes have been read into
 *        @a buf, instead of returning as soon as any data is available the
 *        way tal_net_recv() does. The closest POSIX equivalent is a caller-
 *        side read loop, or MSG_WAITALL where the platform's socket layer
 *        supports it.
 *
 * @param[in] buf_size: capacity of @a buf; must be >= @a nd_size
 * @param[in] nd_size: bytes to read before returning
 *
 * @return >0 on success. Others on error
 */
int tal_net_recv_nd_size(const int fd, void *buf, const uint32_t buf_size, const uint32_t nd_size);

/**
 * @brief POSIX recvfrom().
 * @return >0 on num of recv, <0 please refer to the error no of the target
 * system
 */
TUYA_ERRNO tal_net_recvfrom(const int fd, void *buf, const uint32_t nbytes, TUYA_IP_ADDR_T *addr, uint16_t *port);

/** @brief SO_RCVTIMEO/SO_SNDTIMEO, selected by @a type instead of a separate
 *         setsockopt() per direction.
 *  @return OPRT_OK on success, else tuya_error_code.h. */
OPERATE_RET tal_net_set_timeout(const int fd, const int ms_timeout, const TUYA_TRANS_TYPE_E type);

/** @brief SO_RCVBUF/SO_SNDBUF, selected by @a type.
 *  @return OPRT_OK on success, else tuya_error_code.h. */
OPERATE_RET tal_net_set_bufsize(const int fd, const int buf_size, const TUYA_TRANS_TYPE_E type);

/** @brief SO_REUSEADDR. @return OPRT_OK on success, else tuya_error_code.h. */
OPERATE_RET tal_net_set_reuse(const int fd);

/** @brief TCP_NODELAY. @return OPRT_OK on success, else tuya_error_code.h. */
OPERATE_RET tal_net_disable_nagle(const int fd);

/** @brief SO_BROADCAST. @return OPRT_OK on success, else tuya_error_code.h. */
OPERATE_RET tal_net_set_broadcast(const int fd);

/** @brief POSIX gethostbyname(), collapsed to the one address a caller here
 *         ever wants instead of a full hostent list.
 *  @return OPRT_OK on success, else tuya_error_code.h. */
OPERATE_RET tal_net_gethostbyname(const char *domain, TUYA_IP_ADDR_T *addr);

/**
 * @brief SO_KEEPALIVE plus TCP_KEEPIDLE/TCP_KEEPINTVL/TCP_KEEPCNT in one call
 *        instead of four separate setsockopt()s.
 *
 * @param[in] idle: seconds of silence before the first probe
 * @param[in] intr: seconds between probes
 * @param[in] cnt: probes sent before the connection is declared dead
 *
 * @return OPRT_OK on success, else tuya_error_code.h.
 */
OPERATE_RET tal_net_set_keepalive(int fd, const BOOL_T alive, const uint32_t idle, const uint32_t intr,
                                  const uint32_t cnt);

/** @brief The local address @a fd is bound to, without the port - see
 *         tal_net_getsockname() for both.
 *  @return OPRT_OK on success, else tuya_error_code.h. */
OPERATE_RET tal_net_get_socket_ip(int fd, TUYA_IP_ADDR_T *addr);

/**
 * @brief POSIX getsockname(). Unlike tal_net_get_socket_ip() this also
 *        reports the port, the only way to learn the port the stack picked
 *        when binding to port 0.
 *
 * @note Trust this return value, not just its OPRT_OK: commit b9c931b5 found
 * a platform implementation that answers success without writing @a addr or
 * @a port at all, which is why the source-binding logic in tal_net_connect()
 * runs a one-time probe before ever trusting this call. A caller reading the
 * output directly has no such probe and should treat OPRT_OK plus an
 * all-zero @a addr and @a port with suspicion on an unfamiliar platform.
 *
 * @param[out] addr: local ip address in host byte order, may be NULL
 * @param[out] port: local port in host byte order, may be NULL
 *
 * @return OPRT_OK on success, else tuya_error_code.h.
 */
OPERATE_RET tal_net_getsockname(int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);

/** @brief POSIX getpeername(). @return OPRT_OK on success, else
 *         tuya_error_code.h. */
OPERATE_RET tal_net_getpeername(int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);

/** @brief POSIX inet_addr()/inet_pton(AF_INET, ...), IPv4 only.
 *  @return ip address */
TUYA_IP_ADDR_T tal_net_str2addr(const char *ip_str);

/** @brief POSIX inet_ntoa(), IPv4 only. @return ip string */
char *tal_net_addr2str(TUYA_IP_ADDR_T ipaddr);

/** @brief POSIX setsockopt(). @return OPRT_OK on success, else
 *         tuya_error_code.h. */
OPERATE_RET tal_net_setsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname,
                               const void *optval, const int optlen);

/** @brief POSIX getsockopt(). @return OPRT_OK on success, else
 *         tuya_error_code.h. */
OPERATE_RET tal_net_getsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname, void *optval,
                               int *optlen);

#ifdef __cplusplus
}
#endif

#endif // __TAL_NETWORK_H__
