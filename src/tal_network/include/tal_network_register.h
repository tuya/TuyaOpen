/**
 * @file tal_network_register.h
 * @brief tal_network_register module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
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

typedef uint8_t TAL_NETWORK_CARD_TYPE_E;
#define TAL_NET_TYPE_POSIX    (0)
#define TAL_NET_TYPE_PLATFORM (1)
#define TAL_NET_TYPE_AT_MODEM (2)
#define TAL_NET_TYPE_MAX      (3)

typedef struct {
    char name[16];
    TAL_NETWORK_CARD_TYPE_E type;
    TUYA_IP_ADDR_T ipaddr;
    TAL_NETWORK_OPS_T ops;
} TAL_NETWORK_CARD_T;

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tal_network_card_init(void);

OPERATE_RET tal_network_card_set_active(TAL_NETWORK_CARD_TYPE_E type);

TAL_NETWORK_CARD_TYPE_E tal_network_card_get_active_type(void);

TAL_NETWORK_OPS_T *tal_network_get_active_ops(void);

/**
 * @brief Record the source address outbound sockets should bind to.
 *
 * Pushed by whoever owns link selection (netmgr) when the active connection or
 * its address changes, so the value is refreshed exactly once per link event
 * instead of being looked up on every connect.
 *
 * @param[in] ipaddr the active connection address, or 0 when no link is up
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_network_card_set_active_ip(TUYA_IP_ADDR_T ipaddr);

/**
 * @brief Get the source address outbound sockets should bind to.
 *
 * @return the active connection address, or 0 when unknown - callers must treat
 *         0 as "do not bind" and let the stack pick the source itself.
 */
TUYA_IP_ADDR_T tal_network_card_get_active_ip(void);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_NETWORK_REGISTER_H__ */
