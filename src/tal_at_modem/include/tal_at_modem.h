/**
 * @file tal_at_modem.h
 * @brief tal_at_modem module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TAL_AT_MODEM_H__
#define __TAL_AT_MODEM_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef uint8_t TAL_AT_MODEM_TYPE_T;
#define TAL_AT_MODEM_TYPE_ML307R 0x01
#define TAL_AT_MODEM_TYPE_MAX    0x02

typedef uint8_t TAL_AT_MODEM_CMD_T;
#define TAL_AT_MODEM_CMD_READY          (0x00)
#define TAL_AT_MODEM_CMD_SIM            (0x01)
#define TAL_AT_MODEM_CMD_NETWORK        (0x02)
#define TAL_AT_MODEM_CMD_CONNECT_STATUS (0x03)
#define TAL_AT_MODEM_CMD_SOCKET_RECV    (0x04)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void (*AT_MODEM_CB)(TAL_AT_MODEM_CMD_T cmd, void *args);

typedef enum {
    AT_CONNECTED,
    AT_CONNECT_FAILED,
    AT_DISCONNECTED,
} AT_MODEM_EVENT_E;
typedef void (*AT_MODEM_EVENT_CB)(AT_MODEM_EVENT_E event, void *arg);

typedef struct {
    int fd;
    int result; // 0: success, other: fail reason
} AT_CONNECT_STATUS_T;

typedef struct {
    int fd;
    uint32_t len;
    char *data;
} AT_SOCKET_RECV_T;

typedef struct {
    OPERATE_RET (*get_at_modem_ip)(NW_IP_S *ip);
    uint8_t (*at_check)(void);
    OPERATE_RET (*at_get_cereg_status)(void);
    uint8_t (*at_get_socket_num_max)(void);
    TUYA_ERRNO (*at_connect)(int fd, TUYA_PROTOCOL_TYPE_E type, const char *ip, uint16_t port, uint32_t timeout_ms);
    OPERATE_RET (*at_gethostbyname)(const char *domain, TUYA_IP_ADDR_T *addr);
    TUYA_ERRNO (*at_send)(const int fd, const void *buf, const uint32_t nbytes);
    TUYA_ERRNO (*at_close)(const int fd);
} AT_MODULE_OPS_T;

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tal_at_modem_init(const char *transport_name, TAL_AT_MODEM_TYPE_T type);

OPERATE_RET tal_at_modem_set_event_cb(AT_MODEM_EVENT_CB cb);

OPERATE_RET tal_at_modem_get_ip(NW_IP_S *ip);

OPERATE_RET tal_at_modem_deinit(void);

TUYA_ERRNO tal_at_net_get_errno(void);

OPERATE_RET tal_at_net_fd_set(int fd, TUYA_FD_SET_T *fds);

OPERATE_RET tal_at_net_fd_clear(int fd, TUYA_FD_SET_T *fds);

OPERATE_RET tal_at_net_fd_isset(int fd, TUYA_FD_SET_T *fds);

OPERATE_RET tal_at_net_fd_zero(TUYA_FD_SET_T *fds);

int tal_at_net_select(const int maxfd, TUYA_FD_SET_T *readfds, TUYA_FD_SET_T *writefds, TUYA_FD_SET_T *errorfds,
                      const uint32_t ms_timeout);

int tal_at_net_get_nonblock(const int fd);

OPERATE_RET tal_at_net_set_block(const int fd, const BOOL_T block);

TUYA_ERRNO tal_at_net_close(const int fd);

int tal_at_net_socket_create(const TUYA_PROTOCOL_TYPE_E type);

TUYA_ERRNO tal_at_net_connect(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port);

TUYA_ERRNO tal_at_net_connect_raw(const int fd, void *p_socket, const int len);

TUYA_ERRNO tal_at_net_bind(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port);

TUYA_ERRNO tal_at_net_listen(const int fd, const int backlog);

TUYA_ERRNO tal_at_net_send(const int fd, const void *buf, const uint32_t nbytes);

TUYA_ERRNO tal_at_net_send_to(const int fd, const void *buf, const uint32_t nbytes, const TUYA_IP_ADDR_T addr,
                              const uint16_t port);

int tal_at_net_accept(const int fd, TUYA_IP_ADDR_T *addr, uint16_t *port);

TUYA_ERRNO tal_at_net_recv(const int fd, void *buf, const uint32_t nbytes);

int tal_at_net_recv_nd_size(const int fd, void *buf, const uint32_t buf_size, const uint32_t nd_size);

TUYA_ERRNO tal_at_net_recvfrom(const int fd, void *buf, const uint32_t nbytes, TUYA_IP_ADDR_T *addr, uint16_t *port);

OPERATE_RET tal_at_net_set_timeout(const int fd, const int ms_timeout, const TUYA_TRANS_TYPE_E type);

OPERATE_RET tal_at_net_set_bufsize(const int fd, const int buf_size, const TUYA_TRANS_TYPE_E type);

OPERATE_RET tal_at_net_set_reuse(const int fd);

OPERATE_RET tal_at_net_disable_nagle(const int fd);

OPERATE_RET tal_at_net_set_broadcast(const int fd);

OPERATE_RET tal_at_net_gethostbyname(const char *domain, TUYA_IP_ADDR_T *addr);

OPERATE_RET tal_at_net_set_keepalive(int fd, const BOOL_T alive, const uint32_t idle, const uint32_t intr,
                                     const uint32_t cnt);

OPERATE_RET tal_at_net_get_socket_ip(int fd, TUYA_IP_ADDR_T *addr);

TUYA_IP_ADDR_T tal_at_net_str2addr(const char *ip_str);

char *tal_at_net_addr2str(TUYA_IP_ADDR_T ipaddr);

OPERATE_RET tal_at_net_setsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname,
                                  const void *optval, const int optlen);

OPERATE_RET tal_at_net_getsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname, void *optval,
                                  int *optlen);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_AT_MODEM_H__ */
