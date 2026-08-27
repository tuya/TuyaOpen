/**
 * @file tal_network.c
 * @brief Implementation of tal_network.h. Every function below is a thin
 *        dispatch onto the platform's TAL_NETWORK_OPS_T (TAL_NET_EXEC_OP /
 *        TAL_NET_EXEC_OP_VOID below) - the contract for each one lives in
 *        the header, not here. What this file documents instead: the ops
 *        dispatch itself, active-link source address binding's
 *        implementation (__net_connect_bind_active_src()), the getsockname
 *        probe that binding depends on (__net_getsockname_trustworthy()),
 *        and the ULP notifications wrapped around connect() and DNS lookup.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */
#include "tuya_iot_config.h"
#include "tal_api.h"

#include "tal_net_provider.h"
#include "tal_net_route.h"
#include "dev_evt.h"

/***********************************************************
************************macro define************************
***********************************************************/

/**
 * @brief Every socket primitive below routes through this: look up the
 *        platform's TAL_NETWORK_OPS_T and call the matching member if one
 *        was registered, returning its result directly. One macro instead
 *        of repeating the lookup/NULL-check/call in each function - and a
 *        hot path, since every send/recv/connect goes through it.
 *
 * @param op_name: member of TAL_NETWORK_OPS_T to call
 * @param default_ret: returned when the platform never registered @a op_name
 * @param ...: arguments forwarded to the operation
 */
#define TAL_NET_EXEC_OP(op_name, default_ret, ...)                                                                     \
    do {                                                                                                               \
        TAL_NETWORK_OPS_T *ops = tal_net_provider_ops();                                                               \
        if (NULL != ops && ops->op_name) {                                                                             \
            return ops->op_name(__VA_ARGS__);                                                                          \
        }                                                                                                              \
        PR_ERR("Network operation %s not available", #op_name);                                                        \
        return default_ret;                                                                                            \
    } while (0)

/**
 * @brief TUYA_IP_ADDR_T is a plain uint32_t IPv4 address (host byte order)
 *        on every platform in the build matrix except the IPv6-capable
 *        variant, where it is a struct - and that variant also ships this
 *        accessor, so use theirs when it exists and fall back to the
 *        integer form otherwise.
 */
#ifndef TUYA_IP_ADDR_GET_IP4
#define TUYA_IP_ADDR_GET_IP4(addr) (addr)
#endif

/** @brief Test an IPv4 address in host byte order for 127.0.0.0/8. */
#define TAL_NET_IP4_IS_LOOPBACK(v) ((((uint32_t)(v)) >> 24) == 127)

/** @brief TAL_NET_EXEC_OP()'s twin for an operation with no return value -
 *         same dispatch, nothing to give back when @a op_name is missing. */
#define TAL_NET_EXEC_OP_VOID(op_name, ...)                                                                             \
    do {                                                                                                               \
        TAL_NETWORK_OPS_T *ops = tal_net_provider_ops();                                                               \
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

TUYA_ERRNO tal_net_get_errno(void)
{
    TAL_NET_EXEC_OP(get_errno, -100);
}

OPERATE_RET tal_net_fd_set(int fd, TUYA_FD_SET_T *fds)
{
    if ((fd < 0) || (fds == NULL)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(fd_set, OPRT_COM_ERROR, fd, fds);
}

OPERATE_RET tal_net_fd_clear(int fd, TUYA_FD_SET_T *fds)
{
    if ((fd < 0) || (fds == NULL)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(fd_clear, OPRT_COM_ERROR, fd, fds);
}

OPERATE_RET tal_net_fd_isset(int fd, TUYA_FD_SET_T *fds)
{
    if ((fd < 0) || (fds == NULL)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(fd_isset, OPRT_COM_ERROR, fd, fds);
}

OPERATE_RET tal_net_fd_zero(TUYA_FD_SET_T *fds)
{
    if (fds == NULL) {
        return -1;
    }

    TAL_NET_EXEC_OP(fd_zero, OPRT_COM_ERROR, fds);
}

int tal_net_select(const int maxfd, TUYA_FD_SET_T *readfds, TUYA_FD_SET_T *writefds, TUYA_FD_SET_T *errorfds,
                   const uint32_t ms_timeout)
{
    TAL_NET_EXEC_OP(select, -1, maxfd, readfds, writefds, errorfds, ms_timeout);
}

int tal_net_get_nonblock(const int fd)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(get_nonblock, 0, fd);
}

OPERATE_RET tal_net_set_block(const int fd, const BOOL_T block)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(set_block, OPRT_COM_ERROR, fd, block);
}

TUYA_ERRNO tal_net_close(const int fd)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(close, -1, fd);
}

TUYA_ERRNO tal_net_shutdown(const int fd, const int how)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(shutdown, -1, fd, how);
}

int tal_net_socket_create(const TUYA_PROTOCOL_TYPE_E type)
{
    TAL_NET_EXEC_OP(socket_create, -1, type);
}

#if OPERATING_SYSTEM != SYSTEM_LINUX
/**
 * @brief Whether this platform's tkl_net_getsockname() can be believed.
 *
 * The check below treats "reports 0.0.0.0:0" as "not bound yet, safe to
 * bind". A stub that returns success without writing its outputs produces
 * exactly that answer for EVERY socket, including one the caller bound
 * itself - turning the guard into the unconditional bind it replaced and
 * silently moving the caller's own address. platform/T3 ships such a stub
 * (`return 0;`), and it cannot be fixed from here: the per-platform
 * directories are gitignored and carry no tracked files.
 *
 * So the implementation is measured once instead of assumed, with the same
 * property the gate itself relies on: bind(ANY, 0) is a real bind and the
 * stack assigns an ephemeral port, so a working getsockname MUST report a
 * non-zero port afterwards. No stub can.
 *
 * Undecided is not cached: a socket-create failure here is a transient
 * condition (out of descriptors at start-up, say), not evidence about the
 * implementation, so the probe stays pending and the caller simply does not
 * bind this time.
 *
 * @return TRUE when the answer can be trusted, FALSE while it cannot
 */
static BOOL_T __net_getsockname_trustworthy(void)
{
    /* 0 unknown, 1 trusted, 2 distrusted. Written once, read on every connect. */
    static uint8_t s_gsn_verdict = 0;

    int            fd    = -1;
    TUYA_IP_ADDR_T addr  = 0;
    uint16_t       port  = 0;
    BOOL_T         works = FALSE;

    if (0 != s_gsn_verdict) {
        return (1 == s_gsn_verdict);
    }

    fd = tal_net_socket_create(PROTOCOL_UDP);
    if (fd < 0) {
        return FALSE;
    }

    if ((0 == tal_net_bind(fd, TY_IPADDR_ANY, 0)) && (OPRT_OK == tal_net_getsockname(fd, &addr, &port)) &&
        (0 != port)) {
        works = TRUE;
    }

    tal_net_close(fd);

    s_gsn_verdict = works ? 1 : 2;
    if (!works) {
        PR_NOTICE("tkl_net_getsockname unusable here, active-link source binding disabled");
    }

    return works;
}

/**
 * @brief Bind an about-to-connect socket to the source address of the
 *        active link, so the flow leaves the interface netmgr selected on a
 *        target with more than one interface UP at once (Wi-Fi and cellular,
 *        say) - one routing table, no policy routing. Used to live in
 *        tcp_transporter for a single caller; here it covers every outbound
 *        connect() in the SDK.
 *
 * Why connect() and not tal_net_socket_create(): a freshly created socket
 * has no direction yet - it may still become a listener - and binding it to
 * a unicast address there would break every server socket.
 *
 * Why only when nothing has bound the socket yet: a caller that manages its
 * own local address must win. pjproject (ICE/STUN/TURN) binds every socket
 * itself to gather candidates, and on lwIP a second bind of a still-CLOSED
 * pcb succeeds and silently moves the local address (tcp_bind() only rejects
 * a pcb that has left CLOSED) - an unconditional bind here would quietly
 * corrupt candidate gathering instead of failing loudly.
 *
 * Why a failed bind is not fatal: falling back to the stack's own source
 * choice still connects on a single-interface device; failing the connect
 * instead would turn a routing preference into an outage.
 *
 * Compiled out on Linux host builds - see the call site in tal_net_connect().
 */
static void __net_connect_bind_active_src(const int fd, const TUYA_IP_ADDR_T dst)
{
    TUYA_IP_ADDR_T src        = tal_net_route_src_ip();
    TUYA_IP_ADDR_T local      = 0;
    uint16_t       local_port = 0;

    /* No link address known yet (nothing up, or the provider never reported
     * one) - leave the socket unbound and let the stack decide. */
    if (0 == TUYA_IP_ADDR_GET_IP4(src)) {
        return;
    }

    /* A loopback peer is only reachable from a loopback source; pinning the
     * link address would make the connect fail outright. */
    if (TAL_NET_IP4_IS_LOOPBACK(TUYA_IP_ADDR_GET_IP4(dst))) {
        return;
    }

    /* A platform whose getsockname cannot be believed makes the "already
     * bound" probe below meaningless, and acting on a meaningless answer is
     * what would destroy a caller's own bind on T3 - so trust is checked
     * before the probe runs. local/local_port stay pre-zeroed above so that
     * even a stub answering OPRT_OK without writing them reads as "unbound"
     * and falls through to the always-bind behaviour this logic replaces,
     * rather than branching on stack garbage. */
    if (!__net_getsockname_trustworthy()) {
        return;
    }

    /* Treat both "probe failed" and "already bound" as hands off - an
     * unreliable answer must not cost a working P2P session. The port is
     * part of the test because bind(ANY, 0) is a real bind: lwIP assigns the
     * ephemeral port at bind time, so an ANY-bound socket reports address 0
     * with a non-zero port, and checking the address alone would misread it
     * as unbound. */
    if (OPRT_OK != tal_net_getsockname(fd, &local, &local_port)) {
        return;
    }
    if ((0 != TUYA_IP_ADDR_GET_IP4(local)) || (0 != local_port)) {
        return;
    }

    if (0 != tal_net_bind(fd, src, 0)) {
        PR_WARN("bind fd %d to active src %s failed, errno %d, falling back to stack routing", fd,
                tal_net_addr2str(src), tal_net_get_errno());
    }
}
#endif // OPERATING_SYSTEM != SYSTEM_LINUX

TUYA_ERRNO tal_net_connect(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    /* Notify ULP around the TCP connect so it can keep the device awake for the
     * duration (covers TLS/HTTP/MQTT/ATOP socket setup - they all land here). */
    TUYA_ERRNO rt = -1;
    TAL_NETWORK_OPS_T *ops = tal_net_provider_ops();
    if (NULL != ops && ops->connect) {
#if OPERATING_SYSTEM != SYSTEM_LINUX
        /* Linux host builds keep the host routing table: it has policy rules,
         * loopback and interfaces this SDK knows nothing about, and source-binding
         * there breaks more than it fixes. Embedded targets are the ones with a
         * single routing table and no way to express the choice otherwise. */
        __net_connect_bind_active_src(fd, addr);
#endif
        tuya_dev_evt_notify(DEV_EVT_TCP_CONNECT, ACTION_BEFORE, NULL);
        rt = ops->connect(fd, addr, port);
        tuya_dev_evt_notify(DEV_EVT_TCP_CONNECT, ACTION_AFTER, NULL);
        return rt;
    }
    PR_ERR("Network operation connect not available");
    return rt;
}

TUYA_ERRNO tal_net_connect_raw(const int fd, void *p_socket_addr, const int len)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(connect_raw, -1, fd, p_socket_addr, len);
}

TUYA_ERRNO tal_net_bind(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(bind, -1, fd, addr, port);
}

TUYA_ERRNO tal_net_listen(const int fd, const int backlog)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(listen, -1, fd, backlog);
}

int tal_net_accept(const int fd, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(accept, -1, fd, addr, port);
}

TUYA_ERRNO tal_net_send(const int fd, const void *buf, const uint32_t nbytes)
{
    if ((fd < 0) || (buf == NULL) || (nbytes == 0)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(send, -1, fd, buf, nbytes);
}

TUYA_ERRNO tal_net_send_to(const int fd, const void *buf, const uint32_t nbytes, const TUYA_IP_ADDR_T addr,
                           const uint16_t port)
{
    if ((fd < 0) || (buf == NULL) || (nbytes == 0)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(send_to, -1, fd, buf, nbytes, addr, port);
}

TUYA_ERRNO tal_net_recv(const int fd, void *buf, const uint32_t nbytes)
{
    if ((fd < 0) || (buf == NULL) || (nbytes == 0)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(recv, -1, fd, buf, nbytes);
}

int tal_net_recv_nd_size(const int fd, void *buf, const uint32_t buf_size, const uint32_t nd_size)
{
    if ((fd < 0) || (NULL == buf) || (buf_size == 0) || (nd_size == 0) || (buf_size < nd_size)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(recv_nd_size, -1, fd, buf, buf_size, nd_size);
}

TUYA_ERRNO tal_net_recvfrom(const int fd, void *buf, const uint32_t nbytes, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    if ((fd < 0) || (buf == NULL) || (nbytes == 0)) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(recvfrom, -1, fd, buf, nbytes, addr, port);
}

OPERATE_RET tal_net_setsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname,
                               const void *optval, const int optlen)
{
    TAL_NET_EXEC_OP(setsockopt, OPRT_COM_ERROR, fd, level, optname, optval, optlen);
}

OPERATE_RET tal_net_getsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname, void *optval,
                               int *optlen)
{
    TAL_NET_EXEC_OP(getsockopt, OPRT_COM_ERROR, fd, level, optname, optval, optlen);
}

OPERATE_RET tal_net_set_timeout(const int fd, const int ms_timeout, const TUYA_TRANS_TYPE_E type)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(set_timeout, OPRT_COM_ERROR, fd, ms_timeout, type);
}

OPERATE_RET tal_net_set_bufsize(const int fd, const int buf_size, const TUYA_TRANS_TYPE_E type)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(set_bufsize, OPRT_COM_ERROR, fd, buf_size, type);
}

OPERATE_RET tal_net_set_reuse(const int fd)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(set_reuse, OPRT_COM_ERROR, fd);
}

OPERATE_RET tal_net_disable_nagle(const int fd)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(disable_nagle, OPRT_COM_ERROR, fd);
}

OPERATE_RET tal_net_set_broadcast(const int fd)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(set_broadcast, OPRT_COM_ERROR, fd);
}

OPERATE_RET tal_net_gethostbyname(const char *domain, TUYA_IP_ADDR_T *addr)
{
    if ((domain == NULL) || (addr == NULL)) {
        return -2;
    }

    /* Notify ULP around the DNS lookup so it can keep the device awake for it. */
    TAL_NETWORK_OPS_T *dns_ops = tal_net_provider_ops();
    if (NULL != dns_ops && dns_ops->gethostbyname) {
        OPERATE_RET rt = OPRT_OK;
        tuya_dev_evt_notify(DEV_EVT_DNS_LOOKUP, ACTION_BEFORE, NULL);
        rt = dns_ops->gethostbyname(domain, addr);
        tuya_dev_evt_notify(DEV_EVT_DNS_LOOKUP, ACTION_AFTER, NULL);
        return rt;
    }

    TAL_NET_EXEC_OP(gethostbyname, OPRT_COM_ERROR, domain, addr);
}

OPERATE_RET tal_net_set_keepalive(int fd, const BOOL_T alive, const uint32_t idle, const uint32_t intr,
                                  const uint32_t cnt)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(set_keepalive, OPRT_COM_ERROR, fd, alive, idle, intr, cnt);
}

OPERATE_RET tal_net_get_socket_ip(int fd, TUYA_IP_ADDR_T *addr)
{
    TAL_NET_EXEC_OP(get_socket_ip, OPRT_COM_ERROR, fd, addr);
}

OPERATE_RET tal_net_getsockname(int fd, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(getsockname, OPRT_COM_ERROR, fd, addr, port);
}

OPERATE_RET tal_net_getpeername(int fd, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    if (fd < 0) {
        return -3000 + fd;
    }

    TAL_NET_EXEC_OP(getpeername, OPRT_COM_ERROR, fd, addr, port);
}

TUYA_IP_ADDR_T tal_net_str2addr(const char *ip_str)
{
    TAL_NET_EXEC_OP(str2addr, 0, ip_str);
}

char *tal_net_addr2str(TUYA_IP_ADDR_T ipaddr)
{
    TAL_NET_EXEC_OP(addr2str, NULL, ipaddr);
}
