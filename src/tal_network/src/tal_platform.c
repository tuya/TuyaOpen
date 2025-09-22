/**
 * @file tal_platform.c
 * @brief tal_platform module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_iot_config.h"
#include "tuya_cloud_types.h"

#if (defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)) || 100 == OPERATING_SYSTEM
#define NET_USING_POSIX        1
#define TAL_TO_SYS_FD_SET(fds) ((fd_set *)fds)
#else
#define NET_USING_TKL 1
#endif

#if defined(NET_USING_TKL) && (NET_USING_TKL == 1)

#include "tal_api.h"
#include "tal_network_register.h"

#include "tkl_network.h"

/***********************************************************
************************macro define************************
***********************************************************/

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

TAL_NETWORK_CARD_T tal_network_card_platform = {
    .name = "tkl",
    .type = TAL_NET_TYPE_PLATFORM,
    .ipaddr = 0,
    .ops =
        {
            .get_errno = tkl_net_get_errno,
            .fd_set = tkl_net_fd_set,
            .fd_clear = tkl_net_fd_clear,
            .fd_isset = tkl_net_fd_isset,
            .fd_zero = tkl_net_fd_zero,
            .select = tkl_net_select,
            .get_nonblock = tkl_net_get_nonblock,
            .set_block = tkl_net_set_block,
            .close = tkl_net_close,
            .socket_create = tkl_net_socket_create,
            .connect = tkl_net_connect,
            .connect_raw = tkl_net_connect_raw,
            .bind = tkl_net_bind,
            .listen = tkl_net_listen,
            .send = tkl_net_send,
            .send_to = tkl_net_send_to,
            .accept = tkl_net_accept,
            .recv = tkl_net_recv,
            .recv_nd_size = tkl_net_recv_nd_size,
            .recvfrom = tkl_net_recvfrom,
            .set_timeout = tkl_net_set_timeout,
            .set_bufsize = tkl_net_set_bufsize,
            .set_reuse = tkl_net_set_reuse,
            .disable_nagle = tkl_net_disable_nagle,
            .set_broadcast = tkl_net_set_broadcast,
            .gethostbyname = tkl_net_gethostbyname,
            .set_keepalive = tkl_net_set_keepalive,
            .get_socket_ip = tkl_net_get_socket_ip,
            .str2addr = tkl_net_str2addr,
            .addr2str = tkl_net_addr2str,
            .setsockopt = tkl_net_setsockopt,
            .getsockopt = tkl_net_getsockopt,
        },
};

#endif // NET_USING_TKL
