/**
 * @file tkl_init_network.c
 * @brief Common process - tkl init wired description
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_init_network.h"

const TKL_NETWORK_DESC_T c_network_desc = {
    .tkl_get_errno = tkl_net_get_errno,
    .tkl_fd_set1 = tkl_net_fd_set,
    .tkl_fd_clear = tkl_net_fd_clear,
    .tkl_fd_isset = tkl_net_fd_isset,
    .tkl_fd_zero = tkl_net_fd_zero,
    .tkl_select = tkl_net_select,
    .tkl_close = tkl_net_close,
    .tkl_socket_create = tkl_net_socket_create,
    .tkl_connect = tkl_net_connect,
    .tkl_connect_raw = tkl_net_connect_raw,
    .tkl_bind = tkl_net_bind,
    .tkl_listen = tkl_net_listen,
    .tkl_send = tkl_net_send,
    .tkl_send_to = tkl_net_send_to,
    .tkl_recv = tkl_net_recv,
    .tkl_recvfrom = tkl_net_recvfrom,
    .tkl_accept = tkl_net_accept,
    .tkl_recv_nd_size = tkl_net_recv_nd_size,
    .tkl_socket_bind = tkl_net_socket_bind,
    .tkl_set_block = tkl_net_set_block,
    .tkl_set_cloexec = tkl_net_set_cloexec,
    .tkl_get_socket_ip = tkl_net_get_socket_ip,
    .tkl_get_nonblock = tkl_net_get_nonblock,
    .tkl_gethostbyname = tkl_net_gethostbyname,
    .tkl_str2addr = tkl_net_str2addr,
    .tkl_addr2str = tkl_net_addr2str,
    .tkl_setsockopt = tkl_net_setsockopt,
    .tkl_getsockopt = tkl_net_getsockopt,
    .tkl_set_timeout = tkl_net_set_timeout,
    .tkl_set_bufsize = tkl_net_set_bufsize,
    .tkl_set_reuse = tkl_net_set_reuse,
    .tkl_disable_nagle = tkl_net_disable_nagle,
    .tkl_set_broadcast = tkl_net_set_broadcast,
    .tkl_set_keepalive = tkl_net_set_keepalive,
    .tkl_getsockname = tkl_net_getsockname,
    .tkl_getpeername = tkl_net_getpeername,
    .tkl_sethostname = tkl_net_sethostname,
};

/**
 * @brief register wired description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE TKL_NETWORK_DESC_T *tkl_network_desc_get()
{
    return (TKL_NETWORK_DESC_T *)&c_network_desc;
}
