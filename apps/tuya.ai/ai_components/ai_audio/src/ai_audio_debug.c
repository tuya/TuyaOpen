/**
 * @file tuya_audio_debug.c
 * @brief Implements audio debugging functionality for network communication
 *
 * This source file provides the implementation of audio debugging functionalities
 * required for network communication. It includes functionality for audio stream
 * handling, TCP connection management, data transmission, and ring buffer operations.
 * The implementation supports multiple audio stream types, TCP connections, and
 * data upload mechanisms. This file is essential for developers working on IoT
 * applications that require real-time audio debugging and network communication.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include "tal_api.h"
#include "tal_network.h"
#include "tuya_ringbuf.h"
#include "ai_audio_debug.h"

#if defined(AI_AUDIO_DEBUG) && (AI_AUDIO_DEBUG == 1)

#define TUYA_AUDIO_DEBUG_MAX_CONNECTIONS 1

#define TCP_SERVER_IP   "192.168.1.238"
#define TCP_SERVER_PORT 5055

static TUYA_IP_ADDR_T server_ip;
static TUYA_ERRNO net_errno = 0;
static TUYA_RINGBUFF_T audio_ringbufs[DEBUG_UPLOAD_STREAM_TYPE_MAX];
static int sock_fds[DEBUG_UPLOAD_STREAM_TYPE_MAX] = {-1, -1, -1, -1};
static uint8_t *audio_buf;
static MUTEX_HANDLE s_ringbuf_mutex;

/**
 * @brief Write data to the audio debug stream.
 *
 * @param type The type of the debug stream.
 * @param buf Pointer to the data buffer to write.
 * @param len The length of the data to write in bytes.
 * @return int - The number of bytes written on success, or OPRT_COM_ERROR on failure.
 */
static int __ai_audio_debug_stream_write(DEBUG_UPLOAD_STREAM_TYPE type, char *buf, uint32_t len)
{
    if (type >= TUYA_AUDIO_DEBUG_MAX_CONNECTIONS || sock_fds[type] < 0) {
        return len;
    }

    int ret = 0, write_size = 0;
    TUYA_RINGBUFF_T audio_ringbuf = audio_ringbufs[type];
    tal_mutex_lock(s_ringbuf_mutex);
    ret = tuya_ring_buff_write(audio_ringbuf, buf, len);
    tal_mutex_unlock(s_ringbuf_mutex);
    if (ret != len) {
        PR_ERR("tuya_ring_buff_write failed, ret=%d", ret);
        return OPRT_COM_ERROR;
    }

    write_size = ret;
    return write_size;
}

/**
 * @brief Read data from the audio debug stream.
 *
 * @param type The type of the debug stream.
 * @param buf Pointer to the buffer where the read data will be stored.
 * @param len The maximum number of bytes to read.
 * @return int - The number of bytes read on success, or OPRT_COM_ERROR on failure.
 */
static int __ai_audio_debug_stream_read(DEBUG_UPLOAD_STREAM_TYPE type, char *buf, uint32_t len)
{
    if (type >= TUYA_AUDIO_DEBUG_MAX_CONNECTIONS || sock_fds[type] < 0) {
        return len;
    }

    int ret = 0, read_size = 0;
    TUYA_RINGBUFF_T audio_ringbuf = audio_ringbufs[type];
    tal_mutex_lock(s_ringbuf_mutex);
    ret = tuya_ring_buff_read(audio_ringbuf, buf, len);
    tal_mutex_unlock(s_ringbuf_mutex);
    if (ret < 0) {
        PR_ERR("tuya_ring_buff_read failed, ret=%d", ret);
        return OPRT_COM_ERROR;
    }

    read_size = ret;
    return read_size;
}

/**
 * @brief Peek at data in the audio debug stream without removing it.
 *
 * @param type The type of the debug stream.
 * @param buf Pointer to the buffer where the peeked data will be stored.
 * @param len The maximum number of bytes to peek.
 * @return int - The number of bytes peeked on success, or OPRT_COM_ERROR on failure.
 */
static int __ai_audio_debug_stream_peek(DEBUG_UPLOAD_STREAM_TYPE type, char *buf, uint32_t len)
{
    if (type >= TUYA_AUDIO_DEBUG_MAX_CONNECTIONS || sock_fds[type] < 0) {
        return len;
    }

    int ret = 0, read_size = 0;
    TUYA_RINGBUFF_T audio_ringbuf = audio_ringbufs[type];
    tal_mutex_lock(s_ringbuf_mutex);
    ret = tuya_ring_buff_peek(audio_ringbuf, buf, len);
    tal_mutex_unlock(s_ringbuf_mutex);
    if (ret < 0) {
        PR_ERR("tuya_ring_buff_peek failed, ret=%d", ret);
        return OPRT_COM_ERROR;
    }

    read_size = ret;
    return read_size;
}

/**
 * @brief Clear the audio debug stream.
 *
 * @param type The type of the debug stream to clear.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
static OPERATE_RET __ai_audio_debug_stream_clear(DEBUG_UPLOAD_STREAM_TYPE type)
{
    OPERATE_RET ret;
    TUYA_RINGBUFF_T audio_ringbuf = audio_ringbufs[type];
    tal_mutex_lock(s_ringbuf_mutex);
    ret = tuya_ring_buff_reset(audio_ringbuf);
    tal_mutex_unlock(s_ringbuf_mutex);
    return ret;
}

/**
 * @brief Get the current size of the audio debug stream.
 *
 * @param type The type of the debug stream.
 * @return int - The size of the stream in bytes.
 */
static int __ai_audio_debug_stream_get_size(DEBUG_UPLOAD_STREAM_TYPE type)
{
    int size = 0;
    TUYA_RINGBUFF_T audio_ringbuf = audio_ringbufs[type];
    tal_mutex_lock(s_ringbuf_mutex);
    size = tuya_ring_buff_used_size_get(audio_ringbuf);
    tal_mutex_unlock(s_ringbuf_mutex);
    return size;
}

/**
 * @brief Connect to a TCP server by port.
 *
 * @param fd Pointer to an integer where the file descriptor will be stored.
 * @param ip_addr The IP address of the server.
 * @param port The port number of the server.
 * @return OPERATE_RET - OPRT_OK on success, or OPRT_COM_ERROR on failure.
 */
static OPERATE_RET _tcp_connect_by_port(int *fd, const char *ip_addr, uint16_t port)
{
    if (*fd >= 0) {
        tal_net_close(*fd);
        *fd = -1;
    }
    // create socket
    *fd = tal_net_socket_create(PROTOCOL_TCP);
    if (*fd < 0) {
        PR_ERR("create socket err");
        *fd = -1;
    }
    PR_NOTICE("create socket success, fd=%d", *fd);
    server_ip = tal_net_str2addr(ip_addr);
    PR_NOTICE("connect tcp server ip: %s, port: %d", ip_addr, port);
    net_errno = tal_net_connect(*fd, server_ip, port);
    if (net_errno < 0) {
        PR_ERR("connect fail, exit");
        tal_net_close(*fd);
        *fd = -1;
        return OPRT_COM_ERROR;
    }
    PR_NOTICE("connect to %s:%d success", ip_addr, port);
    return OPRT_OK;
}

/**
 * @brief Connect to all TCP servers for audio debugging.
 *
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
static OPERATE_RET __ai_audio_debug_tcp_connect(void)
{
    OPERATE_RET rt = OPRT_OK;
    int sock_fd = -1;
    int i = 0;

    for (i = 0; i < TUYA_AUDIO_DEBUG_MAX_CONNECTIONS; i++) {
        rt = _tcp_connect_by_port(&sock_fds[i], TCP_SERVER_IP, TCP_SERVER_PORT + i);
        if (rt != OPRT_OK) {
            PR_ERR("connect fail, exit");
            return rt;
        }
    }

    return rt;
}

/**
 * @brief Send data over a TCP connection.
 *
 * @param type The type of the debug stream.
 * @param data Pointer to the data to send.
 * @param len The length of the data to send in bytes.
 * @return int - The number of bytes sent on success, or -1 on failure.
 */
static int __ai_audio_debug_tcp_send(int type, char *data, int len)
{
    if (type >= TUYA_AUDIO_DEBUG_MAX_CONNECTIONS || sock_fds[type] < 0) {
        return -1;
    }

    int rt = 0;
    rt = tal_net_send(sock_fds[type], data, len);
    if (rt < 0) {
        PR_ERR("send fail, exit");
        tal_net_close(sock_fds[type]);
        return -1;
    }
    return rt;
}

/**
 * @brief Close all TCP connections.
 *
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
static OPERATE_RET __ai_audio_debug_tcp_close_all(void)
{
    OPERATE_RET rt = OPRT_OK;
    int i = 0;
    for (i = 0; i < TUYA_AUDIO_DEBUG_MAX_CONNECTIONS; i++) {
        if (sock_fds[i] >= 0) {
            tal_net_close(sock_fds[i]);
            sock_fds[i] = -1;
        }
    }
    return rt;
}

/**
 * @brief Close a specific TCP connection.
 *
 * @param type The type of the debug stream.
 * @return OPERATE_RET - OPRT_OK on success, or OPRT_OK if the connection was already closed.
 */
static OPERATE_RET __ai_audio_debug_tcp_close(int type)
{
    OPERATE_RET rt = OPRT_OK;
    if (type >= TUYA_AUDIO_DEBUG_MAX_CONNECTIONS || sock_fds[type] < 0) {
        return rt;
    }
    tal_net_close(sock_fds[type]);
    return rt;
}

/**
 * @brief Initializes the audio debug module.
 * @param None
 * @return OPERATE_RET - OPRT_OK if initialization is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_debug_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&s_ringbuf_mutex));

    // init audio ringbuf
    for (int i = 0; i < TUYA_AUDIO_DEBUG_MAX_CONNECTIONS; i++) {
        rt = tuya_ring_buff_create(32000 * 10, OVERFLOW_PSRAM_STOP_TYPE, &audio_ringbufs[i]);
        if (rt != OPRT_OK) {
            PR_ERR("tuya_ring_buff_init failed, ret=%d", rt);
            return rt;
        }
    }

    // init audio_buf
    audio_buf = (uint8_t *)tal_malloc(3200);

    return OPRT_OK;
}

/**
 * @brief Starts audio debugging by establishing TCP connections.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the connections are successfully established, otherwise an error code.
 */
OPERATE_RET ai_audio_debug_start(void)
{
    OPERATE_RET rt = OPRT_OK;
    int i = 0;

    // close all tcp connections if exist
    for (i = 0; i < TUYA_AUDIO_DEBUG_MAX_CONNECTIONS; i++) {
        if (sock_fds[i] >= 0) {
            tal_net_close(sock_fds[i]);
            sock_fds[i] = -1;
        }
    }

    // create new tcp connections
    for (i = 0; i < TUYA_AUDIO_DEBUG_MAX_CONNECTIONS; i++) {
        rt = _tcp_connect_by_port(&sock_fds[i], TCP_SERVER_IP, TCP_SERVER_PORT + i);
        if (rt != OPRT_OK) {
            PR_ERR("connect fail, exit");
            return rt;
        }
    }

    return rt;
}

/**
 * @brief Handles and uploads audio data for debugging.
 * @param buf Pointer to the data buffer containing audio data.
 * @param len The length of the data buffer in bytes.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_debug_data(char *buf, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    int i = 0;

    if (NULL == buf || 0 == len) {
        return OPRT_INVALID_PARM;
    }

    // upload buf to the first tcp connection
    rt = __ai_audio_debug_tcp_send(DEBUG_UPLOAD_STREAM_TYPE_RAW, buf, len);

    for (i = DEBUG_UPLOAD_STREAM_TYPE_MIC; i < TUYA_AUDIO_DEBUG_MAX_CONNECTIONS; i++) {
        if (sock_fds[i] >= 0) {
            // read from ringbuf
            int read_size = __ai_audio_debug_stream_read(i, audio_buf, len);
            if (read_size <= 0) {
                PR_ERR("tuya_audio_debug_stream_read failed, ret=%d", read_size);
                return OPRT_COM_ERROR;
            }
            rt = __ai_audio_debug_tcp_send(i, audio_buf, read_size);
            if (rt < 0) {
                PR_ERR("send fail, exit");
                return rt;
            }
        }
    }

    return OPRT_OK;
}

/**
 * @brief Stops audio debugging by closing all TCP connections.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success.
 */
OPERATE_RET ai_audio_debug_stop(void)
{
    // close all tcp connections
    __ai_audio_debug_tcp_close_all();
    return OPRT_OK;
}

#endif
