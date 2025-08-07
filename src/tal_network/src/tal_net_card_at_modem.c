/**
 * @file tal_net_card_at_modem.c
 * @brief tal_net_card_at_modem module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_iot_config.h"
#include "tuya_cloud_types.h"

#if defined(ENABLE_AT_MODEM) && (ENABLE_AT_MODEM == 1)

#include "tal_network_register.h"

#include "tal_at_modem.h"

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

TAL_NETWORK_CARD_T tal_network_card_at_modem = {
    .name = "at_modem",
    .type = TAL_NET_TYPE_AT_MODEM,
    .ipaddr = 0,
    .ops =
        {
            .get_errno = tal_at_net_get_errno,
            .fd_set = tal_at_net_fd_set,
            .fd_clear = tal_at_net_fd_clear,
            .fd_isset = tal_at_net_fd_isset,
            .fd_zero = tal_at_net_fd_zero,
            .select = tal_at_net_select,
            .get_nonblock = tal_at_net_get_nonblock,
            .set_block = tal_at_net_set_block,
            .close = tal_at_net_close,
            .socket_create = tal_at_net_socket_create,
            .connect = tal_at_net_connect,
            .connect_raw = tal_at_net_connect_raw,
            .bind = tal_at_net_bind,
            .listen = tal_at_net_listen,
            .send = tal_at_net_send,
            .send_to = tal_at_net_send_to,
            .accept = tal_at_net_accept,
            .recv = tal_at_net_recv,
            .recv_nd_size = tal_at_net_recv_nd_size,
            .recvfrom = tal_at_net_recvfrom,
            .set_timeout = tal_at_net_set_timeout,
            .set_bufsize = tal_at_net_set_bufsize,
            .set_reuse = tal_at_net_set_reuse,
            .disable_nagle = tal_at_net_disable_nagle,
            .set_broadcast = tal_at_net_set_broadcast,
            .gethostbyname = tal_at_net_gethostbyname,
            .set_keepalive = tal_at_net_set_keepalive,
            .get_socket_ip = tal_at_net_get_socket_ip,
            .str2addr = tal_at_net_str2addr,
            .addr2str = tal_at_net_addr2str,
            .setsockopt = tal_at_net_setsockopt,
            .getsockopt = tal_at_net_getsockopt,
        },
};

#endif // ENABLE_AT_MODEM
