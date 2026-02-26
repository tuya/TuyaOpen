#include "ws_server.h"

#include "bus/message_bus.h"
#include "cJSON.h"
#include "mimi_config.h"
#include "mix_method.h"
#include "tal_hash.h"
#include "tal_mutex.h"
#include "tal_network.h"
#include "tal_thread.h"

static const char *TAG = "ws";

typedef struct {
    int     fd;
    BOOL_T  active;
    BOOL_T  handshake_done;
    char    chat_id[96];
    uint8_t rx_buf[4096];
    size_t  rx_len;
} ws_client_t;

static THREAD_HANDLE   s_ws_thread  = NULL;
static MUTEX_HANDLE    s_ws_mutex   = NULL;
static volatile BOOL_T s_ws_running = FALSE;
static int             s_listen_fd  = -1;
static ws_client_t     s_clients[MIMI_WS_MAX_CLIENTS];

static void ws_reset_client(ws_client_t *client)
{
    if (!client) {
        return;
    }

    memset(client, 0, sizeof(ws_client_t));
    client->fd = -1;
}

static void ws_clients_init(void)
{
    for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
        ws_reset_client(&s_clients[i]);
    }
}

static void ws_close_fd(int *fd)
{
    if (!fd || *fd < 0) {
        return;
    }
    tal_net_close(*fd);
    *fd = -1;
}

static void ws_close_client_locked(ws_client_t *client)
{
    if (!client || !client->active) {
        return;
    }
    MIMI_LOGI(TAG, "client disconnected chat_id=%s fd=%d", client->chat_id[0] ? client->chat_id : "unknown",
              client->fd);
    ws_close_fd(&client->fd);
    ws_reset_client(client);
}

static ws_client_t *ws_find_client_by_chat_id_locked(const char *chat_id)
{
    if (!chat_id || !chat_id[0]) {
        return NULL;
    }

    for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
        if (!s_clients[i].active) {
            continue;
        }
        if (strncmp(s_clients[i].chat_id, chat_id, sizeof(s_clients[i].chat_id)) == 0) {
            return &s_clients[i];
        }
    }
    return NULL;
}

static int ws_find_header_end(const uint8_t *buf, size_t len)
{
    if (!buf || len < 4) {
        return -1;
    }

    for (size_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            return (int)(i + 4);
        }
    }
    return -1;
}

static bool ws_get_header_value(const char *headers, const char *name, char *value, size_t value_size)
{
    if (!headers || !name || !value || value_size == 0) {
        return false;
    }

    size_t      name_len = strlen(name);
    const char *line     = headers;
    while (*line) {
        const char *line_end = strstr(line, "\r\n");
        size_t      line_len = line_end ? (size_t)(line_end - line) : strlen(line);
        if (line_len == 0) {
            break;
        }

        if (line_len > name_len && tuya_strncasecmp(line, name, name_len) == 0 && line[name_len] == ':') {
            const char *val = line + name_len + 1;
            while (*val == ' ' || *val == '\t') {
                val++;
            }

            size_t copy_len = line_len - (size_t)(val - line);
            if (copy_len > value_size - 1) {
                copy_len = value_size - 1;
            }
            memcpy(value, val, copy_len);
            value[copy_len] = '\0';
            return true;
        }

        if (!line_end) {
            break;
        }
        line = line_end + 2;
    }

    return false;
}

static OPERATE_RET ws_send_all(int fd, const uint8_t *buf, size_t len)
{
    if (fd < 0 || (!buf && len > 0)) {
        return OPRT_INVALID_PARM;
    }

    size_t sent = 0;
    while (sent < len) {
        int n = tal_net_send(fd, buf + sent, (uint32_t)(len - sent));
        if (n == OPRT_RESOURCE_NOT_READY) {
            tal_system_sleep(5);
            continue;
        }
        if (n <= 0) {
            return OPRT_SEND_ERR;
        }
        sent += (size_t)n;
    }
    return OPRT_OK;
}

static OPERATE_RET ws_send_frame(int fd, uint8_t opcode, const uint8_t *payload, size_t payload_len)
{
    uint8_t  header[14] = {0};
    size_t   header_len = 0;
    uint64_t plen64     = (uint64_t)payload_len;

    header[0] = (uint8_t)(0x80 | (opcode & 0x0F));
    if (payload_len <= 125) {
        header[1]  = (uint8_t)payload_len;
        header_len = 2;
    } else if (payload_len <= 0xFFFF) {
        header[1]  = 126;
        header[2]  = (uint8_t)((payload_len >> 8) & 0xFF);
        header[3]  = (uint8_t)(payload_len & 0xFF);
        header_len = 4;
    } else {
        header[1] = 127;
        for (int i = 0; i < 8; i++) {
            header[2 + i] = (uint8_t)((plen64 >> (56 - i * 8)) & 0xFF);
        }
        header_len = 10;
    }

    OPERATE_RET rt = ws_send_all(fd, header, header_len);
    if (rt != OPRT_OK) {
        return rt;
    }

    if (payload_len > 0) {
        rt = ws_send_all(fd, payload, payload_len);
    }
    return rt;
}

static OPERATE_RET ws_build_accept_key(const char *client_key, char *accept_key, size_t accept_size)
{
    static const char ws_guid[]    = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char              key_cat[128] = {0};
    uint8_t           sha1[20]     = {0};
    char              b64[64]      = {0};

    if (!client_key || !accept_key || accept_size == 0) {
        return OPRT_INVALID_PARM;
    }

    int n = snprintf(key_cat, sizeof(key_cat), "%s%s", client_key, ws_guid);
    if (n <= 0 || (size_t)n >= sizeof(key_cat)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    OPERATE_RET rt = tal_sha1_ret((const uint8_t *)key_cat, strlen(key_cat), sha1);
    if (rt != OPRT_OK) {
        return rt;
    }

    if (!tuya_base64_encode(sha1, b64, sizeof(sha1))) {
        return OPRT_COM_ERROR;
    }

    snprintf(accept_key, accept_size, "%s", b64);
    return OPRT_OK;
}

static OPERATE_RET ws_do_handshake_locked(ws_client_t *client)
{
    if (!client || !client->active) {
        return OPRT_INVALID_PARM;
    }

    int hdr_end = ws_find_header_end(client->rx_buf, client->rx_len);
    if (hdr_end < 0) {
        return OPRT_RESOURCE_NOT_READY;
    }

    client->rx_buf[hdr_end - 1] = '\0';
    const char *hdr             = (const char *)client->rx_buf;

    char ws_key[128] = {0};
    if (!ws_get_header_value(hdr, "Sec-WebSocket-Key", ws_key, sizeof(ws_key))) {
        MIMI_LOGW(TAG, "handshake missing websocket header fd=%d", client->fd);
        return OPRT_CJSON_GET_ERR;
    }

    char        accept_key[64] = {0};
    OPERATE_RET rt             = ws_build_accept_key(ws_key, accept_key, sizeof(accept_key));
    if (rt != OPRT_OK) {
        return rt;
    }

    char resp[256] = {0};
    int  n         = snprintf(resp, sizeof(resp),
                              "HTTP/1.1 101 Switching Protocols\r\n"
                                       "Upgrade: websocket\r\n"
                                       "Connection: Upgrade\r\n"
                                       "Sec-WebSocket-Accept: %s\r\n\r\n",
                              accept_key);
    if (n <= 0 || (size_t)n >= sizeof(resp)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    rt = ws_send_all(client->fd, (const uint8_t *)resp, (size_t)n);
    if (rt != OPRT_OK) {
        return rt;
    }

    client->handshake_done = TRUE;
    if (client->chat_id[0] == '\0') {
        snprintf(client->chat_id, sizeof(client->chat_id), "ws_%d", client->fd);
    }

    size_t remain = client->rx_len - (size_t)hdr_end;
    if (remain > 0) {
        memmove(client->rx_buf, client->rx_buf + hdr_end, remain);
    }
    client->rx_len = remain;

    MIMI_LOGI(TAG, "handshake success chat_id=%s fd=%d", client->chat_id, client->fd);
    return OPRT_OK;
}

static OPERATE_RET ws_decode_one_frame(ws_client_t *client, uint8_t *opcode, uint8_t **payload, size_t *payload_len,
                                       size_t *consumed)
{
    if (!client || !opcode || !payload || !payload_len || !consumed) {
        return OPRT_INVALID_PARM;
    }

    if (client->rx_len < 2) {
        return OPRT_RESOURCE_NOT_READY;
    }

    const uint8_t *buf    = client->rx_buf;
    uint8_t        op     = (uint8_t)(buf[0] & 0x0F);
    bool           masked = (buf[1] & 0x80) != 0;
    uint64_t       plen   = (uint64_t)(buf[1] & 0x7F);
    size_t         off    = 2;

    if (plen == 126) {
        if (client->rx_len < off + 2) {
            return OPRT_RESOURCE_NOT_READY;
        }
        plen = (uint64_t)((buf[off] << 8) | buf[off + 1]);
        off += 2;
    } else if (plen == 127) {
        if (client->rx_len < off + 8) {
            return OPRT_RESOURCE_NOT_READY;
        }
        plen = 0;
        for (int i = 0; i < 8; i++) {
            plen = (plen << 8) | buf[off + i];
        }
        off += 8;
    }

    if (plen > (uint64_t)(sizeof(client->rx_buf) - 16)) {
        return OPRT_MSG_OUT_OF_LIMIT;
    }

    if (masked) {
        if (client->rx_len < off + 4) {
            return OPRT_RESOURCE_NOT_READY;
        }
    }

    size_t frame_len = off + (masked ? 4 : 0) + (size_t)plen;
    if (client->rx_len < frame_len) {
        return OPRT_RESOURCE_NOT_READY;
    }

    uint8_t mask[4] = {0};
    if (masked) {
        memcpy(mask, buf + off, sizeof(mask));
        off += sizeof(mask);
    }

    uint8_t *data = malloc((size_t)plen + 1);
    if (!data) {
        return OPRT_MALLOC_FAILED;
    }

    if (plen > 0) {
        memcpy(data, buf + off, (size_t)plen);
        if (masked) {
            for (size_t i = 0; i < (size_t)plen; i++) {
                data[i] = (uint8_t)(data[i] ^ mask[i % 4]);
            }
        }
    }
    data[plen] = '\0';

    *opcode      = op;
    *payload     = data;
    *payload_len = (size_t)plen;
    *consumed    = frame_len;
    return OPRT_OK;
}

static void ws_consume_rx(ws_client_t *client, size_t consumed)
{
    if (!client || consumed == 0 || consumed > client->rx_len) {
        return;
    }
    if (consumed < client->rx_len) {
        memmove(client->rx_buf, client->rx_buf + consumed, client->rx_len - consumed);
    }
    client->rx_len -= consumed;
}

static void ws_handle_text_message_locked(ws_client_t *client, const uint8_t *payload, size_t payload_len)
{
    if (!client || !payload || payload_len == 0) {
        return;
    }

    cJSON *root = cJSON_Parse((const char *)payload);
    if (!root) {
        MIMI_LOGW(TAG, "invalid ws json chat_id=%s", client->chat_id);
        return;
    }

    cJSON *type    = cJSON_GetObjectItem(root, "type");
    cJSON *content = cJSON_GetObjectItem(root, "content");
    cJSON *chat_id = cJSON_GetObjectItem(root, "chat_id");

    if (!cJSON_IsString(type) || !type->valuestring || strcmp(type->valuestring, "message") != 0 ||
        !cJSON_IsString(content) || !content->valuestring) {
        cJSON_Delete(root);
        return;
    }

    if (cJSON_IsString(chat_id) && chat_id->valuestring && chat_id->valuestring[0]) {
        snprintf(client->chat_id, sizeof(client->chat_id), "%s", chat_id->valuestring);
    }

    mimi_msg_t msg = {0};
    strncpy(msg.channel, MIMI_CHAN_WEBSOCKET, sizeof(msg.channel) - 1);
    strncpy(msg.chat_id, client->chat_id, sizeof(msg.chat_id) - 1);
    msg.content = strdup(content->valuestring);
    if (!msg.content) {
        cJSON_Delete(root);
        return;
    }

    OPERATE_RET rt = message_bus_push_inbound(&msg);
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "push ws inbound failed rt=%d", rt);
        free(msg.content);
    }

    cJSON_Delete(root);
}

static void ws_process_client_buffer_locked(ws_client_t *client)
{
    if (!client || !client->active) {
        return;
    }

    if (!client->handshake_done) {
        OPERATE_RET hs = ws_do_handshake_locked(client);
        if (hs == OPRT_RESOURCE_NOT_READY) {
            return;
        }
        if (hs != OPRT_OK) {
            ws_close_client_locked(client);
            return;
        }
    }

    while (client->active && client->rx_len > 0) {
        uint8_t  opcode      = 0;
        uint8_t *payload     = NULL;
        size_t   payload_len = 0;
        size_t   consumed    = 0;

        OPERATE_RET rt = ws_decode_one_frame(client, &opcode, &payload, &payload_len, &consumed);
        if (rt == OPRT_RESOURCE_NOT_READY) {
            break;
        }
        if (rt != OPRT_OK || consumed == 0) {
            free(payload);
            ws_close_client_locked(client);
            break;
        }

        ws_consume_rx(client, consumed);

        if (opcode == 0x1) {
            ws_handle_text_message_locked(client, payload, payload_len);
        } else if (opcode == 0x8) {
            (void)ws_send_frame(client->fd, 0x8, payload, payload_len);
            free(payload);
            ws_close_client_locked(client);
            break;
        } else if (opcode == 0x9) {
            (void)ws_send_frame(client->fd, 0xA, payload, payload_len);
        }

        free(payload);
    }
}

static void ws_accept_client_locked(void)
{
    TUYA_IP_ADDR_T addr = 0;
    uint16_t       port = 0;
    int            fd   = tal_net_accept(s_listen_fd, &addr, &port);
    if (fd < 0) {
        return;
    }

    (void)tal_net_set_reuse(fd);
    (void)tal_net_set_block(fd, FALSE);

    ws_client_t *slot = NULL;
    for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
        if (!s_clients[i].active) {
            slot = &s_clients[i];
            break;
        }
    }

    if (!slot) {
        MIMI_LOGW(TAG, "max clients reached, reject fd=%d", fd);
        tal_net_close(fd);
        return;
    }

    ws_reset_client(slot);
    slot->fd     = fd;
    slot->active = TRUE;
    snprintf(slot->chat_id, sizeof(slot->chat_id), "ws_%d", fd);
    MIMI_LOGI(TAG, "client accepted fd=%d ip=%s:%u", fd, tal_net_addr2str(addr), (unsigned)port);
}

static void ws_server_task(void *arg)
{
    (void)arg;
    MIMI_LOGI(TAG, "ws server task started");

    while (s_ws_running) {
        TUYA_FD_SET_T readfds;
        TAL_FD_ZERO(&readfds);
        int maxfd = -1;

        tal_mutex_lock(s_ws_mutex);
        if (s_listen_fd >= 0) {
            TAL_FD_SET(s_listen_fd, &readfds);
            maxfd = s_listen_fd;
        }
        for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
            if (!s_clients[i].active || s_clients[i].fd < 0) {
                continue;
            }
            TAL_FD_SET(s_clients[i].fd, &readfds);
            if (s_clients[i].fd > maxfd) {
                maxfd = s_clients[i].fd;
            }
        }
        tal_mutex_unlock(s_ws_mutex);

        if (maxfd < 0) {
            tal_system_sleep(50);
            continue;
        }

        int ready = tal_net_select(maxfd + 1, &readfds, NULL, NULL, 200);
        if (ready <= 0) {
            continue;
        }

        tal_mutex_lock(s_ws_mutex);
        if (s_listen_fd >= 0 && TAL_FD_ISSET(s_listen_fd, &readfds)) {
            ws_accept_client_locked();
        }

        for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
            ws_client_t *client = &s_clients[i];
            if (!client->active || client->fd < 0) {
                continue;
            }
            if (!TAL_FD_ISSET(client->fd, &readfds)) {
                continue;
            }

            if (client->rx_len >= sizeof(client->rx_buf)) {
                ws_close_client_locked(client);
                continue;
            }

            int n = tal_net_recv(client->fd, client->rx_buf + client->rx_len,
                                 (uint32_t)(sizeof(client->rx_buf) - client->rx_len));
            if (n == OPRT_RESOURCE_NOT_READY) {
                continue;
            }
            if (n <= 0) {
                ws_close_client_locked(client);
                continue;
            }

            client->rx_len += (size_t)n;
            ws_process_client_buffer_locked(client);
        }
        tal_mutex_unlock(s_ws_mutex);
    }

    MIMI_LOGI(TAG, "ws server task stopped");
}

static OPERATE_RET ws_send_text_to_client_locked(ws_client_t *client, const char *chat_id, const char *text)
{
    if (!client || !chat_id || !text) {
        return OPRT_INVALID_PARM;
    }

    cJSON *resp = cJSON_CreateObject();
    if (!resp) {
        return OPRT_MALLOC_FAILED;
    }
    cJSON_AddStringToObject(resp, "type", "response");
    cJSON_AddStringToObject(resp, "content", text);
    cJSON_AddStringToObject(resp, "chat_id", chat_id);

    char *payload = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (!payload) {
        return OPRT_MALLOC_FAILED;
    }

    OPERATE_RET rt = ws_send_frame(client->fd, 0x1, (const uint8_t *)payload, strlen(payload));
    cJSON_free(payload);
    return rt;
}

OPERATE_RET ws_server_start(void)
{
    if (s_ws_thread) {
        return OPRT_OK;
    }

    if (!s_ws_mutex) {
        OPERATE_RET rt = tal_mutex_create_init(&s_ws_mutex);
        if (rt != OPRT_OK) {
            MIMI_LOGE(TAG, "create ws mutex failed rt=%d", rt);
            return rt;
        }
    }

    ws_clients_init();

    s_listen_fd = tal_net_socket_create(PROTOCOL_TCP);
    if (s_listen_fd < 0) {
        MIMI_LOGE(TAG, "create ws socket failed");
        return OPRT_SOCK_ERR;
    }

    (void)tal_net_set_reuse(s_listen_fd);
    (void)tal_net_set_block(s_listen_fd, FALSE);

    if (tal_net_bind(s_listen_fd, TY_IPADDR_ANY, MIMI_WS_PORT) < 0) {
        MIMI_LOGE(TAG, "bind ws failed port=%d errno=%d", MIMI_WS_PORT, tal_net_get_errno());
        ws_close_fd(&s_listen_fd);
        return OPRT_SOCK_ERR;
    }

    if (tal_net_listen(s_listen_fd, MIMI_WS_MAX_CLIENTS) < 0) {
        MIMI_LOGE(TAG, "listen ws failed errno=%d", tal_net_get_errno());
        ws_close_fd(&s_listen_fd);
        return OPRT_SOCK_ERR;
    }

    THREAD_CFG_T cfg = {0};
    cfg.stackDepth   = 10 * 1024;
    cfg.priority     = THREAD_PRIO_1;
    cfg.thrdname     = "mimi_ws";

    s_ws_running   = TRUE;
    OPERATE_RET rt = tal_thread_create_and_start(&s_ws_thread, NULL, NULL, ws_server_task, NULL, &cfg);
    if (rt != OPRT_OK) {
        s_ws_running = FALSE;
        ws_close_fd(&s_listen_fd);
        MIMI_LOGE(TAG, "create ws thread failed rt=%d", rt);
        return rt;
    }

    MIMI_LOGI(TAG, "ws server started port=%d", MIMI_WS_PORT);
    return OPRT_OK;
}

OPERATE_RET ws_server_send(const char *chat_id, const char *text)
{
    if (!chat_id || !text) {
        return OPRT_INVALID_PARM;
    }

    if (!s_ws_running || !s_ws_thread) {
        return OPRT_RESOURCE_NOT_READY;
    }

    tal_mutex_lock(s_ws_mutex);
    ws_client_t *client = ws_find_client_by_chat_id_locked(chat_id);
    if (!client || !client->handshake_done) {
        tal_mutex_unlock(s_ws_mutex);
        return OPRT_NOT_FOUND;
    }

    OPERATE_RET rt = ws_send_text_to_client_locked(client, chat_id, text);
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "send ws response failed chat_id=%s rt=%d", chat_id, rt);
        ws_close_client_locked(client);
    }
    tal_mutex_unlock(s_ws_mutex);
    return rt;
}

OPERATE_RET ws_server_stop(void)
{
    s_ws_running = FALSE;

    if (s_ws_thread) {
        tal_thread_delete(s_ws_thread);
        s_ws_thread = NULL;
    }

    if (!s_ws_mutex) {
        ws_close_fd(&s_listen_fd);
        return OPRT_OK;
    }

    for (int i = 0; i < MIMI_WS_MAX_CLIENTS; i++) {
        ws_close_client_locked(&s_clients[i]);
    }
    ws_close_fd(&s_listen_fd);
    tal_mutex_release(s_ws_mutex);
    s_ws_mutex = NULL;
    ws_clients_init();

    return OPRT_OK;
}
