#include "channels/discord_bot.h"

#include "bus/message_bus.h"
#include "cJSON.h"
#include "im_config.h"
#include "im_utils.h"
#include "proxy/http_proxy.h"
#include "certs/tls_cert_bundle.h"
#include "tuya_transporter.h"
#include "tuya_tls.h"

#include "mbedtls/ssl.h"
#include "tal_network.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>

static const char *TAG = "discord";

static char          s_bot_token[160] = {0};
static char          s_channel_id[32] = {0}; /* Optional default outbound channel */
static THREAD_HANDLE s_gateway_thread = NULL;

typedef enum {
    DC_CONN_NONE = 0,
    DC_CONN_PROXY,
    DC_CONN_DIRECT,
} dc_conn_mode_t;

typedef struct {
    dc_conn_mode_t     mode;
    proxy_conn_t      *proxy;
    tuya_transporter_t tcp;
    tuya_tls_hander    tls;
    int                socket_fd;
    uint8_t           *rx_buf;
    size_t             rx_cap;
    size_t             rx_len;
} dc_gateway_conn_t;

#define DC_HOST                IM_DC_API_HOST
#define DC_HTTP_TIMEOUT_MS     10000
#define DC_HTTP_RESP_BUF_SIZE  (16 * 1024)
#define DC_PROXY_READ_SLICE_MS 1000
#define DC_PROXY_READ_TOTAL_MS 15000

static OPERATE_RET dc_direct_open(dc_gateway_conn_t *conn, const char *host, int port, int timeout_ms)
{
    if (!conn || !host || port <= 0 || timeout_ms <= 0) {
        return OPRT_INVALID_PARM;
    }

    conn->tcp = tuya_transporter_create(TRANSPORT_TYPE_TCP, NULL);
    if (!conn->tcp) {
        return OPRT_COM_ERROR;
    }
    conn->mode = DC_CONN_DIRECT;

    tuya_tcp_config_t cfg = {0};
    cfg.isReuse           = TRUE;
    cfg.isDisableNagle    = TRUE;
    cfg.sendTimeoutMs     = timeout_ms;
    cfg.recvTimeoutMs     = timeout_ms;
    (void)tuya_transporter_ctrl(conn->tcp, TUYA_TRANSPORTER_SET_TCP_CONFIG, &cfg);

    OPERATE_RET rt = tuya_transporter_connect(conn->tcp, (char *)host, port, timeout_ms);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = tuya_transporter_ctrl(conn->tcp, TUYA_TRANSPORTER_GET_TCP_SOCKET, &conn->socket_fd);
    if (rt != OPRT_OK || conn->socket_fd < 0) {
        return OPRT_SOCK_ERR;
    }

    uint8_t *cacert      = NULL;
    size_t   cacert_len  = 0;
    bool     verify_peer = false;

    rt = im_tls_query_domain_certs(host, &cacert, &cacert_len);
    if (rt == OPRT_OK && cacert && cacert_len > 0) {
        verify_peer = true;
    } else {
        IM_LOGD(TAG, "tls cert unavailable for %s, fallback to TLS no-verify mode rt=%d", host, rt);
    }
    if (verify_peer && cacert_len > (size_t)INT_MAX) {
        IM_LOGW(TAG, "tls cert too large for tuya_tls host=%s len=%zu, fallback to no-verify", host, cacert_len);
        verify_peer = false;
    }

    conn->tls = tuya_tls_connect_create();
    if (!conn->tls) {
        if (cacert) {
            im_free(cacert);
        }
        return OPRT_MALLOC_FAILED;
    }

    int timeout_s = timeout_ms / 1000;
    if (timeout_s <= 0) {
        timeout_s = 1;
    }

    tuya_tls_config_t cfg_tls = {
        .mode         = TUYA_TLS_SERVER_CERT_MODE,
        .hostname     = (char *)host,
        .port         = (uint16_t)port,
        .timeout      = (uint32_t)timeout_s,
        .verify       = verify_peer,
        .ca_cert      = verify_peer ? (char *)cacert : NULL,
        .ca_cert_size = verify_peer ? (int)cacert_len : 0,
    };
    (void)tuya_tls_config_set(conn->tls, &cfg_tls);

    rt = tuya_tls_connect(conn->tls, (char *)host, port, conn->socket_fd, timeout_s);
    if (cacert) {
        im_free(cacert);
    }
    if (rt != OPRT_OK) {
        return rt;
    }

    conn->mode = DC_CONN_DIRECT;
    return OPRT_OK;
}

static OPERATE_RET dc_conn_ensure_rx_buf(dc_gateway_conn_t *conn, size_t min_cap)
{
    if (!conn || min_cap == 0) {
        return OPRT_INVALID_PARM;
    }

    if (conn->rx_buf && conn->rx_cap >= min_cap) {
        return OPRT_OK;
    }

    uint8_t *buf = im_malloc(min_cap);
    if (!buf) {
        return OPRT_MALLOC_FAILED;
    }

    if (conn->rx_buf) {
        im_free(conn->rx_buf);
    }

    conn->rx_buf = buf;
    conn->rx_cap = min_cap;
    conn->rx_len = 0;
    return OPRT_OK;
}

static void dc_conn_close(dc_gateway_conn_t *conn)
{
    if (!conn) {
        return;
    }

    if (conn->mode == DC_CONN_PROXY) {
        if (conn->proxy) {
            proxy_conn_close(conn->proxy);
            conn->proxy = NULL;
        }
    } else if (conn->mode == DC_CONN_DIRECT) {
        if (conn->tls) {
            (void)tuya_tls_disconnect(conn->tls);
            tuya_tls_connect_destroy(conn->tls);
            conn->tls = NULL;
        }
        if (conn->tcp) {
            (void)tuya_transporter_close(conn->tcp);
            (void)tuya_transporter_destroy(conn->tcp);
            conn->tcp = NULL;
        }
        conn->socket_fd = -1;
    }

    conn->mode = DC_CONN_NONE;
    if (conn->rx_buf) {
        im_free(conn->rx_buf);
        conn->rx_buf = NULL;
    }
    conn->rx_cap = 0;
    conn->rx_len = 0;
}

static OPERATE_RET dc_conn_open(dc_gateway_conn_t *conn, const char *host, int port, int timeout_ms)
{
    if (!conn || !host || port <= 0 || timeout_ms <= 0) {
        return OPRT_INVALID_PARM;
    }

    memset(conn, 0, sizeof(*conn));
    conn->socket_fd = -1;

    if (http_proxy_is_enabled()) {
        conn->proxy = proxy_conn_open(host, port, timeout_ms);
        if (!conn->proxy) {
            return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
        }
        conn->mode = DC_CONN_PROXY;
        return OPRT_OK;
    }

    OPERATE_RET rt = dc_direct_open(conn, host, port, timeout_ms);
    if (rt != OPRT_OK) {
        dc_conn_close(conn);
        return rt;
    }

    return OPRT_OK;
}

static int dc_conn_write(dc_gateway_conn_t *conn, const uint8_t *data, int len)
{
    if (!conn || !data || len <= 0) {
        return -1;
    }

    if (conn->mode == DC_CONN_PROXY) {
        return proxy_conn_write(conn->proxy, (const char *)data, len);
    }

    if (conn->mode != DC_CONN_DIRECT || !conn->tls) {
        return -1;
    }

    int sent = 0;
    while (sent < len) {
        int n = tuya_tls_write(conn->tls, (uint8_t *)data + sent, (uint32_t)(len - sent));
        if (n <= 0) {
            return -1;
        }
        sent += n;
    }

    return sent;
}

static int dc_conn_read(dc_gateway_conn_t *conn, uint8_t *buf, int len, int timeout_ms)
{
    if (!conn || !buf || len <= 0 || timeout_ms <= 0) {
        return -1;
    }

    if (conn->mode == DC_CONN_PROXY) {
        return proxy_conn_read(conn->proxy, (char *)buf, len, timeout_ms);
    }

    if (conn->mode != DC_CONN_DIRECT || !conn->tls || conn->socket_fd < 0) {
        return -1;
    }

    TUYA_FD_SET_T readfds;
    tal_net_fd_zero(&readfds);
    tal_net_fd_set(conn->socket_fd, &readfds);
    int ready = tal_net_select(conn->socket_fd + 1, &readfds, NULL, NULL, timeout_ms);
    if (ready < 0) {
        return -1;
    }
    if (ready == 0) {
        return OPRT_RESOURCE_NOT_READY;
    }

    int n = tuya_tls_read(conn->tls, buf, (uint32_t)len);
    if (n > 0) {
        return n;
    }
    if (n == 0 || n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        return 0;
    }
    if (n == OPRT_RESOURCE_NOT_READY || n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE ||
        n == MBEDTLS_ERR_SSL_TIMEOUT || n == -100) {
        return OPRT_RESOURCE_NOT_READY;
    }

    return n;
}

static OPERATE_RET dc_ws_send_frame(dc_gateway_conn_t *conn, uint8_t opcode, const uint8_t *payload, size_t payload_len)
{
    if (!conn || (payload_len > 0 && !payload)) {
        return OPRT_INVALID_PARM;
    }

    size_t  header_len = 0;
    uint8_t header[14] = {0};

    header[0] = (uint8_t)(0x80 | (opcode & 0x0F));

    if (payload_len <= 125) {
        header[1]  = (uint8_t)(0x80 | payload_len);
        header_len = 2;
    } else if (payload_len <= 0xFFFF) {
        header[1]  = (uint8_t)(0x80 | 126);
        header[2]  = (uint8_t)((payload_len >> 8) & 0xFF);
        header[3]  = (uint8_t)(payload_len & 0xFF);
        header_len = 4;
    } else {
        header[1]       = (uint8_t)(0x80 | 127);
        uint64_t plen64 = (uint64_t)payload_len;
        for (int i = 0; i < 8; i++) {
            header[2 + i] = (uint8_t)((plen64 >> (56 - i * 8)) & 0xFF);
        }
        header_len = 10;
    }

    uint32_t m       = (uint32_t)tal_system_get_random(0xFFFFFFFFu);
    uint8_t  mask[4] = {
        (uint8_t)(m & 0xFF),
        (uint8_t)((m >> 8) & 0xFF),
        (uint8_t)((m >> 16) & 0xFF),
        (uint8_t)((m >> 24) & 0xFF),
    };

    size_t   frame_len = header_len + 4 + payload_len;
    uint8_t *frame     = im_malloc(frame_len);
    if (!frame) {
        return OPRT_MALLOC_FAILED;
    }

    memcpy(frame, header, header_len);
    memcpy(frame + header_len, mask, 4);
    for (size_t i = 0; i < payload_len; i++) {
        frame[header_len + 4 + i] = (uint8_t)(payload[i] ^ mask[i % 4]);
    }

    int n = dc_conn_write(conn, frame, (int)frame_len);
    im_free(frame);

    return (n == (int)frame_len) ? OPRT_OK : OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
}

static OPERATE_RET dc_ws_handshake(dc_gateway_conn_t *conn)
{
    if (!conn) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = dc_conn_ensure_rx_buf(conn, IM_DC_GATEWAY_RX_BUF_SIZE);
    if (rt != OPRT_OK) {
        return rt;
    }

    const char *ws_key   = "dGhlIHNhbXBsZSBub25jZQ==";
    char        req[768] = {0};
    int         req_len  = snprintf(req, sizeof(req),
                                    "GET " IM_DC_GATEWAY_PATH " HTTP/1.1\r\n"
                                             "Host: " IM_DC_GATEWAY_HOST "\r\n"
                                             "Upgrade: websocket\r\n"
                                             "Connection: Upgrade\r\n"
                                             "Sec-WebSocket-Key: %s\r\n"
                                             "Sec-WebSocket-Version: 13\r\n"
                                             "User-Agent: IM/1.0\r\n\r\n",
                                    ws_key);
    if (req_len <= 0 || req_len >= (int)sizeof(req)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    if (dc_conn_write(conn, (const uint8_t *)req, req_len) != req_len) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    char     header[2048] = {0};
    int      total        = 0;
    int      header_end   = -1;
    uint32_t start_ms     = tal_system_get_millisecond();

    while ((int)(tal_system_get_millisecond() - start_ms) < DC_HTTP_TIMEOUT_MS && total < (int)sizeof(header) - 1) {
        int n = dc_conn_read(conn, (uint8_t *)header + total, (int)sizeof(header) - total - 1, 1000);
        if (n == OPRT_RESOURCE_NOT_READY) {
            continue;
        }
        if (n <= 0) {
            return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
        }

        total += n;
        header[total] = '\0';
        header_end    = im_find_header_end(header, total);
        if (header_end > 0) {
            break;
        }
    }

    if (header_end <= 0) {
        return OPRT_TIMEOUT;
    }

    uint16_t status = im_parse_http_status(header);
    if (status != 101) {
        IM_LOGE(TAG, "discord gateway handshake failed http=%u", status);
        return OPRT_COM_ERROR;
    }

    size_t remain = (size_t)(total - header_end);
    conn->rx_len  = 0;
    if (remain > 0) {
        if (remain > conn->rx_cap) {
            return OPRT_BUFFER_NOT_ENOUGH;
        }
        memcpy(conn->rx_buf, header + header_end, remain);
        conn->rx_len = remain;
    }

    IM_LOGI(TAG, "discord gateway handshake success");
    return OPRT_OK;
}

static OPERATE_RET dc_ws_decode_one_frame(dc_gateway_conn_t *conn, uint8_t *opcode, uint8_t **payload,
                                          size_t *payload_len, size_t *consumed)
{
    if (!conn || !opcode || !payload || !payload_len || !consumed) {
        return OPRT_INVALID_PARM;
    }
    if (!conn->rx_buf || conn->rx_cap == 0) {
        return OPRT_INVALID_PARM;
    }

    if (conn->rx_len < 2) {
        return OPRT_RESOURCE_NOT_READY;
    }

    const uint8_t *buf    = conn->rx_buf;
    bool           masked = (buf[1] & 0x80) != 0;
    uint64_t       plen   = (uint64_t)(buf[1] & 0x7F);
    size_t         off    = 2;

    if (plen == 126) {
        if (conn->rx_len < off + 2) {
            return OPRT_RESOURCE_NOT_READY;
        }
        plen = (uint64_t)((buf[off] << 8) | buf[off + 1]);
        off += 2;
    } else if (plen == 127) {
        if (conn->rx_len < off + 8) {
            return OPRT_RESOURCE_NOT_READY;
        }
        plen = 0;
        for (int i = 0; i < 8; i++) {
            plen = (plen << 8) | buf[off + i];
        }
        off += 8;
    }

    if (masked && conn->rx_len < off + 4) {
        return OPRT_RESOURCE_NOT_READY;
    }

    if (conn->rx_cap <= 16 || plen > (uint64_t)(conn->rx_cap - 16)) {
        return OPRT_MSG_OUT_OF_LIMIT;
    }

    size_t frame_len = off + (masked ? 4 : 0) + (size_t)plen;
    if (conn->rx_len < frame_len) {
        return OPRT_RESOURCE_NOT_READY;
    }

    uint8_t mask[4] = {0};
    if (masked) {
        memcpy(mask, buf + off, 4);
        off += 4;
    }

    uint8_t *data = im_malloc((size_t)plen + 1);
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

    *opcode      = (uint8_t)(buf[0] & 0x0F);
    *payload     = data;
    *payload_len = (size_t)plen;
    *consumed    = frame_len;

    return OPRT_OK;
}

static void dc_ws_consume_rx(dc_gateway_conn_t *conn, size_t consumed)
{
    if (!conn || consumed == 0 || consumed > conn->rx_len) {
        return;
    }

    if (consumed < conn->rx_len) {
        memmove(conn->rx_buf, conn->rx_buf + consumed, conn->rx_len - consumed);
    }
    conn->rx_len -= consumed;
}

static OPERATE_RET dc_ws_poll_frame(dc_gateway_conn_t *conn, int wait_ms, uint8_t *opcode, uint8_t **payload,
                                    size_t *payload_len)
{
    if (!conn || !opcode || !payload || !payload_len) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET ensure_rt = dc_conn_ensure_rx_buf(conn, IM_DC_GATEWAY_RX_BUF_SIZE);
    if (ensure_rt != OPRT_OK) {
        return ensure_rt;
    }

    *payload     = NULL;
    *payload_len = 0;

    size_t      consumed = 0;
    OPERATE_RET rt       = dc_ws_decode_one_frame(conn, opcode, payload, payload_len, &consumed);
    if (rt == OPRT_OK) {
        dc_ws_consume_rx(conn, consumed);
        return OPRT_OK;
    }
    if (rt != OPRT_RESOURCE_NOT_READY) {
        return rt;
    }

    uint8_t tmp[1024] = {0};
    int     n         = dc_conn_read(conn, tmp, sizeof(tmp), wait_ms);
    if (n == OPRT_RESOURCE_NOT_READY) {
        return OPRT_RESOURCE_NOT_READY;
    }
    if (n <= 0) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    if (conn->rx_len + (size_t)n > conn->rx_cap) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    memcpy(conn->rx_buf + conn->rx_len, tmp, (size_t)n);
    conn->rx_len += (size_t)n;

    consumed = 0;
    rt       = dc_ws_decode_one_frame(conn, opcode, payload, payload_len, &consumed);
    if (rt == OPRT_OK) {
        dc_ws_consume_rx(conn, consumed);
    }
    return rt;
}

static OPERATE_RET dc_gateway_send_identify(dc_gateway_conn_t *conn)
{
    /* Discord IDENTIFY frame payload includes "op":2. */
    char payload[512] = {0};
    int  n            = snprintf(payload, sizeof(payload),
                                 "{\"op\":2,\"d\":{\"token\":\"%s\",\"intents\":%u,\"properties\":{\"os\":\"tuyaopen\",\"browser\":"
                                             "\"im_bot\",\"device\":\"im_bot\"}}}",
                                 s_bot_token, (unsigned)IM_DC_GATEWAY_INTENTS);
    if (n <= 0 || n >= (int)sizeof(payload)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    IM_LOGI(TAG, "discord gateway identify sent intents=%u", (unsigned)IM_DC_GATEWAY_INTENTS);
    return dc_ws_send_frame(conn, 0x1, (const uint8_t *)payload, (size_t)n);
}

static OPERATE_RET dc_gateway_send_heartbeat(dc_gateway_conn_t *conn, int64_t seq)
{
    char payload[96] = {0};
    int  n           = 0;
    if (seq >= 0) {
        n = snprintf(payload, sizeof(payload), "{\"op\":1,\"d\":%lld}", (long long)seq);
    } else {
        n = snprintf(payload, sizeof(payload), "{\"op\":1,\"d\":null}");
    }

    if (n <= 0 || n >= (int)sizeof(payload)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    return dc_ws_send_frame(conn, 0x1, (const uint8_t *)payload, (size_t)n);
}

static void publish_inbound_discord(const char *channel_id, const char *content)
{
    if (!channel_id || !content || channel_id[0] == '\0') {
        return;
    }

    im_msg_t in = {0};
    strncpy(in.channel, IM_CHAN_DISCORD, sizeof(in.channel) - 1);
    strncpy(in.chat_id, channel_id, sizeof(in.chat_id) - 1);
    in.content = im_strdup(content);
    if (!in.content) {
        return;
    }

    OPERATE_RET rt = message_bus_push_inbound(&in);
    if (rt != OPRT_OK) {
        IM_LOGW(TAG, "push inbound failed rt=%d", rt);
        im_free(in.content);
    }
}

static void handle_message_create_event(cJSON *event)
{
    if (!cJSON_IsObject(event)) {
        return;
    }

    cJSON *author = cJSON_GetObjectItem(event, "author");
    cJSON *bot    = author ? cJSON_GetObjectItem(author, "bot") : NULL;
    if (cJSON_IsTrue(bot)) {
        return;
    }

    const char *channel_id      = cJSON_GetStringValue(cJSON_GetObjectItem(event, "channel_id"));
    const char *message_id      = cJSON_GetStringValue(cJSON_GetObjectItem(event, "id"));
    const char *content         = cJSON_GetStringValue(cJSON_GetObjectItem(event, "content"));
    cJSON      *attachments     = cJSON_GetObjectItem(event, "attachments");
    bool        has_attachments = cJSON_IsArray(attachments) && cJSON_GetArraySize(attachments) > 0;

    if (!channel_id || channel_id[0] == '\0') {
        return;
    }

    if (content && content[0] != '\0') {
        IM_LOGI(TAG, "rx inbound_text channel=%s chat=%s len=%u", IM_CHAN_DISCORD, channel_id,
                  (unsigned)strlen(content));
    }

    if (has_attachments) {
        cJSON      *first     = cJSON_GetArrayItem(attachments, 0);
        const char *file_name = im_json_str(first, "filename", "<empty>");
        const char *mime_type = im_json_str(first, "content_type", "<empty>");
        uint32_t    file_size = im_json_uint(first, "size", 0);
        IM_LOGI(TAG, "rx attachment chat=%s message_id=%s name=%s mime=%s size=%u", channel_id,
                  message_id ? message_id : "", file_name, mime_type, (unsigned)file_size);
    }

    if (!content || content[0] == '\0') {
        if (has_attachments) {
            content = "[non-text message]";
        } else {
            return;
        }
    }

    im_safe_copy(s_channel_id, sizeof(s_channel_id), channel_id);
    publish_inbound_discord(channel_id, content);
}

static OPERATE_RET handle_gateway_payload(dc_gateway_conn_t *conn, const char *json_str, int64_t *seq,
                                          uint32_t *heartbeat_ms, uint32_t *next_heartbeat_ms)
{
    if (!conn || !json_str || !seq || !heartbeat_ms || !next_heartbeat_ms) {
        return OPRT_INVALID_PARM;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return OPRT_CR_CJSON_ERR;
    }

    cJSON *op_item = cJSON_GetObjectItem(root, "op");
    int    op      = cJSON_IsNumber(op_item) ? (int)op_item->valuedouble : -1;

    cJSON *seq_item = cJSON_GetObjectItem(root, "s");
    if (cJSON_IsNumber(seq_item)) {
        *seq = (int64_t)seq_item->valuedouble;
    }

    OPERATE_RET rt = OPRT_OK;

    if (op == 10) {
        cJSON *d  = cJSON_GetObjectItem(root, "d");
        cJSON *hb = d ? cJSON_GetObjectItem(d, "heartbeat_interval") : NULL;
        if (cJSON_IsNumber(hb) && hb->valuedouble > 0) {
            *heartbeat_ms = (uint32_t)hb->valuedouble;
            if (*heartbeat_ms < 1000) {
                *heartbeat_ms = 1000;
            }
            *next_heartbeat_ms = tal_system_get_millisecond() + *heartbeat_ms;
            IM_LOGI(TAG, "discord gateway HELLO heartbeat=%u ms", (unsigned)*heartbeat_ms);
        }

        rt = dc_gateway_send_identify(conn);
    } else if (op == 11) {
        IM_LOGD(TAG, "discord gateway HEARTBEAT_ACK");
    } else if (op == 1) {
        rt = dc_gateway_send_heartbeat(conn, *seq);
        if (rt == OPRT_OK && *heartbeat_ms > 0) {
            *next_heartbeat_ms = tal_system_get_millisecond() + *heartbeat_ms;
        }
    } else if (op == 7 || op == 9) {
        IM_LOGW(TAG, "discord gateway requested reconnect op=%d", op);
        rt = OPRT_COM_ERROR;
    } else if (op == 0) {
        const char *event_type = cJSON_GetStringValue(cJSON_GetObjectItem(root, "t"));
        cJSON      *d          = cJSON_GetObjectItem(root, "d");

        if (event_type && strcmp(event_type, "READY") == 0) {
            const char *uid   = NULL;
            const char *uname = NULL;
            cJSON      *user  = d ? cJSON_GetObjectItem(d, "user") : NULL;
            if (user) {
                uid   = cJSON_GetStringValue(cJSON_GetObjectItem(user, "id"));
                uname = cJSON_GetStringValue(cJSON_GetObjectItem(user, "username"));
            }
            IM_LOGI(TAG, "discord gateway READY user=%s(%s)", uname ? uname : "", uid ? uid : "");
        } else if (event_type && strcmp(event_type, "MESSAGE_CREATE") == 0) {
            handle_message_create_event(d);
        }
    }

    cJSON_Delete(root);
    return rt;
}

static void discord_gateway_task(void *arg)
{
    (void)arg;

    IM_LOGI(TAG, "discord gateway task started");

    while (1) {
        if (s_bot_token[0] == '\0') {
            tal_system_sleep(3000);
            continue;
        }

        dc_gateway_conn_t *conn = im_calloc(1, sizeof(dc_gateway_conn_t));
        if (!conn) {
            tal_system_sleep(IM_DC_GATEWAY_RECONNECT_MS);
            continue;
        }

        OPERATE_RET rt = dc_conn_open(conn, IM_DC_GATEWAY_HOST, 443, DC_HTTP_TIMEOUT_MS);
        if (rt != OPRT_OK) {
            IM_LOGW(TAG, "discord gateway connect failed rt=%d", rt);
            im_free(conn);
            tal_system_sleep(IM_DC_GATEWAY_RECONNECT_MS);
            continue;
        }

        rt = dc_ws_handshake(conn);
        if (rt != OPRT_OK) {
            IM_LOGW(TAG, "discord gateway handshake failed rt=%d", rt);
            dc_conn_close(conn);
            im_free(conn);
            tal_system_sleep(IM_DC_GATEWAY_RECONNECT_MS);
            continue;
        }

        int64_t  seq               = -1;
        uint32_t heartbeat_ms      = 0;
        uint32_t next_heartbeat_ms = 0;

        while (1) {
            if (heartbeat_ms > 0) {
                uint32_t now = tal_system_get_millisecond();
                if ((int32_t)(now - next_heartbeat_ms) >= 0) {
                    OPERATE_RET hb_rt = dc_gateway_send_heartbeat(conn, seq);
                    if (hb_rt != OPRT_OK) {
                        IM_LOGW(TAG, "discord gateway heartbeat failed rt=%d", hb_rt);
                        break;
                    }
                    next_heartbeat_ms = now + heartbeat_ms;
                }
            }

            uint8_t  opcode      = 0;
            uint8_t *payload     = NULL;
            size_t   payload_len = 0;
            rt                   = dc_ws_poll_frame(conn, 500, &opcode, &payload, &payload_len);
            if (rt == OPRT_RESOURCE_NOT_READY) {
                continue;
            }
            if (rt != OPRT_OK) {
                im_free(payload);
                IM_LOGW(TAG, "discord gateway poll failed rt=%d", rt);
                break;
            }

            if (opcode == 0x1) {
                if (payload && payload_len > 0) {
                    OPERATE_RET hrt =
                        handle_gateway_payload(conn, (const char *)payload, &seq, &heartbeat_ms, &next_heartbeat_ms);
                    if (hrt != OPRT_OK) {
                        im_free(payload);
                        break;
                    }
                }
            } else if (opcode == 0x8) {
                int         close_code   = -1;
                const char *close_reason = "";
                if (payload && payload_len >= 2) {
                    close_code = ((int)payload[0] << 8) | (int)payload[1];
                    if (payload_len > 2) {
                        close_reason = (const char *)(payload + 2);
                    }
                }
                im_free(payload);
                IM_LOGW(TAG, "discord gateway closed by peer code=%d reason=%.120s", close_code, close_reason);
                break;
            } else if (opcode == 0x9) {
                (void)dc_ws_send_frame(conn, 0xA, payload, payload_len);
            }

            im_free(payload);
        }

        dc_conn_close(conn);
        im_free(conn);
        tal_system_sleep(IM_DC_GATEWAY_RECONNECT_MS);
    }
}

static OPERATE_RET dc_http_call_via_proxy(const char *path, const char *method, const char *post_data, char *resp_buf,
                                          size_t resp_buf_size, uint16_t *status_code)
{
    proxy_conn_t *conn = proxy_conn_open(DC_HOST, 443, DC_HTTP_TIMEOUT_MS);
    if (!conn) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    int  body_len         = post_data ? (int)strlen(post_data) : 0;
    char req_header[1024] = {0};
    int  req_len;
    if (post_data) {
        req_len = snprintf(req_header, sizeof(req_header),
                           "%s %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Authorization: Bot %s\r\n"
                           "User-Agent: IM/1.0\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n\r\n",
                           method, path, DC_HOST, s_bot_token, body_len);
    } else {
        req_len = snprintf(req_header, sizeof(req_header),
                           "%s %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Authorization: Bot %s\r\n"
                           "User-Agent: IM/1.0\r\n"
                           "Connection: close\r\n\r\n",
                           method, path, DC_HOST, s_bot_token);
    }

    if (req_len <= 0 || req_len >= (int)sizeof(req_header)) {
        proxy_conn_close(conn);
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    if (proxy_conn_write(conn, req_header, req_len) != req_len) {
        proxy_conn_close(conn);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }
    if (body_len > 0 && proxy_conn_write(conn, post_data, body_len) != body_len) {
        proxy_conn_close(conn);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    size_t raw_cap = 4096;
    size_t raw_len = 0;
    char  *raw     = im_calloc(1, raw_cap);
    if (!raw) {
        proxy_conn_close(conn);
        return OPRT_MALLOC_FAILED;
    }

    uint32_t wait_begin_ms = tal_system_get_millisecond();
    while (1) {
        if (raw_len + 1024 >= raw_cap) {
            size_t new_cap = raw_cap * 2;
            char  *tmp     = im_realloc(raw, new_cap);
            if (!tmp) {
                im_free(raw);
                proxy_conn_close(conn);
                return OPRT_MALLOC_FAILED;
            }
            raw     = tmp;
            raw_cap = new_cap;
        }

        int n = proxy_conn_read(conn, raw + raw_len, (int)(raw_cap - raw_len - 1), DC_PROXY_READ_SLICE_MS);
        if (n == OPRT_RESOURCE_NOT_READY) {
            if ((int)(tal_system_get_millisecond() - wait_begin_ms) >= DC_PROXY_READ_TOTAL_MS) {
                im_free(raw);
                proxy_conn_close(conn);
                return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
            }
            continue;
        }
        if (n < 0) {
            if (raw_len > 0) {
                break;
            }
            im_free(raw);
            proxy_conn_close(conn);
            return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
        }
        if (n == 0) {
            break;
        }

        raw_len += (size_t)n;
        raw[raw_len]  = '\0';
        wait_begin_ms = tal_system_get_millisecond();
    }
    proxy_conn_close(conn);

    if (raw_len == 0) {
        im_free(raw);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    if (status_code) {
        *status_code = im_parse_http_status(raw);
    }

    resp_buf[0] = '\0';
    char *body  = strstr(raw, "\r\n\r\n");
    if (body) {
        body += 4;
        size_t body_len_sz = strlen(body);
        size_t copy        = (body_len_sz < resp_buf_size - 1) ? body_len_sz : (resp_buf_size - 1);
        memcpy(resp_buf, body, copy);
        resp_buf[copy] = '\0';
    }
    im_free(raw);

    return OPRT_OK;
}

static OPERATE_RET dc_http_call_direct(const char *path, const char *method, const char *post_data, char *resp_buf,
                                       size_t resp_buf_size, uint16_t *status_code)
{
    dc_gateway_conn_t *conn = im_calloc(1, sizeof(dc_gateway_conn_t));
    if (!conn) {
        return OPRT_MALLOC_FAILED;
    }

    OPERATE_RET rt = dc_direct_open(conn, DC_HOST, 443, DC_HTTP_TIMEOUT_MS);
    if (rt != OPRT_OK) {
        dc_conn_close(conn);
        im_free(conn);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    int  body_len         = post_data ? (int)strlen(post_data) : 0;
    char req_header[1024] = {0};
    int  req_len;
    if (post_data) {
        req_len = snprintf(req_header, sizeof(req_header),
                           "%s %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Authorization: Bot %s\r\n"
                           "User-Agent: IM/1.0\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n\r\n",
                           method, path, DC_HOST, s_bot_token, body_len);
    } else {
        req_len = snprintf(req_header, sizeof(req_header),
                           "%s %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Authorization: Bot %s\r\n"
                           "User-Agent: IM/1.0\r\n"
                           "Connection: close\r\n\r\n",
                           method, path, DC_HOST, s_bot_token);
    }
    if (req_len <= 0 || req_len >= (int)sizeof(req_header)) {
        dc_conn_close(conn);
        im_free(conn);
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    if (dc_conn_write(conn, (const uint8_t *)req_header, req_len) != req_len) {
        dc_conn_close(conn);
        im_free(conn);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }
    if (body_len > 0 && dc_conn_write(conn, (const uint8_t *)post_data, body_len) != body_len) {
        dc_conn_close(conn);
        im_free(conn);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    size_t raw_cap = 4096;
    size_t raw_len = 0;
    char  *raw     = im_calloc(1, raw_cap);
    if (!raw) {
        dc_conn_close(conn);
        im_free(conn);
        return OPRT_MALLOC_FAILED;
    }

    uint32_t wait_begin_ms = tal_system_get_millisecond();
    while (1) {
        if (raw_len + 1024 >= raw_cap) {
            size_t new_cap = raw_cap * 2;
            char  *tmp     = im_realloc(raw, new_cap);
            if (!tmp) {
                im_free(raw);
                dc_conn_close(conn);
                im_free(conn);
                return OPRT_MALLOC_FAILED;
            }
            raw     = tmp;
            raw_cap = new_cap;
        }

        int n = dc_conn_read(conn, (uint8_t *)raw + raw_len, (int)(raw_cap - raw_len - 1), DC_PROXY_READ_SLICE_MS);
        if (n == OPRT_RESOURCE_NOT_READY) {
            if ((int)(tal_system_get_millisecond() - wait_begin_ms) >= DC_PROXY_READ_TOTAL_MS) {
                im_free(raw);
                dc_conn_close(conn);
                im_free(conn);
                return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
            }
            continue;
        }
        if (n < 0) {
            if (raw_len > 0) {
                break;
            }
            im_free(raw);
            dc_conn_close(conn);
            im_free(conn);
            return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
        }
        if (n == 0) {
            break;
        }

        raw_len += (size_t)n;
        raw[raw_len]  = '\0';
        wait_begin_ms = tal_system_get_millisecond();
    }
    dc_conn_close(conn);
    im_free(conn);

    if (raw_len == 0) {
        im_free(raw);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    if (status_code) {
        *status_code = im_parse_http_status(raw);
    }

    resp_buf[0] = '\0';
    char *body  = strstr(raw, "\r\n\r\n");
    if (body) {
        body += 4;
        size_t body_len_sz = strlen(body);
        size_t copy        = (body_len_sz < resp_buf_size - 1) ? body_len_sz : (resp_buf_size - 1);
        memcpy(resp_buf, body, copy);
        resp_buf[copy] = '\0';
    }

    im_free(raw);
    return OPRT_OK;
}

static OPERATE_RET dc_http_call(const char *path, const char *method, const char *post_data, char *resp_buf,
                                size_t resp_buf_size, uint16_t *status_code)
{
    if (!path || !method || !resp_buf || resp_buf_size == 0) {
        return OPRT_INVALID_PARM;
    }
    if (s_bot_token[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    if (http_proxy_is_enabled()) {
        return dc_http_call_via_proxy(path, method, post_data, resp_buf, resp_buf_size, status_code);
    }

    return dc_http_call_direct(path, method, post_data, resp_buf, resp_buf_size, status_code);
}

static uint32_t parse_retry_after_ms(const char *json_str)
{
    if (!json_str || json_str[0] == '\0') {
        return 0;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return 0;
    }

    uint32_t retry_ms    = 0;
    cJSON   *retry_after = cJSON_GetObjectItem(root, "retry_after");
    if (cJSON_IsNumber(retry_after) && retry_after->valuedouble > 0) {
        double value = retry_after->valuedouble;
        if (value < 1000.0) {
            value *= 1000.0;
        }
        if (value > 120000.0) {
            value = 120000.0;
        }
        retry_ms = (uint32_t)value;
    }

    cJSON_Delete(root);
    return retry_ms;
}

static void parse_message_id(const char *json_str, char *msg_id, size_t msg_id_size)
{
    if (!msg_id || msg_id_size == 0) {
        return;
    }
    msg_id[0] = '\0';
    if (!json_str || json_str[0] == '\0') {
        return;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return;
    }

    const char *id = cJSON_GetStringValue(cJSON_GetObjectItem(root, "id"));
    if (id && id[0] != '\0') {
        im_safe_copy(msg_id, msg_id_size, id);
    }
    cJSON_Delete(root);
}

OPERATE_RET discord_bot_init(void)
{
    if (IM_SECRET_DC_TOKEN[0] != '\0') {
        im_safe_copy(s_bot_token, sizeof(s_bot_token), IM_SECRET_DC_TOKEN);
    }
    if (IM_SECRET_DC_CHANNEL_ID[0] != '\0') {
        im_safe_copy(s_channel_id, sizeof(s_channel_id), IM_SECRET_DC_CHANNEL_ID);
    }

    char tmp[160] = {0};
    if (im_kv_get_string(IM_NVS_DC, IM_NVS_KEY_DC_TOKEN, tmp, sizeof(tmp)) == OPRT_OK && tmp[0] != '\0') {
        im_safe_copy(s_bot_token, sizeof(s_bot_token), tmp);
    }

    memset(tmp, 0, sizeof(tmp));
    if (im_kv_get_string(IM_NVS_DC, IM_NVS_KEY_DC_CHANNEL_ID, tmp, sizeof(tmp)) == OPRT_OK && tmp[0] != '\0') {
        im_safe_copy(s_channel_id, sizeof(s_channel_id), tmp);
    }

    IM_LOGI(TAG, "discord init credential=%s default_channel=%s", s_bot_token[0] ? "configured" : "empty",
              s_channel_id[0] ? "configured" : "empty");
    return OPRT_OK;
}

OPERATE_RET discord_bot_start(void)
{
    if (s_bot_token[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    if (s_gateway_thread) {
        return OPRT_OK;
    }

    THREAD_CFG_T cfg = {0};
    cfg.stackDepth   = IM_DC_POLL_STACK;
    cfg.priority     = THREAD_PRIO_1;
    cfg.thrdname     = "im_dc_gw";

    OPERATE_RET rt = tal_thread_create_and_start(&s_gateway_thread, NULL, NULL, discord_gateway_task, NULL, &cfg);
    if (rt != OPRT_OK) {
        IM_LOGE(TAG, "create discord gateway thread failed: %d", rt);
        return rt;
    }

    return OPRT_OK;
}

OPERATE_RET discord_send_message(const char *channel_id, const char *text)
{
    if (!text) {
        return OPRT_INVALID_PARM;
    }
    if (s_bot_token[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    const char *target_channel = channel_id;
    if (!target_channel || target_channel[0] == '\0') {
        target_channel = s_channel_id;
    }
    if (!target_channel || target_channel[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    size_t text_len = strlen(text);
    size_t offset   = 0;
    bool   all_ok   = true;

    while (offset < text_len || (text_len == 0 && offset == 0)) {
        size_t chunk = text_len - offset;
        if (chunk > IM_DC_MAX_MSG_LEN) {
            chunk = IM_DC_MAX_MSG_LEN;
        }
        if (text_len == 0) {
            chunk = 0;
        }

        char *segment = im_calloc(1, chunk + 1);
        if (!segment) {
            return OPRT_MALLOC_FAILED;
        }
        if (chunk > 0) {
            memcpy(segment, text + offset, chunk);
        }
        segment[chunk] = '\0';

        cJSON *body = cJSON_CreateObject();
        if (!body) {
            im_free(segment);
            return OPRT_MALLOC_FAILED;
        }
        cJSON_AddStringToObject(body, "content", segment);
        char *json = cJSON_PrintUnformatted(body);
        cJSON_Delete(body);
        if (!json) {
            im_free(segment);
            return OPRT_MALLOC_FAILED;
        }

        char path[320] = {0};
        snprintf(path, sizeof(path), IM_DC_API_BASE "/channels/%s/messages", target_channel);

        char *resp = im_malloc(DC_HTTP_RESP_BUF_SIZE);
        if (!resp) {
            cJSON_free(json);
            im_free(segment);
            return OPRT_MALLOC_FAILED;
        }
        memset(resp, 0, DC_HTTP_RESP_BUF_SIZE);

        uint16_t    status = 0;
        OPERATE_RET rt     = dc_http_call(path, "POST", json, resp, DC_HTTP_RESP_BUF_SIZE, &status);
        if (rt == OPRT_OK && (status == 200 || status == 201)) {
            char msg_id[40] = {0};
            parse_message_id(resp, msg_id, sizeof(msg_id));
            IM_LOGI(TAG, "discord send success channel=%s bytes=%u message_id=%s", target_channel, (unsigned)chunk,
                      msg_id[0] ? msg_id : "unknown");
        } else if (status == 429) {
            uint32_t retry_ms = parse_retry_after_ms(resp);
            if (retry_ms == 0) {
                retry_ms = 2000;
            }
            IM_LOGW(TAG, "discord send rate limited, retry in %u ms", (unsigned)retry_ms);
            tal_system_sleep(retry_ms);
            memset(resp, 0, DC_HTTP_RESP_BUF_SIZE);
            status = 0;
            rt     = dc_http_call(path, "POST", json, resp, DC_HTTP_RESP_BUF_SIZE, &status);
            if (rt == OPRT_OK && (status == 200 || status == 201)) {
                char msg_id[40] = {0};
                parse_message_id(resp, msg_id, sizeof(msg_id));
                IM_LOGI(TAG, "discord send success channel=%s bytes=%u message_id=%s", target_channel,
                          (unsigned)chunk, msg_id[0] ? msg_id : "unknown");
            } else {
                all_ok = false;
                IM_LOGE(TAG, "discord send failed channel=%s rt=%d http=%u", target_channel, rt, status);
            }
        } else {
            all_ok = false;
            IM_LOGE(TAG, "discord send failed channel=%s rt=%d http=%u", target_channel, rt, status);
        }

        im_free(resp);
        cJSON_free(json);
        im_free(segment);
        if (text_len == 0) {
            break;
        }
        offset += chunk;
    }

    return all_ok ? OPRT_OK : OPRT_COM_ERROR;
}

OPERATE_RET discord_set_token(const char *token)
{
    if (!token) {
        return OPRT_INVALID_PARM;
    }

    im_safe_copy(s_bot_token, sizeof(s_bot_token), token);
    return im_kv_set_string(IM_NVS_DC, IM_NVS_KEY_DC_TOKEN, token);
}

OPERATE_RET discord_set_channel_id(const char *channel_id)
{
    if (!channel_id) {
        return OPRT_INVALID_PARM;
    }

    im_safe_copy(s_channel_id, sizeof(s_channel_id), channel_id);
    return im_kv_set_string(IM_NVS_DC, IM_NVS_KEY_DC_CHANNEL_ID, channel_id);
}
