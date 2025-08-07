/**
 * @file tal_at_modem.c
 * @brief tal_at_modem module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_at_modem.h"

#include "at_utils.h"
#include "at_client.h"
#include "at_module_ml307r.h"

#include "tdl_transport_manage.h"

#include "tal_api.h"

#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define MODULE_HEARTBEAT_INTERVAL_MS (30 * 1000) // Heartbeat interval for AT modem

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    int is_used; // Indicates if the socket is in use

    MUTEX_HANDLE mutex;      // Mutex for thread safety
    MUTEX_HANDLE send_mutex; // Mutex for thread safety

    char ip_str[16];   // IP address of the socket
    TUYA_IP_ADDR_T ip; // IP address structure for the socket

    uint16_t port; // Port number of the socket

    TUYA_PROTOCOL_TYPE_E type; // Protocol type (TCP/UDP)

    BOOL_T is_block;     // Indicates if the socket is blocking
    BOOL_T is_connected; // Indicates if the socket is connected

    // send
    uint32_t send_timeout; // Send timeout in milliseconds

    // recv
    char *recv;
    uint32_t recv_size;    // The total size of the receive buffer
    uint32_t recv_used;    // Amount of data currently in the receive buffer
    uint32_t recv_timeout; // Receive timeout in milliseconds
} AT_SOCKET_T;

typedef struct {
    NW_IP_S ip; // IP address structure for the modem
    AT_MODEM_EVENT_CB event_cb;
    MUTEX_HANDLE mutex;
    TIMER_ID heartbeat_timer; // Timer for heartbeat

    TUYA_ERRNO errno;
    TAL_AT_MODEM_TYPE_T type;

    TDL_TRANSPORT_HANDLE transport_hdl;

    uint8_t socket_num_max;
    AT_SOCKET_T *socket;

    uint8_t at_ready;
    uint8_t sim_ready;
    uint8_t network_ready;

    AT_MODULE_OPS_T ops;
} TAL_AT_MODEM_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TAL_AT_MODEM_CFG_T sg_at_modem = {
    .event_cb = NULL,
    .mutex = NULL,
    .heartbeat_timer = NULL, // Heartbeat timer initialized to NULL
    .errno = UNW_SUCCESS,
    .type = TAL_AT_MODEM_TYPE_ML307R, // Default modem type
    .transport_hdl = NULL,
    .socket_num_max = 0,
    .socket = NULL,
    .at_ready = 0,
    .sim_ready = 0,
    .network_ready = 0,
    .ops = {NULL},
};
/***********************************************************
***********************function define**********************
***********************************************************/

static void __at_socket_status_cb(AT_CONNECT_STATUS_T *status)
{
    if (!status) {
        return;
    }

    // PR_DEBUG("Socket status: fd=%d, result=%d", status->fd, status->result);
    if (status->fd < 0 || status->fd >= sg_at_modem.socket_num_max) {
        PR_ERR("Invalid socket fd: %d", status->fd);
        return;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[status->fd];

    if (NULL == socket->mutex) {
        PR_ERR("Socket mutex is NULL for fd: %d", status->fd);
        return;
    }

    if (status->result == 0) {
        // Connection successful
        socket->is_connected = 1;
        PR_DEBUG("Socket fd %d connected successfully", status->fd);
    } else {
        // Connection failed
        socket->is_connected = 0;
        PR_ERR("Socket fd %d connection failed, result: %d", status->fd, status->result);
    }

    return;
}

static void __at_socket_recv_cb(AT_SOCKET_RECV_T *recv_data)
{
    if (!recv_data) {
        return;
    }

    // PR_DEBUG("Socket recv: fd=%d, len=%d, data=%.*s", recv_data->fd, recv_data->len, recv_data->len,
    // recv_data->data);
    // PR_HEXDUMP_DEBUG("Socket recv data", recv_data->data, recv_data->len);

    if (recv_data->fd < 0 || recv_data->fd >= sg_at_modem.socket_num_max) {
        PR_ERR("Invalid socket fd: %d", recv_data->fd);
        return;
    }

    if (NULL == recv_data->data || recv_data->len == 0) {
        PR_ERR("Invalid socket recv data for fd: %d", recv_data->fd);
        return;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[recv_data->fd];

    if (NULL == socket->mutex) {
        PR_ERR("Socket mutex is NULL for fd: %d", recv_data->fd);
        return;
    }

    if (socket->is_connected == 0 || socket->is_used == 0) {
        PR_ERR("Socket fd %d is not connected or not in use", recv_data->fd);
        return;
    }

    tal_mutex_lock(socket->mutex);
#if 1
    char *p_start = NULL;
    if (socket->recv == NULL) {
        socket->recv = (char *)tal_malloc(recv_data->len);
        if (NULL == socket->recv) {
            PR_ERR("Failed to allocate memory for socket recv buffer");
            tal_mutex_unlock(socket->mutex);
            return;
        }
        memset(socket->recv, 0, recv_data->len);
        socket->recv_size = recv_data->len;
        p_start = socket->recv;
    } else if (socket->recv_used + recv_data->len > socket->recv_size) {
        // Resize the buffer if needed
        char *new_recv = (char *)tal_malloc(socket->recv_used + recv_data->len);
        if (NULL == new_recv) {
            PR_ERR("Failed to allocate memory for resized socket recv buffer");
            tal_mutex_unlock(socket->mutex);
            return;
        }
        memcpy(new_recv, socket->recv, socket->recv_used);
        tal_free(socket->recv);
        socket->recv = new_recv;
        socket->recv_size = socket->recv_used + recv_data->len;
        p_start = new_recv + socket->recv_used;
    } else {
        p_start = socket->recv + socket->recv_used;
    }

    memcpy(p_start, recv_data->data, recv_data->len);
    socket->recv_used += recv_data->len;
#else
#endif
    tal_mutex_unlock(socket->mutex);
    return;
}

static void __at_modem_urc_cb(TAL_AT_MODEM_CMD_T cmd, void *args)
{
    switch (cmd) {
    case (TAL_AT_MODEM_CMD_READY): {
        if (!args) {
            return;
        }

        sg_at_modem.at_ready = (*(uint8_t *)(args)) == 1 ? 1 : 0;

        PR_DEBUG("at ready: %d", sg_at_modem.at_ready);
    } break;
    case (TAL_AT_MODEM_CMD_SIM): {
        if (!args) {
            return;
        }

        sg_at_modem.sim_ready = (*(uint8_t *)(args)) == 1 ? 1 : 0;

        PR_DEBUG("sim ready: %d", sg_at_modem.sim_ready);
    } break;
    case (TAL_AT_MODEM_CMD_NETWORK): {
        if (!args) {
            return;
        }

        sg_at_modem.network_ready = (*(uint8_t *)(args)) == 1 ? 1 : 0;

        if (0 == sg_at_modem.network_ready) {
            // Reset IP address if network is not ready
            memset(&sg_at_modem.ip, 0, sizeof(NW_IP_S));
        }

        if (sg_at_modem.event_cb) {
            sg_at_modem.event_cb(sg_at_modem.network_ready ? AT_CONNECTED : AT_DISCONNECTED, NULL);
        }

        PR_DEBUG("network read: %d", sg_at_modem.network_ready);
    } break;
    case (TAL_AT_MODEM_CMD_CONNECT_STATUS): {
        __at_socket_status_cb((AT_CONNECT_STATUS_T *)args);
    } break;
    case (TAL_AT_MODEM_CMD_SOCKET_RECV): {
        __at_socket_recv_cb((AT_SOCKET_RECV_T *)args);
    } break;
    default: {
        PR_ERR("Unknown URC cmd: %d", cmd);
    } break;
    }

    return;
}

static void __at_modem_heartbeat_cb(TIMER_ID timer_id, void *arg)
{
    static uint32_t heartbeat_err_cnt = 0;

    if (NULL == arg) {
        return;
    }

    TAL_AT_MODEM_CFG_T *modem = (TAL_AT_MODEM_CFG_T *)arg;

    uint8_t at_ready = 0;

    if (NULL == modem->mutex) {
        return;
    }

    tal_mutex_lock(modem->mutex);
    at_ready = modem->ops.at_check();
    if (at_ready == 0) {
        heartbeat_err_cnt++;

        if (heartbeat_err_cnt * MODULE_HEARTBEAT_INTERVAL_MS >= 60 * 1000) {
            heartbeat_err_cnt = 0;
            modem->at_ready = 0;
            modem->sim_ready = 0;
            modem->network_ready = 0;

            memset(&modem->ip, 0, sizeof(NW_IP_S));
            if (modem->event_cb) {
                modem->event_cb(AT_DISCONNECTED, NULL);
            }
        }
    } else if (at_ready && modem->at_ready == 0) {
        // AT is ready now
        modem->at_ready = 1;
        heartbeat_err_cnt = 0;
    }
    tal_mutex_unlock(modem->mutex);

    return;
}

OPERATE_RET tal_at_modem_init(const char *transport_name, TAL_AT_MODEM_TYPE_T type)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(transport_name, OPRT_INVALID_PARM);

    // Create mutex
    if (NULL == sg_at_modem.mutex) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_at_modem.mutex));
    }

    // Create heartbeat timer
    if (NULL == sg_at_modem.heartbeat_timer) {
        tal_sw_timer_create(__at_modem_heartbeat_cb, &sg_at_modem, &sg_at_modem.heartbeat_timer);
    }

    TUYA_CALL_ERR_GOTO(tdl_transport_find(transport_name, &sg_at_modem.transport_hdl), __ERR);
    TUYA_CALL_ERR_GOTO(tdl_transport_open(sg_at_modem.transport_hdl), __ERR);

    TUYA_CALL_ERR_GOTO(at_client_init(sg_at_modem.transport_hdl), __ERR);

    // init 4G module
    if (type == TAL_AT_MODEM_TYPE_ML307R) {
        // Initialize ML307R specific settings
        TUYA_CALL_ERR_GOTO(at_module_ml307r_init(&sg_at_modem.ops, __at_modem_urc_cb), __ERR);
        tal_sw_timer_start(sg_at_modem.heartbeat_timer, MODULE_HEARTBEAT_INTERVAL_MS, TAL_TIMER_CYCLE);
    }

    // get max socket number
    if (sg_at_modem.ops.at_get_socket_num_max) {
        sg_at_modem.socket_num_max = sg_at_modem.ops.at_get_socket_num_max();
        PR_DEBUG("AT module max socket number: %d", sg_at_modem.socket_num_max);
    }

    if (0 == sg_at_modem.socket_num_max) {
        rt = OPRT_COM_ERROR;
        PR_ERR("Socket max number is 0");
        goto __ERR;
    }

    // allocate memory for sockets
    sg_at_modem.socket = (AT_SOCKET_T *)tal_malloc(sg_at_modem.socket_num_max * sizeof(AT_SOCKET_T));
    if (NULL == sg_at_modem.socket) {
        PR_ERR("malloc for at socket fail");
        rt = OPRT_MALLOC_FAILED;
        goto __ERR;
    }
    memset(sg_at_modem.socket, 0, sg_at_modem.socket_num_max * sizeof(AT_SOCKET_T));

    // AT
    if (0 == sg_at_modem.ops.at_check()) {
        // TODO: wait ok?
        PR_ERR("AT check fail");
        rt = OPRT_COM_ERROR;
        goto __ERR;
    }

    // cereg
    if (sg_at_modem.ops.at_get_cereg_status) {
        TUYA_CALL_ERR_LOG(sg_at_modem.ops.at_get_cereg_status());
    }

    // get IP address
    memset(&sg_at_modem.ip, 0, sizeof(NW_IP_S));
    NW_IP_S at_module_ip = {0};
    tal_at_modem_get_ip(&at_module_ip);

    return OPRT_OK;

__ERR:
    // tal_at_modem_deinit();

    return rt;
}

OPERATE_RET tal_at_modem_set_event_cb(AT_MODEM_EVENT_CB cb)
{
    if (NULL == cb) {
        PR_ERR("AT modem event callback is NULL");
        return OPRT_INVALID_PARM;
    }

    sg_at_modem.event_cb = cb;

    return OPRT_OK;
}

OPERATE_RET tal_at_modem_get_ip(NW_IP_S *ip)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == ip) {
        PR_ERR("IP parameter is NULL");
        return OPRT_INVALID_PARM;
    }

    if (NULL == sg_at_modem.mutex) {
        PR_ERR("AT modem mutex is NULL");
        return OPRT_COM_ERROR;
    }

    if (strlen(sg_at_modem.ip.ip) != 0) {
        memcpy(ip, &sg_at_modem.ip, sizeof(NW_IP_S));
        return OPRT_OK;
    }

    if (sg_at_modem.ops.get_at_modem_ip) {
        tal_mutex_lock(sg_at_modem.mutex);
        rt = sg_at_modem.ops.get_at_modem_ip(ip);
        tal_mutex_unlock(sg_at_modem.mutex);
    }

    return rt;
}

static void __at_modem_free_socket(AT_SOCKET_T *socket, uint8_t number)
{
    if (NULL == socket) {
        return;
    }

    for (uint8_t i = 0; i < number; i++) {
        if (socket[i].recv) {
            tal_free(socket[i].recv);
            socket[i].recv = NULL;
        }
    }

    tal_free(socket);

    return;
}

OPERATE_RET tal_at_modem_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_at_modem.mutex) {
        PR_WARN("AT modem maybe not init");
        return OPRT_OK;
    }

    tal_mutex_lock(sg_at_modem.mutex);
    PR_DEBUG("-> lock sg_at_modem.mutex");

    at_client_deinit();

    __at_modem_free_socket(sg_at_modem.socket, sg_at_modem.socket_num_max);
    sg_at_modem.socket = NULL;
    sg_at_modem.socket_num_max = 0;

    tal_mutex_unlock(sg_at_modem.mutex);
    PR_DEBUG("<- unlock sg_at_modem.mutex");

    tal_mutex_release(sg_at_modem.mutex);
    sg_at_modem.mutex = NULL;

    return rt;
}

static int __find_free_socket(void)
{
    if (NULL == sg_at_modem.mutex) {
        return -1;
    }

    tal_mutex_lock(sg_at_modem.mutex);
    PR_DEBUG("-> lock sg_at_modem.mutex");

    for (int i = 0; i < sg_at_modem.socket_num_max; i++) {
        if (sg_at_modem.socket[i].is_used == 0) {
            tal_mutex_unlock(sg_at_modem.mutex);
            PR_DEBUG("<- unlock sg_at_modem.mutex");
            return i;
        }
    }
    tal_mutex_unlock(sg_at_modem.mutex);
    PR_ERR("<- unlock sg_at_modem.mutex");

    return -1; // No free socket found
}

TUYA_ERRNO tal_at_net_get_errno(void)
{
    return sg_at_modem.errno;
}

OPERATE_RET tal_at_net_fd_set(int fd, TUYA_FD_SET_T *fds)
{
    if (fd < 0 || fd >= sg_at_modem.socket_num_max || fds == NULL) {
        return OPRT_INVALID_PARM;
    }

    fds->placeholder[fd / 8] |= (1 << (fd % 8)); // Set the bit for the fd

    return OPRT_OK;
}

OPERATE_RET tal_at_net_fd_clear(int fd, TUYA_FD_SET_T *fds)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_fd_isset(int fd, TUYA_FD_SET_T *fds)
{
    // PR_ERR("[%s] not supported", __func__);

    if (fd < 0 || fd >= sg_at_modem.socket_num_max || fds == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (fds->placeholder[fd / 8] & (1 << (fd % 8))) {
        return OPRT_OK; // fd is set
    } else {
        return OPRT_COM_ERROR; // fd is not set
    }
    return OPRT_OK;
}

OPERATE_RET tal_at_net_fd_zero(TUYA_FD_SET_T *fds)
{
    // PR_ERR("[%s] not supported", __func__);
    if (fds == NULL) {
        return OPRT_INVALID_PARM;
    }

    memset(fds->placeholder, 0, sizeof(fds->placeholder));
    return OPRT_OK;
}

int tal_at_net_select(const int maxfd, TUYA_FD_SET_T *readfds, TUYA_FD_SET_T *writefds, TUYA_FD_SET_T *errorfds,
                      const uint32_t ms_timeout)
{
    // TODO: Implement select for AT modem
    return 1;
}

int tal_at_net_get_nonblock(const int fd)
{
    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        return -3000 + fd;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];
    if (socket->is_used == 0) {
        PR_ERR("socket %d not used", fd);
        return -3000 + fd;
    }

    return socket->is_block ? 0 : 1;
}

OPERATE_RET tal_at_net_set_block(const int fd, const BOOL_T block)
{
    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        return OPRT_INVALID_PARM;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];
    if (socket->is_used == 0) {
        PR_ERR("socket %d not used", fd);
        return OPRT_INVALID_PARM;
    }

    socket->is_block = (block == TRUE) ? 1 : 0;

    return OPRT_OK;
}

TUYA_ERRNO tal_at_net_close(const int fd)
{
    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        return UNW_FAIL;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];
    if (socket->is_used == 0) {
        PR_ERR("Socket %d not used", fd);
        return UNW_FAIL;
    }

    if (socket->mutex == NULL) {
        PR_ERR("Socket mutex is NULL for fd: %d", fd);
        return UNW_FAIL;
    }

    PR_DEBUG("tal_at_net_close: fd=%d, recv: %d", fd, socket->recv_used);

    uint32_t wait_cnt = 0;
    do {
        // Wait for the socket to be ready to close
        if (socket->recv_used > 0) {
            // PR_DEBUG("Waiting for socket %d to finish receiving data", fd);
            tal_system_sleep(50); // Wait for a short time before checking again
            wait_cnt++;
            if (wait_cnt * 50 > 50 * 1000) {
                PR_ERR("Socket %d close timeout, recv_used: %d", fd, socket->recv_used);
                break;
            }
            continue;
        }

        break; // Exit the loop if the socket is ready to close
    } while (1);

    tal_mutex_lock(socket->send_mutex);
    // Reset socket state
    memset(socket->ip_str, 0, sizeof(socket->ip_str));
    socket->ip = 0;
    socket->port = 0;

    if (socket->recv) {
        PR_DEBUG("--> lock socket mutex for fd: %d", fd);
        tal_free(socket->recv);
        socket->recv = NULL;
        socket->recv_size = 0;
        socket->recv_used = 0;
    }

    if (sg_at_modem.ops.at_close && socket->is_connected) {
        sg_at_modem.ops.at_close(fd);
    }

    socket->is_used = 0;
    socket->is_connected = 0;
    tal_mutex_unlock(socket->send_mutex);

    return UNW_SUCCESS;
}

int tal_at_net_socket_create(const TUYA_PROTOCOL_TYPE_E type)
{
    PR_DEBUG("tal_at_net_socket_create: type=%d", type);

    OPERATE_RET rt = OPRT_OK;
    int fd = -1;

    fd = __find_free_socket();
    if (fd < 0) {
        PR_ERR("no free socket");
        return -3000 + fd;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];

    if (NULL == socket->mutex) {
        rt = tal_mutex_create_init(&socket->mutex);
        if (OPRT_OK != rt) {
            PR_ERR("Socket[%d] create mutex fail, %d", fd, rt);
            return -1;
        }
    }

    if (NULL == socket->send_mutex) {
        rt = tal_mutex_create_init(&socket->send_mutex);
        if (OPRT_OK != rt) {
            PR_ERR("Socket[%d] create send mutex fail, %d", fd, rt);
            return -1;
        }
    }

    tal_mutex_lock(socket->mutex);
    PR_DEBUG("--> lock socket mutex for fd: %d", fd);
    socket->is_used = 1;
    socket->type = type;
    socket->is_block = 1;
    socket->is_connected = 0;
    socket->recv_used = 0;
    socket->send_timeout = 60 * 1000;
    socket->recv_timeout = 60 * 1000;
    tal_mutex_unlock(socket->mutex);
    PR_DEBUG("<-- unlock socket mutex for fd: %d", fd);

    return fd;
}

TUYA_ERRNO tal_at_net_connect(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port)
{
    PR_TRACE("tal_at_net_connect: fd=%d, addr=0x%08X, port=%d", fd, addr, port);

    TUYA_ERRNO rt_errno = UNW_FAIL;

    char ip_str[16] = {0};

    // PR_ERR("[%s] not supported", __func__);
    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        PR_ERR("Invalid socket fd: %d", fd);
        return UNW_FAIL;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];
    if (socket->is_used == 0) {
        PR_ERR("Socket %d not used", fd);
        return UNW_FAIL;
    }

    // Convert IP address to string
    char *p = tal_at_net_addr2str(addr);
    if (p == NULL) {
        PR_ERR("Invalid IP address");
        return UNW_FAIL;
    }
    memset(ip_str, 0, sizeof(ip_str));
    memcpy(ip_str, p, sizeof(ip_str) - 1);

    // Set socket parameters
    tal_mutex_lock(socket->send_mutex);
    PR_DEBUG("--> lock socket mutex for fd: %d", fd);
    socket->ip = addr;
    socket->port = port;
    memset(socket->ip_str, 0, sizeof(socket->ip_str));
    memcpy(socket->ip_str, ip_str, sizeof(socket->ip_str) - 1);

    if (sg_at_modem.ops.at_connect) {
        rt_errno = sg_at_modem.ops.at_connect(fd, socket->type, socket->ip_str, socket->port, socket->send_timeout);
    } else {
        PR_DEBUG("AT connect function not implemented");
        rt_errno = UNW_FAIL;
    }

    tal_mutex_unlock(socket->send_mutex);
    PR_DEBUG("<-- unlock socket mutex for fd: %d", fd);

    // wait connect
    uint32_t wait_cnt = 0;
    do {
        if (socket->is_connected) {
            break;
        }

        if (wait_cnt * 50 > 20 * 1000) {
            rt_errno = UNW_ETIMEDOUT;
            break;
        }

        tal_system_sleep(50);
        wait_cnt++;
    } while (1);

    return rt_errno;
}

TUYA_ERRNO tal_at_net_connect_raw(const int fd, void *p_socket, const int len)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

TUYA_ERRNO tal_at_net_bind(const int fd, const TUYA_IP_ADDR_T addr, const uint16_t port)
{
    PR_ERR("[%s] not supported", __func__);
    PR_ERR("tal_at_net_bind: fd=%d, addr=%u, port=%d", fd, addr, port);
    return OPRT_OK;
}

TUYA_ERRNO tal_at_net_listen(const int fd, const int backlog)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

TUYA_ERRNO tal_at_net_send(const int fd, const void *buf, const uint32_t nbytes)
{
    PR_DEBUG("tal_at_net_send: fd=%d, nbytes=%d", fd, nbytes);

    TUYA_CHECK_NULL_RETURN(buf, UNW_FAIL);

    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        PR_ERR("Invalid socket fd: %d", fd);
        return UNW_FAIL;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];
    if (socket->is_used == 0 || socket->is_connected == 0) {
        PR_ERR("Socket %d not used or not connected", fd);
        return UNW_FAIL;
    }

    if (NULL == socket->send_mutex) {
        PR_ERR("Socket mutex is NULL for fd: %d", fd);
        return UNW_FAIL;
    }

    tal_mutex_lock(socket->send_mutex);
    if (sg_at_modem.ops.at_send) {
        int send_len = sg_at_modem.ops.at_send(fd, buf, nbytes);
        tal_mutex_unlock(socket->send_mutex);
        return send_len;
    }
    tal_mutex_unlock(socket->send_mutex);

    return UNW_SUCCESS;
}

TUYA_ERRNO tal_at_net_send_to(const int fd, const void *buf, const uint32_t nbytes, const TUYA_IP_ADDR_T addr,
                              const uint16_t port)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

int tal_at_net_accept(const int fd, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

TUYA_ERRNO tal_at_net_recv(const int fd, void *buf, const uint32_t nbytes)
{
    // PR_DEBUG("tal_at_net_recv: fd=%d, nbytes=%d", fd, nbytes);

    if (buf == NULL || nbytes == 0) {
        PR_ERR("Invalid parameters: buf is NULL or nbytes is 0");
        return UNW_FAIL;
    }

    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        PR_ERR("Invalid socket fd: %d", fd);
        return UNW_FAIL;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];
    if (socket->is_used == 0 || socket->is_connected == 0) {
        PR_ERR("Socket %d not used or not connected", fd);
        return UNW_FAIL;
    }

    uint32_t wait_cnt = 0;
    do {
        if (nbytes <= socket->recv_used) {
            break; // Enough data available
        }
        tal_system_sleep(50); // Wait for more data
        wait_cnt++;
    } while (wait_cnt * 50 < 5 * 1000);

    uint32_t available_data = (socket->recv_used < nbytes) ? socket->recv_used : nbytes;

    tal_mutex_lock(socket->mutex);
    // PR_DEBUG("--> lock socket mutex for fd: %d", fd);

    memcpy(buf, socket->recv, available_data);
    socket->recv_used -= available_data;

    if (socket->recv_used == 0) {
        // If no data left, free the buffer
        tal_free(socket->recv);
        socket->recv = NULL;
        socket->recv_size = 0;
    } else {
        // Move remaining data to the start of the buffer
        memmove(socket->recv, socket->recv + available_data, socket->recv_used);
    }

    tal_mutex_unlock(socket->mutex);
    // PR_DEBUG("<-- unlock socket mutex for fd: %d", fd);

    return available_data; // Return the number of bytes received
}

int tal_at_net_recv_nd_size(const int fd, void *buf, const uint32_t buf_size, const uint32_t nd_size)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

TUYA_ERRNO tal_at_net_recvfrom(const int fd, void *buf, const uint32_t nbytes, TUYA_IP_ADDR_T *addr, uint16_t *port)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_set_timeout(const int fd, const int ms_timeout, const TUYA_TRANS_TYPE_E type)
{
    // PR_ERR("[%s] not supported", __func__);
    if (fd < 0 || fd >= sg_at_modem.socket_num_max) {
        PR_ERR("Invalid socket fd: %d", fd);
        return OPRT_INVALID_PARM;
    }

    AT_SOCKET_T *socket = &sg_at_modem.socket[fd];
    if (socket->is_used == 0) {
        PR_ERR("Socket %d not used", fd);
        return OPRT_INVALID_PARM;
    }

    if (NULL == socket->mutex) {
        PR_ERR("Socket mutex is NULL for fd: %d", fd);
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(socket->mutex);
    PR_DEBUG("--> lock socket mutex for fd: %d", fd);
    if (type == TRANS_RECV) {
        if (ms_timeout > 0) {
            socket->recv_timeout = ms_timeout;
        }
        // PR_DEBUG("Set recv timeout for socket %d: %d ms", fd, socket->recv_timeout);
    } else if (type == TRANS_SEND) {
        if (ms_timeout > 0) {
            socket->send_timeout = ms_timeout;
        }
        // PR_DEBUG("Set send timeout for socket %d: %d ms", fd, socket->send_timeout);
    }
    tal_mutex_unlock(socket->mutex);
    PR_DEBUG("<-- unlock socket mutex for fd: %d", fd);

    return OPRT_OK;
}

OPERATE_RET tal_at_net_set_bufsize(const int fd, const int buf_size, const TUYA_TRANS_TYPE_E type)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_set_reuse(const int fd)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_disable_nagle(const int fd)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_set_broadcast(const int fd)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_gethostbyname(const char *domain, TUYA_IP_ADDR_T *addr)
{
    OPERATE_RET rt = OPRT_OK;

    if (!sg_at_modem.mutex) {
        return OPRT_COM_ERROR;
    }

    if (domain == NULL || addr == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(sg_at_modem.mutex);
    PR_DEBUG("-> lock sg_at_modem.mutex");
    if (sg_at_modem.ops.at_gethostbyname) {
        rt = sg_at_modem.ops.at_gethostbyname(domain, addr);
    }
    tal_mutex_unlock(sg_at_modem.mutex);
    PR_DEBUG("<- unlock sg_at_modem.mutex");

    return rt;
}

OPERATE_RET tal_at_net_set_keepalive(int fd, const BOOL_T alive, const uint32_t idle, const uint32_t intr,
                                     const uint32_t cnt)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_get_socket_ip(int fd, TUYA_IP_ADDR_T *addr)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

TUYA_IP_ADDR_T tal_at_net_str2addr(const char *ip_str)
{
    return at_utils_str2addr(ip_str);
}

char *tal_at_net_addr2str(TUYA_IP_ADDR_T ipaddr)
{
    return at_utils_addr2str(ipaddr);
}

OPERATE_RET tal_at_net_setsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname,
                                  const void *optval, const int optlen)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tal_at_net_getsockopt(const int fd, const TUYA_OPT_LEVEL level, const TUYA_OPT_NAME optname, void *optval,
                                  int *optlen)
{
    PR_ERR("[%s] not supported", __func__);
    return OPRT_NOT_SUPPORTED;
}
