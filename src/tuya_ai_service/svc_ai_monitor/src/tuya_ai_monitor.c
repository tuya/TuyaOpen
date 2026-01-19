/**
 * @file tuya_ai_monitor.c
 * @author tuya
 * @brief TUYA AI monitor service implementation
 * @version 0.1
 * @date 2025-06-09
 *
 * @copyright Copyright 2014-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_ai_monitor.h"
#include "tuya_ai_protocol.h"
#include "tuya_ai_biz.h"
#include "tuya_svc_netmgr.h"
#include "tuya_svc_devos.h"
#include "tuya_ai_biz.h"
#include "lan_sock.h"
#include "gw_intf.h"
#include "uni_random.h"
#include "uni_log.h"
#include "tal_memory.h"
#include "tal_network.h"
#include "tal_system.h"
#include "tal_sw_timer.h"
#include "tal_network.h"
#include "tal_mutex.h"
#include "tal_queue.h"
#include "tal_thread.h"
#include <string.h>

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM==1)
#define OS_MALLOC(size)    tal_psram_malloc(size)
#define OS_FREE(ptr)       tal_psram_free(ptr)
#define OS_CALLOC(num, size) tal_psram_calloc(num, size)
#define OS_REALLOC(ptr, size) tal_psram_realloc(ptr, size)
#else
#define OS_MALLOC(size)    tal_malloc(size)
#define OS_FREE(ptr)       tal_free(ptr)
#define OS_CALLOC(num, size) tal_calloc(num, size)
#define OS_REALLOC(ptr, size) tal_realloc(ptr, size)
#endif

#define AI_MONITOR_MAX_CLIENTS_MIN       1
#define AI_MONITOR_MAX_CLIENTS_MAX       3

// Protocol magic number
#define AI_MONITOR_MAGIC                0x54594149

#define AI_MONITOR_TAG                   "AI_MON"

// Protocol version
#define AI_MONITOR_VERSION              0x01

#define AI_MONITOR_DIR_US 0             // Device upload to cloud
#define AI_MONITOR_DIR_DS 1             // Cloud download to device
#define AI_MONITOR_DIR_ACK 2            // Device ack to client
#define AI_MONITOR_DIR_MAX 3            // Maximum direction type

#pragma pack(1)
typedef struct {
    uint32_t magic;                     // magic number for frame synchronization
    uint8_t  reserved : 6;              // reserved bits
    uint8_t  direction : 2;             // direction: 0 for device upload, 1 for cloud download, 2 for device ack to client
    AI_PACKET_HEAD_T pkg_header;        // Base 2.0 Protocol header
} ai_monitor_header_t;
#pragma pack()

typedef struct {
    int fd;                           // socket fd
    TUYA_IP_ADDR_T addr;                // client address
    BOOL_T connected;                   // connection status
    uint64_t last_ping_time;            // last ping time
    uint16_t sequence;                  // sequence number
    uint8_t *recv_buf;                  // receive buffer
    uint32_t recv_buf_size;               // receive buffer size
    uint32_t recv_len;                    // received data length
    uint8_t registered_types[8];        // registered data types bitmap, max 64 types
} ai_monitor_client_t;

typedef struct {
    BOOL_T initialized;                 // initialized flag
    BOOL_T running;                     // running flag
    ai_monitor_config_t config;         // server configuration
    netmgr_linkage_t *linkage;          // default lan linkage
    int server_fd;                    // server socket fd
    ai_monitor_client_t *clients;       // client array
    uint32_t client_count;                // current client count
    uint16_t sequence;                  // global sequence number
    uint32_t session_id;                // current session ID
    MUTEX_HANDLE mutex;                 // mutex for thread safety
    TIMER_ID timer;                     // timer ID
    THREAD_HANDLE log_thread;           // log thread handle
    BOOL_T log_thread_running;          // log thread running flag
    QUEUE_HANDLE log_queue;             // log queue
} ai_monitor_server_t;

typedef struct {
    AI_PACKET_WRITER_T *writer;         // packet writer
    int fd;                           // socket fd
    uint8_t direction;                  // direction: 0 for device upload, 1 for cloud download, 2 for device ack to client
    uint16_t sequence_out;              // sequence number for outgoing packets
    uint32_t frag_offset[AI_MONITOR_DIR_MAX]; // offset for upstream/downstream/ack packet fragments
} ai_monitor_writer_cfg_t;

STATIC ai_monitor_server_t g_ai_monitor_server = {0};
STATIC OPERATE_RET __init_client(ai_monitor_client_t *client, int fd, TUYA_IP_ADDR_T addr);
STATIC VOID __cleanup_client(ai_monitor_client_t *client);
STATIC ai_monitor_client_t* __find_client_by_fd(int fd);
STATIC OPERATE_RET __parse_pkg(ai_monitor_client_t *client, char *data, uint32_t len);
STATIC VOID __accept_handler(int32_t server_sock);
STATIC VOID __accept_err(int fd);
STATIC VOID __socket_read_handler(int32_t sock);
STATIC VOID __socket_error_handler(int sock);
STATIC OPERATE_RET __client_register_clear(ai_monitor_client_t *client);
STATIC OPERATE_RET __client_register(ai_monitor_client_t *client, uint8_t type);
STATIC BOOL_T __is_client_registered(ai_monitor_client_t *client, uint8_t type);
STATIC VOID __session_close_all(VOID);
STATIC OPERATE_RET __default_update(AI_STAGE_E stage, VOID *data, AI_SEND_PACKET_T *info);
STATIC OPERATE_RET __default_write(AI_PACKET_WRITER_T *writer, VOID *buf, uint32_t buf_len);
STATIC VOID __log_output(CONST char *str);

ai_monitor_writer_cfg_t s_monitor_writer_cfg = {
    .writer = NULL,         // Will be set later
    .fd = -1,
    .direction = 0,         // Default to device upload,
    .sequence_out = 1,
};

AI_PACKET_WRITER_T s_default_writer = {
    .update = __default_update,
    .write = __default_write,
    .user_data = &s_monitor_writer_cfg,
};

#define AI_MONITOR_WRITER_UPDATE(_writer, _fd, _direction) \
    do { \
        ((ai_monitor_writer_cfg_t *)_writer->user_data)->writer = &s_default_writer; \
        ((ai_monitor_writer_cfg_t *)_writer->user_data)->fd = (_fd); \
        ((ai_monitor_writer_cfg_t *)_writer->user_data)->direction = (_direction); \
    } while (0)

/**
 * @brief Initialize client structure
 */
STATIC OPERATE_RET __init_client(ai_monitor_client_t *client, int fd, TUYA_IP_ADDR_T addr)
{
    if (!client) {
        return OPRT_INVALID_PARM;
    }

    memset(client, 0, sizeof(ai_monitor_client_t));
    client->fd = fd;
    client->addr = addr;
    client->connected = TRUE;
    client->last_ping_time = tal_time_get_posix_ms();
    client->sequence = 0;
    client->recv_buf_size = g_ai_monitor_server.config.recv_buf_size;

    client->recv_buf = OS_MALLOC(client->recv_buf_size);
    if (!client->recv_buf) {
        PR_ERR("malloc recv buffer failed");
        return OPRT_MALLOC_FAILED;
    }

    client->recv_len = 0;
    __client_register_clear(client); // Clear registered types

    PR_DEBUG("Initialized client fd=%d, addr=0x%08x", fd, addr);
    return OPRT_OK;
}

/**
 * @brief Cleanup client structure
 */
STATIC VOID __cleanup_client(ai_monitor_client_t *client)
{
    if (!client) {
        return;
    }

    if (client->recv_buf) {
        OS_FREE(client->recv_buf);
        client->recv_buf = NULL;
    }

    if (client->fd >= 0) {
        tuya_unreg_lan_sock(client->fd);
        // tal_net_close(client->fd);
        client->fd = -1;
    }

    client->connected = FALSE;
    client->recv_len = 0;
    __client_register_clear(client); // Clear registered types
}

/**
 * @brief Find client by socket fd
 */
STATIC ai_monitor_client_t* __find_client_by_fd(int fd)
{
    for (uint32_t i = 0; i < g_ai_monitor_server.config.max_clients; i++) {
        if (g_ai_monitor_server.clients[i].fd == fd) {
            return &g_ai_monitor_server.clients[i];
        }
    }
    return NULL;
}

STATIC BOOL_T __is_client_registered(ai_monitor_client_t *client, uint8_t type)
{
    if (!client || type >= sizeof(client->registered_types) * 8 || !client->connected) {
        return FALSE;
    }
    return (client->registered_types[type / 8] & (1 << (type % 8))) != 0;
}

STATIC OPERATE_RET __client_register(ai_monitor_client_t *client, uint8_t type)
{
    if (!client || type >= sizeof(client->registered_types) * 8 || !client->connected) {
        return OPRT_INVALID_PARM;
    }

    client->registered_types[type / 8] |= (1 << (type % 8));
    PR_DEBUG("Client fd=%d registered type=%d", client->fd, type);
    return OPRT_OK;
}

STATIC OPERATE_RET __client_register_clear(ai_monitor_client_t *client)
{
    memset(client->registered_types, 0, sizeof(client->registered_types));
    PR_TRACE("Client fd=%d cleared all registered types", client->fd);
    return OPRT_OK;
}

/**
 * @brief send message to specific client
 */
STATIC OPERATE_RET __pack_and_send(ai_monitor_client_t *client, uint8_t direction, uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char *data)
{
    OPERATE_RET rt = OPRT_OK;
    if (!client || !client->connected || !head || !attr) {
        return OPRT_INVALID_PARM;
    }

    AI_PACKET_WRITER_T *writer = &s_default_writer;
    AI_MONITOR_WRITER_UPDATE(writer, client->fd, direction); // Update writer with client fd and direction
    rt = tuya_ai_send_biz_pkt_custom(id, attr, attr->type, head, data, writer);
    if (rt != OPRT_OK) {
        PR_ERR("send biz data failed, rt:%d", rt);
        return rt;
    }
    PR_TRACE("Sent data to client fd=%d, id=%d, type=%d, head len=%d, data len=%d",
             client->fd, id, attr->type, head->len, head->total_len);
    return OPRT_OK;
}

STATIC OPERATE_RET __handle_ping(ai_monitor_client_t *client,  uint64_t client_ts, char *payload)
{
    OPERATE_RET rt = OPRT_OK;
    uint64_t server_ts;

    server_ts = client->last_ping_time = tal_time_get_posix_ms();
    PR_DEBUG("Received ping from client fd=%d, client_ts=%llu", client->fd, client_ts);

    // response ai pong
    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_PONG;
    pkt.writer = &s_default_writer;
    AI_MONITOR_WRITER_UPDATE(pkt.writer, client->fd, AI_MONITOR_DIR_ACK); // Update writer with client fd and ACK direction
    // create pong attrs
    uint32_t attr_idx = 0;
    pkt.attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_CLIENT_TS, ATTR_PT_U64, &client_ts, SIZEOF(uint64_t));
    pkt.attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SERVER_TS, ATTR_PT_U64, &server_ts, SIZEOF(uint64_t));
    pkt.count = attr_idx;
    // TODO: check attr created
    // if (!__ai_check_attr_created(&pkt)) {
    //     return OPRT_MALLOC_FAILED;
    // }
    rt = tuya_ai_basic_pkt_send(&pkt);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to send pong response, rt: %d", rt);
        return rt;
    }
    PR_DEBUG("Handled ping for client fd=%d, client_ts=%llu, server_ts=%llu",
             client->fd, client_ts, server_ts);
    return OPRT_OK;
}

STATIC OPERATE_RET __handle_event_filter(ai_monitor_client_t *client, AI_EVENT_ATTR_T *event)
{
    uint64_t user_data_bitmap = 0;

    if (event->user_len != sizeof(uint64_t)) {
        return OPRT_INVALID_PARM;
    }
    memcpy(&user_data_bitmap, event->user_data, sizeof(uint64_t));
    UNI_NTOHLL(user_data_bitmap);
    PR_DEBUG("Monitor Filter User data bitmap: 0x%016llx", user_data_bitmap);

    __client_register_clear(client); // Clear all registered types
    if (user_data_bitmap & (1ULL << AI_PT_VIDEO)) {
        __client_register(client, AI_PT_VIDEO);
    }
    if (user_data_bitmap & (1ULL << AI_PT_AUDIO)) {
        __client_register(client, AI_PT_AUDIO);
    }
    if (user_data_bitmap & (1ULL << AI_PT_IMAGE)) {
        __client_register(client, AI_PT_IMAGE);
    }
    if (user_data_bitmap & (1ULL << AI_PT_FILE)) {
        __client_register(client, AI_PT_FILE);
    }
    if (user_data_bitmap & (1ULL << AI_PT_TEXT)) {
        __client_register(client, AI_PT_TEXT);
    }
    if (user_data_bitmap & (1ULL << AI_PT_EVENT)) {
        __client_register(client, AI_PT_EVENT);
    }
    if (user_data_bitmap & (1ULL << AI_PT_CUSTOM_LOG)) {
        __client_register(client, AI_PT_CUSTOM_LOG);
        tal_log_add_output_term(AI_MONITOR_TAG, __log_output);
    } else {
        tal_log_del_output_term(AI_MONITOR_TAG);
    }

    return OPRT_OK;
}

STATIC OPERATE_RET __handle_event_alg_ctrl(ai_monitor_client_t *client, AI_EVENT_ATTR_T *event)
{
    return OPRT_NOT_SUPPORTED; // Not implemented yet, return not supported
}

STATIC OPERATE_RET __handle_event(ai_monitor_client_t *client, AI_EVENT_ATTR_T *event, char *payload)
{
    OPERATE_RET rt = OPRT_OK;
    AI_EVENT_HEAD_T *head = (AI_EVENT_HEAD_T *)payload;
    AI_EVENT_TYPE event_type = UNI_NTOHS(head->type);

    PR_DEBUG("Received event: session_id=%s, event_id=%s, user_len=%u, event_type=%d",
             event->session_id, event->event_id, event->user_len, event_type);

    if (event_type == AI_EVENT_MONITOR_FILTER) {
        // Handle event filter
        rt = __handle_event_filter(client, event);
    } else if (event_type == AI_EVENT_MONITOR_ALG_CTRL) {
        // Handle algorithm control event
        rt = __handle_event_alg_ctrl(client, event);
    } else {
        // Unsupported event type
        PR_ERR("Unsupported event type: %d", event_type);
        rt = OPRT_NOT_SUPPORTED;
    }

    AI_SEND_PACKET_T pkt = {0};
    pkt.type = AI_PT_EVENT;
    pkt.writer = &s_default_writer;
    AI_MONITOR_WRITER_UPDATE(pkt.writer, client->fd, AI_MONITOR_DIR_ACK); // Update writer with client fd and ACK direction
    // create event attrs
    uint32_t attr_idx = 0;
    pkt.attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_SESSION_ID, ATTR_PT_STR, event->session_id, strlen(event->session_id));
    pkt.attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_EVENT_ID, ATTR_PT_STR, event->event_id, strlen(event->event_id));
    pkt.attrs[attr_idx++] = tuya_ai_create_attribute(AI_ATTR_USER_DATA, ATTR_PT_BYTES, event->user_data, event->user_len);
    pkt.count = attr_idx;
    // TODO: check attr created
    // if (!__ai_check_attr_created(&pkt)) {
    //     return OPRT_MALLOC_FAILED;
    // }
    char resp_payload[sizeof(AI_EVENT_HEAD_T) + sizeof(uint32_t)] = {0};
    ((AI_EVENT_HEAD_T *)resp_payload)->type = UNI_HTONS(event_type);
    ((AI_EVENT_HEAD_T *)resp_payload)->length = UNI_HTONS(sizeof(uint32_t));
    *((uint32_t *)(resp_payload + sizeof(AI_EVENT_HEAD_T))) = UNI_HTONL(rt); // Response data length
    // Add response data
    pkt.data = resp_payload;
    pkt.len = sizeof(resp_payload);
    rt = tuya_ai_basic_pkt_send(&pkt);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to send event response, rt: %d", rt);
        return rt;
    }
    return OPRT_OK;
}

/**
 * @brief Parse protocol frame
 */
STATIC OPERATE_RET __parse_pkg(ai_monitor_client_t *client, char *data, uint32_t len)
{
    if (!client || !data || len < sizeof(AI_PAYLOAD_HEAD_T)) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = OPRT_OK;
    AI_PAYLOAD_HEAD_T *head = (AI_PAYLOAD_HEAD_T *)data;
    AI_PACKET_PT type = head->type;
    AI_ATTR_FLAG attr_flag = head->attribute_flag;
    uint32_t attr_len = 0;
    uint32_t offset = SIZEOF(AI_PAYLOAD_HEAD_T);
    char *payload, *attr_buf;

    if (type == AI_PT_PING) {
        // parse attr
        AI_ATTRIBUTE_T attr = {0};
        uint64_t client_ts = 0;
        if (head->attribute_flag != AI_HAS_ATTR) {
            PR_ERR("ai ping packet has no attribute");
            return OPRT_COM_ERROR;
        }
        memcpy(&attr_len, data + offset, SIZEOF(attr_len));
        attr_len = UNI_NTOHL(attr_len);
        offset += SIZEOF(attr_len);
        attr_buf = data + offset;
        offset += attr_len;
        // parse attributes
        while (offset < attr_len) {
            memset(&attr, 0, SIZEOF(AI_ATTRIBUTE_T));
            rt = tuya_ai_get_attr_value(attr_buf, &offset, &attr);
            if (OPRT_OK != rt) {
                PR_ERR("get attr value failed, rt:%d", rt);
                return rt;
            }

            if (attr.type == AI_ATTR_CLIENT_TS) {
                client_ts = attr.value.u64;
            } else {
                PR_ERR("unknown attr type: %d", attr.type);
            }
        }

        // handle ping
        return __handle_ping(client, client_ts, payload);
    } else if (type == AI_PT_EVENT) {
        AI_BIZ_ATTR_INFO_T attr_info;
        memset(&attr_info, 0, SIZEOF(AI_BIZ_ATTR_INFO_T));
        attr_info.flag = attr_flag;
        attr_info.type = type;
        if (attr_flag == AI_HAS_ATTR) {
            memcpy(&attr_len, data + offset, SIZEOF(attr_len));
            attr_len = UNI_NTOHL(attr_len);
            offset += SIZEOF(attr_len);
            attr_buf = data + offset;
            rt = tuya_ai_parse_event_attr(attr_buf, attr_len, &attr_info.value.event);
            if (OPRT_OK != rt) {
                PR_ERR("parse event attr failed, rt:%d", rt);
                return rt;
            }
            offset += attr_len;
        }

        offset += SIZEOF(uint32_t);
        payload = data + offset;

        // handle event
        return __handle_event(client, &attr_info.value.event, payload);
    } else {
        PR_ERR("unsupported packet type: %d", type);
        return OPRT_NOT_SUPPORTED;
    }

}

STATIC int __find_sync_frame(CONST uint8_t *data, uint32_t len)
{
    // Search for magic number
    uint32_t magic = UNI_HTONL(AI_MONITOR_MAGIC);
    for (uint32_t offset = 0; offset <= len; offset++) {
        if (memcmp(data + offset, &magic, sizeof(magic)) == 0) {
            return offset; // Found sync frame
        }
    }
    return OPRT_NOT_FOUND; // No valid frame found
}

/**
 * @brief Socket read handler
 */
STATIC VOID __socket_read_handler(int32_t sock)
{
    ai_monitor_client_t *client = __find_client_by_fd(sock);
    if (!client) {
        PR_ERR("client not found for fd=%d", sock);
        return;
    }

    // FIXME: current only support event type with event type == 0xf000, need to handle other types
    // Receive data
    int recv_len = tal_net_recv(sock, client->recv_buf + client->recv_len, client->recv_buf_size - client->recv_len);
    if (recv_len <= 0) {
        if (recv_len == 0) {
            PR_INFO("client fd=%d disconnected", sock);
        } else {
            PR_ERR("recv data failed, errno=%d", tal_net_get_errno());
        }

        tal_mutex_lock(g_ai_monitor_server.mutex);
        // Cleanup client
        __cleanup_client(client);
        g_ai_monitor_server.client_count--;
        tal_mutex_unlock(g_ai_monitor_server.mutex);
        return;
    }

    client->recv_len += recv_len;

    // Parse frames
    uint32_t processed = 0;
    while (processed < client->recv_len) {
        // find sync frame with magic number
        OPERATE_RET ret = __find_sync_frame(client->recv_buf + processed, client->recv_len - processed);
        if (ret != OPRT_OK) {
            // drop all data before the sync frame
            PR_ERR("find sync frame failed: %d", ret);
            processed = client->recv_len;
            break; // No valid frame found
        }
        processed += ret; // Move to the start of the sync frame

        if (client->recv_len - processed < sizeof(ai_monitor_header_t) + sizeof(uint32_t)) {
            break; // Need more data
        }

        ai_monitor_header_t *frame = (ai_monitor_header_t*)(client->recv_buf + processed);
        uint32_t pkg_len;
        memcpy(&pkg_len, frame + 1, sizeof(uint32_t));
        pkg_len = UNI_NTOHL(pkg_len);

        if (frame->direction != AI_MONITOR_DIR_ACK ||
            frame->pkg_header.version != 1 ||
            frame->pkg_header.iv_flag != 0 ||
            frame->pkg_header.security_level != AI_PACKET_SL0 ||
            frame->pkg_header.frag_flag != AI_PACKET_NO_FRAG) {
            PR_ERR("invalid frame: direction=%d, version=%d, iv_flag=%d, security_level=%d, frag_flag=%d, seq=%d, pkg_len=%d",
                   frame->direction, frame->pkg_header.version, frame->pkg_header.iv_flag,
                   frame->pkg_header.security_level, frame->pkg_header.frag_flag, UNI_NTOHS(frame->pkg_header.sequence), pkg_len);
            processed += sizeof(uint32_t); // skip magic for next frame
            continue; // Try next frame
        }

        processed += sizeof(ai_monitor_header_t) + sizeof(uint32_t); // Skip header + length

        // Parse header
        if (client->recv_len - processed < pkg_len) {
            PR_TRACE("incomplete frame, need %u bytes, got %u", pkg_len, client->recv_len - processed);
            processed -= sizeof(ai_monitor_header_t) + sizeof(uint32_t); // Move back to start of frame
            break; // Need more data
        }

        PR_TRACE("direction=%d, version=%d, iv_flag=%d, security_level=%d, frag_flag=%d, seq=%d, pkg_len=%d",
                frame->direction, frame->pkg_header.version, frame->pkg_header.iv_flag,
                frame->pkg_header.security_level, frame->pkg_header.frag_flag, frame->pkg_header.sequence, pkg_len);

        // Parse packet
        ret = __parse_pkg(client, (char *)client->recv_buf + processed, pkg_len);
        processed += pkg_len;
        if (ret != OPRT_OK) {
            PR_ERR("parse frame failed: %d", ret);
        }
    }

    // Move remaining data to buffer start
    if (processed > 0) {
        if (processed < client->recv_len) {
            memmove(client->recv_buf, client->recv_buf + processed, client->recv_len - processed);
        }
        client->recv_len -= processed;
    }
    return;
}

/**
 * @brief Socket error handler
 */
STATIC VOID __socket_error_handler(int sock)
{
    ai_monitor_client_t *client = __find_client_by_fd(sock);
    if (!client) {
        return;
    }

    PR_ERR("socket error for fd=%d", sock);

    // Cleanup client
    __cleanup_client(client);
    g_ai_monitor_server.client_count--;
}

STATIC int __create_server_socket(int port)
{
    int status = 0;
    int sockfd = 0;
    OPERATE_RET op_ret = OPRT_OK;
    NW_IP_S ip;
    TUYA_IP_ADDR_T ip_addr = TY_IPADDR_ANY;
    memset(&ip, 0, SIZEOF(NW_IP_S));

    CONST netmgr_linkage_t *linkage = g_ai_monitor_server.linkage;
    if (NULL == linkage) {
        linkage = tuya_svc_netmgr_linkage_get(LINKAGE_TYPE_DEFAULT);
    }

    op_ret = linkage->get(LINKAGE_CFG_IP, &ip);
    if (op_ret != OPRT_OK) {
        PR_DEBUG("Get IP Fails");
        return -9;
    }

    /* create listening TCP socket */
    sockfd = tal_net_socket_create(PROTOCOL_TCP);
    if (sockfd < 0) {
        return -1;
    }

    status = tal_net_set_reuse(sockfd);
    if (status < 0) {
        tal_net_close(sockfd);
        return -2;
    }

    if (tuya_svc_devos_get_state() < DEVOS_STATE_ACTIVATED) {
        ip_addr = tal_net_str2addr(ip.ip);
        PR_NOTICE("Not actived, use linkage addr[%s][%08x]", ip.ip, ip_addr);
    }

    status = tal_net_bind(sockfd, ip_addr, port);
    if (status < 0) {
        tal_net_close(sockfd);
        return -3;
    }

    status = tal_net_listen(sockfd, 5);
    if (status < 0) {
        tal_net_close(sockfd);
        return -5;
    }

    return sockfd;
}

STATIC int __tcp_create_serv_fd(VOID)
{
    OPERATE_RET rt = OPRT_OK;

    if (g_ai_monitor_server.server_fd < 0) {
        // Create server socket
        rt = __create_server_socket(g_ai_monitor_server.config.port);
        if (rt < 0) {
            PR_ERR("create server socket failed, ret=%d, errno=%d", rt, tal_net_get_errno());
            tal_mutex_unlock(g_ai_monitor_server.mutex);
            return OPRT_COM_ERROR;
        }
        PR_DEBUG("Server socket created, fd=%d", rt);
        g_ai_monitor_server.server_fd = rt;
    
        // Register server socket for monitoring
        sloop_sock_t sock_info = {
            .sock = g_ai_monitor_server.server_fd,
            .pre_select = NULL,
            .read = __accept_handler,
            .err = __accept_err,
            .quit = NULL
        };

        rt = tuya_reg_lan_sock(sock_info);
        if (rt != OPRT_OK) {
            PR_ERR("register server socket failed: %d", rt);
            tal_net_close(g_ai_monitor_server.server_fd);
            g_ai_monitor_server.server_fd = -1;
            tal_mutex_unlock(g_ai_monitor_server.mutex);
            return rt;
        }
    }
    return g_ai_monitor_server.server_fd;
}

/**
 * @brief Accept new client connection
 */
STATIC VOID __accept_handler(int32_t server_sock)
{
    OPERATE_RET ret = OPRT_OK;
    TUYA_IP_ADDR_T addr = 0;

    int client_fd = tal_net_accept(server_sock, &addr, NULL);
    if (client_fd < 0) {
        PR_ERR("accept failed %d (errno: %d)", client_fd, tal_net_get_errno());
        return;
    }

    // Check client limit
    if (g_ai_monitor_server.client_count >= g_ai_monitor_server.config.max_clients) {
        PR_WARN("max clients reached, reject connection");
        tal_net_close(client_fd);
        return;
    }

    // Find free client slot
    ai_monitor_client_t *client = NULL;
    for (uint32_t i = 0; i < g_ai_monitor_server.config.max_clients; i++) {
        if (!g_ai_monitor_server.clients[i].connected) {
            client = &g_ai_monitor_server.clients[i];
            break;
        }
    }

    if (!client) {
        PR_ERR("no free client slot");
        tal_net_close(client_fd);
        return;
    }

    // Initialize client
    ret = __init_client(client, client_fd, addr);
    if (ret != OPRT_OK) {
        PR_ERR("init client failed: %d", ret);
        tal_net_close(client_fd);
        return;
    }

    // set reuse
    tal_net_set_reuse(client_fd);

    // set no block
    tal_net_set_block(client_fd, FALSE);

    // Register socket for monitoring
    sloop_sock_t sock_info = {
        .sock = client_fd,
        .pre_select = NULL,
        .read = __socket_read_handler,
        .err = __socket_error_handler,
        .quit = NULL
    };

    ret = tuya_reg_lan_sock(sock_info);
    if (ret != OPRT_OK) {
        PR_ERR("register socket failed: %d", ret);
        __cleanup_client(client);
        return;
    }

    g_ai_monitor_server.client_count++;

    PR_INFO("client connected, fd=%d, addr=%s, count=%d",
            client_fd, tal_net_addr2str(addr), g_ai_monitor_server.client_count);
}

STATIC VOID __accept_err(int fd)
{
    PR_DEBUG("accept error on fd=%d", fd);
    __session_close_all();
    return;
}

STATIC OPERATE_RET __ai_biz_handler(uint8_t direction, uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char *data, VOID *usr_data)
{
    if (!attr || !head) {
        return OPRT_INVALID_PARM;
    }

    // FIXME: 暂不支持分片报文,需要在底层协议处理部分将分片信息分实例进行管理，否则会导致分片标志乱序
    if (head->total_len > 0 && head->total_len != head->len) {
        PR_ERR("Unsupported fragmented message, total_len=%d, len=%d", head->total_len, head->len);
        return OPRT_NOT_SUPPORTED;
    }

    OPERATE_RET ret = OPRT_OK;
    ai_monitor_server_t *server = (ai_monitor_server_t *)usr_data;

    // send to specific client which registered this type
    for (uint32_t i = 0; i < server->config.max_clients; i++) {
        ai_monitor_client_t *client = &server->clients[i];
        if (!client->connected || client->fd < 0) {
            continue; // Skip disconnected clients
        }

        BOOL_T registered = __is_client_registered(client, attr->type);
        if (!registered) {
            PR_TRACE("client %d not registered for type %d", i, attr->type);
            continue; // Skip unregistered or disconnected clients
        }
        PR_TRACE("Sending to client %d, id=%d, type=%d, head len=%d, data len=%d",
                 client->fd, id, attr->type, head->len, head->total_len);
        ret = __pack_and_send(client, direction, id, attr, head, data);
        if (ret != OPRT_OK) {
            PR_ERR("send to client %d failed: %d", client->fd, ret);
            continue; // Try next client
        }
    }

    return ret;
}

STATIC OPERATE_RET __ai_biz_recv_handler(uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char *data, VOID *usr_data)
{
    // use last attr type for stream data if attr is NULL
    STATIC AI_BIZ_ATTR_INFO_T s_attr_info = {0};
    if (attr)
        s_attr_info.type = attr->type;
    else if (head && head->stream_flag == AI_STREAM_ING)
        attr = &s_attr_info;

    return __ai_biz_handler(AI_MONITOR_DIR_DS, id, attr, head, data, usr_data);
}

STATIC OPERATE_RET __ai_biz_send_handler(uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char *data, VOID *usr_data)
{
    return __ai_biz_handler(AI_MONITOR_DIR_US, id, attr, head, data, usr_data);
}

STATIC VOID __monitor_tm_cb(TIMER_ID timerID, void* pTimerArg)
{
    NW_IP_S ip;
    memset(&ip, 0, SIZEOF(NW_IP_S));

    if (!get_gw_cntl()->is_init) {
        return;
    }

    if (g_ai_monitor_server.server_fd >= 0) {
        return;
    }

    GW_WORK_STAT_T wk_stat = get_gw_cntl()->gw_wsm.stat;
    if (ACTIVATED != wk_stat) {
        PR_TRACE("Gateway not activated, skip creating server socket");
        return;
    }

    // create server socket if not created
    if (__tcp_create_serv_fd() < 0) {
        PR_ERR("create server socket failed");
        return;
    }

    // stop timer if server socket created
    tal_sw_timer_stop(g_ai_monitor_server.timer);
    PR_DEBUG("Server socket created successfully, stopped timer");
}

/**
 * @brief start AI monitor TCP server
 */
STATIC OPERATE_RET __ai_monitor_start(VOID)
{
    OPERATE_RET rt = OPRT_OK;

    if (!g_ai_monitor_server.initialized) {
        return OPRT_INVALID_PARM;
    }

    if (g_ai_monitor_server.running) {
        PR_WARN("AI monitor already running");
        return OPRT_OK;
    }

    tal_mutex_lock(g_ai_monitor_server.mutex);
    tal_sw_timer_start(g_ai_monitor_server.timer, 2000, TAL_TIMER_CYCLE);

    rt = tuya_ai_biz_monitor_register(__ai_biz_recv_handler, __ai_biz_send_handler, &g_ai_monitor_server);
    if (rt != OPRT_OK) {
        PR_ERR("set AI biz monitor callback failed: %d", rt);
        tuya_unreg_lan_sock(g_ai_monitor_server.server_fd);
        // tal_net_close(g_ai_monitor_server.server_fd);
        g_ai_monitor_server.server_fd = -1;
        tal_mutex_unlock(g_ai_monitor_server.mutex);
        return rt;
    }
    g_ai_monitor_server.running = TRUE;
    tal_mutex_unlock(g_ai_monitor_server.mutex);

    PR_INFO("AI monitor started, listening on port %d", g_ai_monitor_server.config.port);
    return OPRT_OK;
}

/**
 * @brief initialize AI monitor TCP server
 */
OPERATE_RET tuya_ai_monitor_init(CONST ai_monitor_config_t *config)
{
    if (!config) {
        return OPRT_INVALID_PARM;
    }

    if (g_ai_monitor_server.initialized) {
        PR_WARN("AI monitor already initialized");
        return OPRT_OK;
    }

    // init default writer
    s_monitor_writer_cfg.writer = &s_default_writer;

    OPERATE_RET rt = tuya_sock_loop_init();
    if (rt != OPRT_OK) {
        PR_ERR("sock loop init failed: %d", rt);
        return rt;
    }

    memset(&g_ai_monitor_server, 0, sizeof(g_ai_monitor_server));

    // Copy configuration
    memcpy(&g_ai_monitor_server.config, config, sizeof(ai_monitor_config_t));

    // Allocate client array
    g_ai_monitor_server.clients = OS_MALLOC(sizeof(ai_monitor_client_t) * config->max_clients);
    if (!g_ai_monitor_server.clients) {
        PR_ERR("malloc clients failed");
        return OPRT_MALLOC_FAILED;
    }

    // Initialize clients
    for (uint32_t i = 0; i < config->max_clients; i++) {
        memset(&g_ai_monitor_server.clients[i], 0, sizeof(ai_monitor_client_t));
        g_ai_monitor_server.clients[i].fd = -1;
        g_ai_monitor_server.clients[i].connected = FALSE;
    }

    // Create mutex
    rt = tal_mutex_create_init(&g_ai_monitor_server.mutex);
    if (rt != OPRT_OK) {
        PR_ERR("create mutex failed: %d", rt);
        OS_FREE(g_ai_monitor_server.clients);
        return rt;
    }

    // Create timer
    rt = tal_sw_timer_create(__monitor_tm_cb, NULL, &g_ai_monitor_server.timer);
    if (rt != OPRT_OK) {
        PR_ERR("create timer failed: %d", rt);
        OS_FREE(g_ai_monitor_server.clients);
        tal_mutex_release(g_ai_monitor_server.mutex);
        return rt;
    }

    g_ai_monitor_server.initialized = TRUE;
    g_ai_monitor_server.running = FALSE;
    g_ai_monitor_server.server_fd = -1;
    g_ai_monitor_server.client_count = 0;
    g_ai_monitor_server.sequence = uni_random();
    g_ai_monitor_server.session_id = uni_random();

    PR_INFO("AI monitor initialized, port=%d, max_clients=%d, inital sid=%u",
            config->port, config->max_clients, g_ai_monitor_server.session_id);

    return __ai_monitor_start();
}

STATIC VOID __session_close_all(VOID)
{
    tal_mutex_lock(g_ai_monitor_server.mutex);

    // Unregister and close server socket
    if (g_ai_monitor_server.server_fd >= 0) {
        tuya_unreg_lan_sock(g_ai_monitor_server.server_fd);
        // tal_net_close(g_ai_monitor_server.server_fd);
        g_ai_monitor_server.server_fd = -1;
    }

    // Disconnect all clients
    for (uint32_t i = 0; i < g_ai_monitor_server.config.max_clients; i++) {
        if (g_ai_monitor_server.clients[i].connected) {
            tuya_unreg_lan_sock(g_ai_monitor_server.clients[i].fd);
            __cleanup_client(&g_ai_monitor_server.clients[i]);
        }
    }
    g_ai_monitor_server.client_count = 0;

    // start timer to create server socket
    tal_sw_timer_start(g_ai_monitor_server.timer, 2000, TAL_TIMER_CYCLE);

    tal_mutex_unlock(g_ai_monitor_server.mutex);
}

/**
 * @brief stop AI monitor TCP server
 */
STATIC OPERATE_RET __ai_monitor_stop(VOID)
{
    if (!g_ai_monitor_server.initialized || !g_ai_monitor_server.running) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(g_ai_monitor_server.mutex);
    __session_close_all();
    g_ai_monitor_server.running = FALSE;
    tal_mutex_unlock(g_ai_monitor_server.mutex);

    PR_INFO("AI monitor stopped");

    return OPRT_OK;
}

/**
 * @brief deinitialize AI monitor TCP server
 */
OPERATE_RET tuya_ai_monitor_deinit(VOID)
{
    if (!g_ai_monitor_server.initialized) {
        return OPRT_INVALID_PARM;
    }

    // Stop server first
    if (g_ai_monitor_server.running) {
        __ai_monitor_stop();
    }

    // Free resources
    if (g_ai_monitor_server.clients) {
        OS_FREE(g_ai_monitor_server.clients);
        g_ai_monitor_server.clients = NULL;
    }

    if (g_ai_monitor_server.mutex) {
        tal_mutex_release(g_ai_monitor_server.mutex);
        g_ai_monitor_server.mutex = NULL;
    }

    memset(&g_ai_monitor_server, 0, sizeof(g_ai_monitor_server));

    PR_INFO("AI monitor deinitialized");

    return OPRT_OK;
}

/**
 * @brief check if server is running
 */
BOOL_T tuya_ai_monitor_is_running(VOID)
{
    return g_ai_monitor_server.initialized && g_ai_monitor_server.running;
}

/**
 * @brief broadcast message to all connected clients
 */
OPERATE_RET tuya_ai_monitor_broadcast(uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char *data)
{
    if (!g_ai_monitor_server.initialized || !g_ai_monitor_server.running) {
        return OPRT_INVALID_PARM;
    }

    return __ai_biz_handler(AI_MONITOR_DIR_ACK, id, attr, head, data, &g_ai_monitor_server);
}

#define TY_AI_MONITOR_US_AUDIO 1
#define TY_AI_MONITOR_US_VIDEO 3
#define TY_AI_MONITOR_US_TEXT 5
#define TY_AI_MONITOR_US_IMAGE 7
#define TY_AI_MONITOR_DS_AUDIO 2
#define TY_AI_MONITOR_DS_TEXT 4
#define TY_AI_MONITOR_US_LOG 0x8001
#define TY_AI_MONITOR_US_MIC 0x8003
#define TY_AI_MONITOR_US_REF 0x8005
#define TY_AI_MONITOR_US_AEC 0x8007

/**
 * @brief broadcast text data to all connected clients
 */
STATIC OPERATE_RET __broadcast_text(uint16_t data_id, char *data, uint32_t len)
{
    if (!g_ai_monitor_server.initialized || !g_ai_monitor_server.running) {
        return OPRT_INVALID_PARM;
    }

    if (!data || len == 0) {
        return OPRT_INVALID_PARM;
    }

    AI_BIZ_ATTR_INFO_T attr = {
        .flag = AI_HAS_ATTR,
        .type = AI_PT_TEXT,
        .value.text = {
            .session_id_list = NULL,
        },
    };

    AI_BIZ_HEAD_INFO_T head = {
        .stream_flag = AI_STREAM_START | AI_STREAM_END,
        .total_len = len,
        .len = len,
    };

    return __ai_biz_handler(AI_MONITOR_DIR_ACK, data_id, &attr, &head, data, &g_ai_monitor_server);
}

/**
 * @brief broadcast text data to all connected clients
 */
OPERATE_RET tuya_ai_monitor_broadcast_text(char *data, uint32_t len)
{
    return __broadcast_text(TY_AI_MONITOR_US_TEXT, data, len);
}

/**
 * @brief broadcast log data to all connected clients
 */
OPERATE_RET tuya_ai_monitor_broadcast_log(char *data, uint32_t len)
{
    return __broadcast_text(TY_AI_MONITOR_US_LOG, data, len);
}

/**
 * @brief broadcast audio data to all connected clients
 */
OPERATE_RET tuya_ai_monitor_broadcast_audio(uint16_t data_id, AI_STREAM_TYPE stype, AI_AUDIO_CODEC_TYPE codec_type, char *data, uint32_t len)
{
    if (!g_ai_monitor_server.initialized || !g_ai_monitor_server.running) {
        return OPRT_INVALID_PARM;
    }

    if (!data || len == 0) {
        return OPRT_INVALID_PARM;
    }

    AI_BIZ_ATTR_INFO_T attr = {
        .flag = AI_HAS_ATTR,
        .type = AI_PT_AUDIO,
        .value.audio = {
            .base.codec_type = codec_type,
            .base.sample_rate = 16000,
            .base.channels = AUDIO_CHANNELS_MONO,
            .base.bit_depth = 16,
        },
    };

    AI_BIZ_HEAD_INFO_T head = {
        .stream_flag = stype,
        .total_len = len,
        .len = len,
    };

    return __ai_biz_handler(AI_MONITOR_DIR_ACK, data_id, &attr, &head, data, &g_ai_monitor_server);
}

/**
 * @brief broadcast mic audio data to all connected clients
 */
OPERATE_RET tuya_ai_monitor_broadcast_audio_mic(AI_STREAM_TYPE stype, char *data, uint32_t len)
{
    return tuya_ai_monitor_broadcast_audio(TY_AI_MONITOR_US_MIC, stype, AUDIO_CODEC_PCM, data, len);
}

/**
 * @brief broadcast ref audio data to all connected clients
 */
OPERATE_RET tuya_ai_monitor_broadcast_audio_ref(AI_STREAM_TYPE stype, char *data, uint32_t len)
{
    return tuya_ai_monitor_broadcast_audio(TY_AI_MONITOR_US_REF, stype, AUDIO_CODEC_PCM, data, len);
}

/**
 * @brief broadcast aec audio data to all connected clients
 */
OPERATE_RET tuya_ai_monitor_broadcast_audio_aec(AI_STREAM_TYPE stype, char *data, uint32_t len)
{
    return tuya_ai_monitor_broadcast_audio(TY_AI_MONITOR_US_AEC, stype, AUDIO_CODEC_PCM, data, len);
}

/**
 * @brief dump server status information
 */
VOID tuya_ai_monitor_dump_status(VOID)
{
    if (!g_ai_monitor_server.initialized) {
        PR_INFO("AI monitor not initialized");
        return;
    }

    PR_INFO("=== AI Monitor Status ===");
    PR_INFO("Initialized: %s", g_ai_monitor_server.initialized ? "Yes" : "No");
    PR_INFO("Running: %s", g_ai_monitor_server.running ? "Yes" : "No");
    PR_INFO("Port: %d", g_ai_monitor_server.config.port);
    PR_INFO("Max clients: %d", g_ai_monitor_server.config.max_clients);
    PR_INFO("Current clients: %d", g_ai_monitor_server.client_count);
    PR_INFO("Server FD: %d", g_ai_monitor_server.server_fd);

    for (uint32_t i = 0; i < g_ai_monitor_server.config.max_clients; i++) {
        if (g_ai_monitor_server.clients[i].connected) {
            PR_INFO("Client[%d]: fd=%d, addr=%s, last_ping=%llu",
                    i, g_ai_monitor_server.clients[i].fd,
                    g_ai_monitor_server.clients[i].addr, g_ai_monitor_server.clients[i].last_ping_time);
        }
    }
    PR_INFO("========================");
}

STATIC OPERATE_RET __default_update(AI_STAGE_E stage, VOID *data, AI_SEND_PACKET_T *info)
{
    ai_monitor_writer_cfg_t *cfg = (ai_monitor_writer_cfg_t *)info->writer->user_data;

    if (stage == AI_STAGE_GET_FRAG_OFFSET) {
        *(uint32_t **)data = &cfg->frag_offset[cfg->direction % AI_MONITOR_DIR_MAX];
        return OPRT_OK;
    } else if (stage == AI_STAGE_GET_SEQUENCE) {
        *(uint16_t *)data = cfg->sequence_out++;
        if (cfg->sequence_out == 0) {
            cfg->sequence_out = 1;
        }
        return OPRT_OK;
    } else if (stage == AI_STAGE_PRE_WRITE) {
        // write magic number and direction at the beginning of the packet
        ai_monitor_header_t header = {0};
        header.magic = UNI_HTONL(AI_MONITOR_MAGIC);
        header.direction = cfg->direction;
        return cfg->writer->write(info->writer, &header, 5);
    } else {
        return OPRT_OK;
    }
}

STATIC OPERATE_RET __default_write(AI_PACKET_WRITER_T *writer, VOID *buf, uint32_t buf_len)
{
    ai_monitor_writer_cfg_t *cfg = (ai_monitor_writer_cfg_t *)writer->user_data;
    if (cfg->fd < 0 || !buf || buf_len == 0) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = OPRT_OK;
    uint32_t total_sent = 0;
    uint32_t remaining = buf_len;

    while (remaining > 0) {
        rt = tal_net_send(cfg->fd, (char *)buf + total_sent, remaining);
        if (rt <= 0) {
            TUYA_ERRNO err = tal_net_get_errno();
            // FIXME: some platforms may return different error codes
            if (err == UNW_EAGAIN || err == UNW_EWOULDBLOCK || err == 11) {
                // Non-blocking mode, retry
                tal_system_sleep(50); // Sleep for a short time before retrying
                continue;
            }
            PR_ERR("send data failed, rt=%d, errno=%d", rt, err);
            return OPRT_COM_ERROR;
        }
        total_sent += rt;
        remaining -= rt;
    }

    return OPRT_OK;
}

#if 0
STATIC VOID __log_recv_task(void* args)
{
    while (g_ai_monitor_server.log_thread_running) {

    }
}

// TODO:
// start async log server to recv log output from log module, then broadcast to all clients
STATIC OPERATE_RET __start_log_server(VOID)
{
    OPERATE_RET rt = OPRT_OK;

    // Register log output term
    tal_log_add_output_term(AI_MONITOR_TAG, __log_output);

    // create async log recv task
    THREAD_CFG_T thrd_param = {0};
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "ai_mon_log";
    thrd_param.stackDepth = 4096;
#if defined(AI_STACK_IN_PSRAM) && (AI_STACK_IN_PSRAM == 1)
    thrd_param.psram_mode = 1;
#endif

    rt = tal_thread_create_and_start(&g_ai_monitor_server.log_thread,
                                     NULL, NULL,
                                     __log_recv_task, NULL,
                                     &thrd_param);

    return rt;
}

STATIC OPERATE_RET __stop_log_server(VOID)
{

}
#endif

STATIC VOID __log_output(CONST char *str)
{
    if (!str || strlen(str) == 0) {
        return; // Ignore empty log messages
    }

    // TODO: async log output
    // Broadcast log message to all connected clients
    // tuya_ai_monitor_broadcast_log((char *)str, strlen(str));
}
