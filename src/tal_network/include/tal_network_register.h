/**
 * @file tal_network_register.h
 * @brief tal_network_register module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TAL_NETWORK_REGISTER_H__
#define __TAL_NETWORK_REGISTER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_ERRNO (*get_errno)(void);
    OPERATE_RET (*fd_set)(int fd, TUYA_FD_SET_T *fds);
    OPERATE_RET (*fd_clear)(int fd, TUYA_FD_SET_T *fds);
    OPERATE_RET (*fd_isset)(int fd, TUYA_FD_SET_T *fds);
    OPERATE_RET (*fd_zero)(TUYA_FD_SET_T *fds);
    int (*select)(const int maxfd, TUYA_FD_SET_T *readfds, TUYA_FD_SET_T *writefds, TUYA_FD_SET_T *errorfds,
                  const uint32_t ms_timeout);
    int (*get_nonblock)(const int fd);
    OPERATE_RET (*set_block)(const int fd, const BOOL_T block);
    TUYA_ERRNO (*close)(const int fd);
    TUYA_ERRNO (*shutdown)(const int fd, const int how);
    int (*socket_create)(const TUYA_PROTOCOL_TYPE_E type);
    TUYA_ERRNO (*connect)(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port);
    TUYA_ERRNO (*connect_raw)(const int fd, void *p_socket, const int len);
    TUYA_ERRNO (*bind)(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port);
    TUYA_ERRNO (*listen)(const int fd, const int backlog);
    TUYA_ERRNO (*send)(const int fd, const void *buf, const uint32_t nbytes);
    TUYA_ERRNO (*send_to)(const int fd, const void *buf, const uint32_t nbytes, const TUYA_IP_ADDR_T addr,
                          const uint16_t port);
    int (*accept)(const int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);
    TUYA_ERRNO (*recv)(const int fd, void *buf, const uint32_t nbytes);
    int (*recv_nd_size)(const int fd, void *buf, const uint32_t buf_size, const uint32_t nd_size);
    TUYA_ERRNO (*recvfrom)(const int fd, void *buf, const uint32_t nbytes, TUYA_IP_ADDR_T *addr, uint16_t *port);
    OPERATE_RET (*set_timeout)(const int fd, const int ms_timeout, const TUYA_TRANS_TYPE_E type);
    OPERATE_RET (*set_bufsize)(const int fd, const int buf_size, const TUYA_TRANS_TYPE_E type);
    OPERATE_RET (*set_reuse)(const int fd);
    OPERATE_RET (*disable_nagle)(const int fd);
    OPERATE_RET (*set_broadcast)(const int fd);
    OPERATE_RET (*gethostbyname)(const char *domain, TUYA_IP_ADDR_T *addr);
    OPERATE_RET (*set_keepalive)(int fd, const BOOL_T alive, const uint32_t idle, const uint32_t intr,
                                 const uint32_t cnt);
    OPERATE_RET (*get_socket_ip)(int fd, TUYA_IP_ADDR_T *addr);
    OPERATE_RET (*getsockname)(int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);
    OPERATE_RET (*getpeername)(int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);
    TUYA_IP_ADDR_T (*str2addr)(const char *ip_str);
    char *(*addr2str)(TUYA_IP_ADDR_T ipaddr);
    OPERATE_RET (*setsockopt)(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname, const void *optval,
                              const int optlen);
    OPERATE_RET (*getsockopt)(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname, void *optval,
                              int *optlen);

} TAL_NETWORK_OPS_T;

/**
 * Which socket ops backend is meant. These are not network cards: POSIX is
 * tal_posix.c's lwip/socket implementation, TKL is tal_platform.c's tkl
 * implementation, and AT_MODEM is a placeholder that has a constant but no
 * implementation. The actual network interfaces - wifi, wired, cellular - live
 * in src/tuya_cloud_service/netmgr/netconn_*.
 *
 * Deliberately a typedef of uint8_t and not an enum: netmgr.h and
 * netconn_registry.h hold this value in a plain uint8_t field so that the
 * control-plane header does not have to include this data-plane one (1767f10b).
 */
typedef uint8_t tal_net_provider_id_t;
#define TAL_NET_PROVIDER_POSIX    (0)
#define TAL_NET_PROVIDER_TKL      (1)
#define TAL_NET_PROVIDER_AT_MODEM (2)
#define TAL_NET_PROVIDER_MAX      (3)

/**
 * The socket ops backend this build talks to: the lwip/socket layer when one is
 * linked in (or when we are hosted on Linux), the platform tkl layer otherwise.
 *
 * A build only ever links one of the two, so this is also the value every link
 * type ends up selecting - it is the answer for wifi, wired and cellular alike.
 * Named here rather than open-coded at each site so the whole tree agrees on one
 * definition of "the backend this build has".
 *
 * ENABLE_LIBLWIP and OPERATING_SYSTEM come from the build-generated
 * tuya_kconfig.h, which every platform's tuya_cloud_types.h pulls in through
 * tuya_iot_config.h - so the include above is enough to make them visible here.
 */
#if (defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)) || 100 == OPERATING_SYSTEM
#define TAL_NET_PROVIDER_DEFAULT TAL_NET_PROVIDER_POSIX
#else
#define TAL_NET_PROVIDER_DEFAULT TAL_NET_PROVIDER_TKL
#endif

typedef struct {
    char                  name[16];
    tal_net_provider_id_t type;
    TUYA_IP_ADDR_T        ipaddr;
    TAL_NETWORK_OPS_T     ops;
} tal_net_provider_t;

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tal_net_provider_init(void);

TAL_NETWORK_OPS_T *tal_net_provider_ops(void);

/**
 * @brief Get the source address outbound sockets should bind to.
 *
 * @return the active connection address, or 0 when unknown - callers must treat
 *         0 as "do not bind" and let the stack pick the source itself.
 */
TUYA_IP_ADDR_T tal_net_route_src_ip(void);

/***********************************************************
*******************deprecated aliases***********************
***********************************************************/
/*
 * The old names, kept so that out-of-tree callers keep compiling. All of them
 * are removed in S4 of docs/netmgr/provider_rename_plan.md; do not use them in
 * new code.
 *
 * The VALUES are unchanged on purpose. An alias disappearing is a loud compile
 * error that reaches whoever is affected; a constant changing meaning is a
 * silent behaviour change that does not.
 *
 * The function aliases are #define rather than forwarding wrappers, because
 * tal_net_provider_ops() sits on the hot path of every socket primitive in
 * tal_network.c and a wrapper would add a call to each one. The price is that
 * the symbol name in the .o changes, so an object compiled against the old
 * header will not link against the new library. TuyaOpen ships source and
 * rebuilds the whole tree, so that costs nothing here - but it is the one part
 * of this step that is not purely additive, and it belongs in the release note.
 */
typedef tal_net_provider_id_t TAL_NETWORK_CARD_TYPE_E
    __attribute__((deprecated("renamed to tal_net_provider_id_t")));
typedef tal_net_provider_t TAL_NETWORK_CARD_T __attribute__((deprecated("renamed to tal_net_provider_t")));

/* [deprecated-s4] no portable way to deprecate a macro; see tools/provider_rename.py */
#define TAL_NET_TYPE_POSIX    TAL_NET_PROVIDER_POSIX
#define TAL_NET_TYPE_PLATFORM TAL_NET_PROVIDER_TKL
#define TAL_NET_TYPE_AT_MODEM TAL_NET_PROVIDER_AT_MODEM
#define TAL_NET_TYPE_MAX      TAL_NET_PROVIDER_MAX

/* [deprecated-s4] no portable way to deprecate a macro; see tools/provider_rename.py */
#define tal_network_card_init          tal_net_provider_init
#define tal_network_get_active_ops     tal_net_provider_ops
#define tal_network_card_get_active_ip tal_net_route_src_ip

/*
 * Three compatibility wrappers over the same state, each still touching only
 * the half of the route it always owned. They have zero callers in the tree and
 * are deleted in S4 rather than renamed: giving a compatibility wrapper a new
 * name would turn it into a new public API, which is the opposite of why it
 * exists. New code should call tal_net_route_set() - moving both halves in one
 * call is the only way to avoid publishing a backend and an address that
 * disagree.
 */
OPERATE_RET tal_network_card_set_active(tal_net_provider_id_t type)
    __attribute__((deprecated("use tal_net_route_set(); this wrapper is removed in S4")));
tal_net_provider_id_t tal_network_card_get_active_type(void)
    __attribute__((deprecated("use tal_net_route_get(); this wrapper is removed in S4")));
OPERATE_RET tal_network_card_set_active_ip(TUYA_IP_ADDR_T ipaddr)
    __attribute__((deprecated("use tal_net_route_set(); this wrapper is removed in S4")));

#ifdef __cplusplus
}
#endif

#endif /* __TAL_NETWORK_REGISTER_H__ */
