/**
 * @file tkl_init_network.h
 * @brief Common process - tkl init network description
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_INIT_NETWORK_H__
#define __TKL_INIT_NETWORK_H__

#include "tkl_network.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * @brief the description of tuya kernel adapter layer network
 *
 */
typedef struct {
    TUYA_ERRNO (*tkl_get_errno)(void);
    OPERATE_RET (*tkl_fd_set1)(const int fd, TUYA_FD_SET_T *fds);
    OPERATE_RET (*tkl_fd_clear)(const int fd, TUYA_FD_SET_T *fds);
    OPERATE_RET (*tkl_fd_isset)(const int fd, TUYA_FD_SET_T *fds);
    OPERATE_RET (*tkl_fd_zero)(TUYA_FD_SET_T *fds);
    int (*tkl_select)(const int maxfd, TUYA_FD_SET_T *readfds, TUYA_FD_SET_T *writefds, TUYA_FD_SET_T *errorfds,
                      const uint32_t ms_timeout);
    TUYA_ERRNO (*tkl_close)(const int fd);
    int (*tkl_socket_create)(const TUYA_PROTOCOL_TYPE_E type);
    TUYA_ERRNO (*tkl_connect)(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port);
    TUYA_ERRNO (*tkl_connect_raw)(const int fd, void *p_socket, const int len);
    TUYA_ERRNO (*tkl_bind)(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port);
    TUYA_ERRNO (*tkl_listen)(const int fd, const int backlog);
    TUYA_ERRNO (*tkl_send)(const int fd, const void *buf, const uint32_t nbytes);
    TUYA_ERRNO(*tkl_send_to)
    (const int fd, const void *buf, const uint32_t nbytes, const TUYA_IP_ADDR_T addr, const uint16_t port);
    TUYA_ERRNO (*tkl_recv)(const int fd, void *buf, const uint32_t nbytes);
    TUYA_ERRNO (*tkl_recvfrom)(const int fd, void *buf, const uint32_t nbytes, TUYA_IP_ADDR_T *addr, uint16_t *port);
    TUYA_ERRNO (*tkl_accept)(const int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);
    int (*tkl_recv_nd_size)(const int fd, void *buf, const uint32_t buf_size, const uint32_t nd_size);
    OPERATE_RET (*tkl_socket_bind)(const int fd, const char *ip);
    OPERATE_RET (*tkl_set_block)(const int fd, const BOOL_T block);
    OPERATE_RET (*tkl_set_cloexec)(const int fd);
    OPERATE_RET (*tkl_get_socket_ip)(const int fd, TUYA_IP_ADDR_T *addr);
    int (*tkl_get_nonblock)(const int fd);
    OPERATE_RET (*tkl_gethostbyname)(const char *domain, TUYA_IP_ADDR_T *addr);
    TUYA_IP_ADDR_T (*tkl_str2addr)(const char *ip_str);
    char *(*tkl_addr2str)(const TUYA_IP_ADDR_T ipaddr);
    OPERATE_RET(*tkl_setsockopt)
    (const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname, const void *optval, const int optlen);
    OPERATE_RET(*tkl_getsockopt)
    (const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname, void *optval, int *optlen);
    OPERATE_RET (*tkl_set_timeout)(const int fd, const int ms_timeout, const TUYA_TRANS_TYPE_E type);
    OPERATE_RET (*tkl_set_bufsize)(const int fd, const int buf_size, const TUYA_TRANS_TYPE_E type);
    OPERATE_RET (*tkl_set_reuse)(const int fd);
    OPERATE_RET (*tkl_disable_nagle)(const int fd);
    OPERATE_RET (*tkl_set_broadcast)(const int fd);
    OPERATE_RET(*tkl_set_keepalive)
    (int fd, const BOOL_T alive, const uint32_t idle, const uint32_t intr, const uint32_t cnt);
    OPERATE_RET (*tkl_getsockname)(int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);
    OPERATE_RET (*tkl_getpeername)(int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);
    OPERATE_RET (*tkl_sethostname)(const char *hostname);
} TKL_NETWORK_DESC_T;

/**
 * @brief register wired description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_NETWORK_DESC_T *tkl_network_desc_get(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TKL_INIT_WIRED_H__
