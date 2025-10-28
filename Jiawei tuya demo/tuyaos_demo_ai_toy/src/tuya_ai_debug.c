#include "tuya_ai_debug.h"
#include <stdio.h>
#include "tal_log.h"
#include "tal_memory.h"
#include "tuya_ringbuf.h"
#include "tal_system.h"
#include "tal_mutex.h"
#include "tuya_iot_com_api.h"

#if defined(TUYA_UPLOAD_DEBUG) && (TUYA_UPLOAD_DEBUG == 1)

// #define TY_DEBUG_MAX_CONNECTIONS 4
#define TY_DEBUG_MAX_CONNECTIONS 1

// #define TCP_SERVER_IP "192.168.32.145"
// #define TCP_SERVER_IP "192.168.28.150"
#define TCP_SERVER_IP "192.168.32.160"
#define TCP_SERVER_PORT 5055
STATIC TUYA_IP_ADDR_T server_ip;
STATIC TUYA_ERRNO net_errno = 0;
STATIC TUYA_RINGBUFF_T audio_ringbufs[_DEBUG_UPLOAD_STREAM_TYPE_MAX];
STATIC INT_T sock_fds[_DEBUG_UPLOAD_STREAM_TYPE_MAX] = {-1, -1, -1, -1};
STATIC UCHAR_T *audio_buf;
STATIC MUTEX_HANDLE s_ringbuf_mutex;

INT_T ty_debug_audio_stream_write(DEBUG_UPLOAD_STREAM_TYPE type, CHAR_T *buf, UINT_T len)
{
    if (type >= TY_DEBUG_MAX_CONNECTIONS || sock_fds[type] < 0) {
        return len;
    }

    INT_T ret = 0, write_size = 0;
    TUYA_RINGBUFF_T audio_ringbuf = audio_ringbufs[type];
    tal_mutex_lock(s_ringbuf_mutex);
    ret = tuya_ring_buff_write(audio_ringbuf, buf, len);
    tal_mutex_unlock(s_ringbuf_mutex);
    if (ret != len) {
        TAL_PR_ERR("tuya_ring_buff_write failed, ret=%d", ret);
        return OPRT_COM_ERROR;
    }

    write_size = ret;
    return write_size;
}

INT_T ty_debug_audio_stream_read(DEBUG_UPLOAD_STREAM_TYPE type, CHAR_T *buf, UINT_T len)
{
    if (type >= TY_DEBUG_MAX_CONNECTIONS || sock_fds[type] < 0) {
        return len;
    }

    INT_T ret = 0, read_size = 0;
    TUYA_RINGBUFF_T audio_ringbuf = audio_ringbufs[type];
    tal_mutex_lock(s_ringbuf_mutex);
    ret = tuya_ring_buff_read(audio_ringbuf, buf, len);
    tal_mutex_unlock(s_ringbuf_mutex);
    if (ret < 0) {
        TAL_PR_ERR("tuya_ring_buff_read failed, ret=%d", ret);
        return OPRT_COM_ERROR;
    }

    read_size = ret;
    return read_size;
}

INT_T ty_debug_audio_stream_peek(DEBUG_UPLOAD_STREAM_TYPE type, CHAR_T *buf, UINT_T len)
{
    if (type >= TY_DEBUG_MAX_CONNECTIONS || sock_fds[type] < 0) {
        return len;
    }

    INT_T ret = 0, read_size = 0;
    TUYA_RINGBUFF_T audio_ringbuf = audio_ringbufs[type];
    tal_mutex_lock(s_ringbuf_mutex);
    ret = tuya_ring_buff_peek(audio_ringbuf, buf, len);
    tal_mutex_unlock(s_ringbuf_mutex);
    if (ret < 0) {
        TAL_PR_ERR("tuya_ring_buff_peek failed, ret=%d", ret);
        return OPRT_COM_ERROR;
    }

    read_size = ret;
    return read_size;
}

OPERATE_RET ty_debug_audio_stream_clear(DEBUG_UPLOAD_STREAM_TYPE type)
{
    OPERATE_RET ret;
    TUYA_RINGBUFF_T audio_ringbuf = audio_ringbufs[type];
    tal_mutex_lock(s_ringbuf_mutex);
    ret = tuya_ring_buff_reset(audio_ringbuf);
    tal_mutex_unlock(s_ringbuf_mutex);
    return ret;
}

INT_T ty_debug_audio_stream_get_size(DEBUG_UPLOAD_STREAM_TYPE type)
{
    INT_T size = 0;
    TUYA_RINGBUFF_T audio_ringbuf = audio_ringbufs[type];
    tal_mutex_lock(s_ringbuf_mutex);
    size = tuya_ring_buff_used_size_get(audio_ringbuf);
    tal_mutex_unlock(s_ringbuf_mutex);
    return size;
}

STATIC OPERATE_RET _tcp_connect_by_port(INT_T *fd, CONST CHAR_T *ip_addr, UINT16_T port)
{
    if (*fd >= 0) {
        tal_net_close(*fd);
        *fd = -1;
    }
    // create socket
    *fd = tal_net_socket_create(PROTOCOL_TCP);
    if (*fd < 0) {
        TAL_PR_ERR("create socket err");
        *fd = -1;
    }
    TAL_PR_NOTICE("create socket success, fd=%d", *fd);
    server_ip = tal_net_str2addr(ip_addr);
    TAL_PR_NOTICE("connect tcp server ip: %s, port: %d", ip_addr, port);
    net_errno = tal_net_connect(*fd, server_ip, port);
    if (net_errno < 0) {
        TAL_PR_ERR("connect fail, exit");
        tal_net_close(*fd);
        *fd = -1;
        return OPRT_COM_ERROR;
    }
    TAL_PR_NOTICE("connect to %s:%d success", ip_addr, port);
    return OPRT_OK;
}

OPERATE_RET ty_debug_tcp_connect(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    INT_T sock_fd = -1;
    INT_T i = 0;

    for (i = 0; i < TY_DEBUG_MAX_CONNECTIONS; i++) {
        rt = _tcp_connect_by_port(&sock_fds[i], TCP_SERVER_IP, TCP_SERVER_PORT + i);
        if (rt != OPRT_OK) {
            TAL_PR_ERR("connect fail, exit");
            return rt;
        }
    }

    return rt;
}

INT_T ty_debug_tcp_send(INT_T type, CHAR_T *data, INT_T len)
{
    if (type >= TY_DEBUG_MAX_CONNECTIONS || sock_fds[type] < 0) {
        return -1;
    }
    
    INT_T rt = 0;
    rt = tkl_net_send(sock_fds[type], data, len);
    if (rt < 0) {
        TAL_PR_ERR("send fail, exit");
        tkl_net_close(sock_fds[type]);
        return -1;
    }
    return rt;
}

OPERATE_RET ty_debug_tcp_close_all(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    INT_T i = 0;
    for (i = 0; i < TY_DEBUG_MAX_CONNECTIONS; i++) {
        if (sock_fds[i] >= 0) {
            tkl_net_close(sock_fds[i]);
            sock_fds[i] = -1;
        }
    }
    return rt;
}

OPERATE_RET ty_debug_tcp_close(INT_T type)
{
    OPERATE_RET rt = OPRT_OK;
    if (type >= TY_DEBUG_MAX_CONNECTIONS || sock_fds[type] < 0) {
        return rt;
    }
    tkl_net_close(sock_fds[type]);
    return rt;
}

OPERATE_RET ty_debug_init(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&s_ringbuf_mutex));

    // init audio ringbuf
    for (INT_T i = 0; i < TY_DEBUG_MAX_CONNECTIONS; i++) {
        rt = tuya_ring_buff_create(32000 * 10, OVERFLOW_PSRAM_STOP_TYPE, &audio_ringbufs[i]);
        if (rt != OPRT_OK) {
            TAL_PR_ERR("tuya_ring_buff_init failed, ret=%d", rt);
            return rt;
        }
    }

    // init audio_buf
    audio_buf = (UCHAR_T *)tal_malloc(3200);

    return OPRT_OK;
}

OPERATE_RET ty_debug_upload_start_cb(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    INT_T i = 0;

    // close all tcp connections if exist
    for (i = 0; i < TY_DEBUG_MAX_CONNECTIONS; i++) {
        if (sock_fds[i] >= 0) {
            tkl_net_close(sock_fds[i]);
            sock_fds[i] = -1;
        }
    }

    // create new tcp connections
    for (i = 0; i < TY_DEBUG_MAX_CONNECTIONS; i++) {
        rt = _tcp_connect_by_port(&sock_fds[i], TCP_SERVER_IP, TCP_SERVER_PORT + i);
        if (rt != OPRT_OK) {
            TAL_PR_ERR("connect fail, exit");
            return rt;
        }
    }

    return rt;
}

OPERATE_RET ty_debug_upload_data_cb(CHAR_T *buf, UINT_T len)
{
    OPERATE_RET rt = OPRT_OK;
    INT_T i = 0;

    // upload buf to the first tcp connection
    rt = ty_debug_tcp_send(DEBUG_UPLOAD_STREAM_TYPE_RAW, buf, len);

    for (i = DEBUG_UPLOAD_STREAM_TYPE_MIC; i < TY_DEBUG_MAX_CONNECTIONS; i++) {
        if (sock_fds[i] >= 0) {
            // read from ringbuf
            INT_T read_size = ty_debug_audio_stream_read(i, audio_buf, len);
            if (read_size <= 0) {
                TAL_PR_ERR("ty_debug_audio_stream_read failed, ret=%d", read_size);
                return OPRT_COM_ERROR;
            }
            rt = ty_debug_tcp_send(i, audio_buf, read_size);
            if (rt < 0) {
                TAL_PR_ERR("send fail, exit");
                return rt;
            }
        }
    }

    return OPRT_OK;
}

OPERATE_RET ty_debug_upload_stop_cb(VOID)
{
    // close all tcp connections
    ty_debug_tcp_close_all();
    return OPRT_OK;
}

#endif
