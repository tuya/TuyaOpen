#include "proxy/http_proxy.h"

#include "im_config.h"
#include "tal_network.h"
#include "certs/tls_cert_bundle.h"
#include "tuya_transporter.h"
#include "tuya_tls.h"
#include "mbedtls/ssl.h"
#include <limits.h>

#include "im_utils.h"

struct proxy_conn {
    tuya_transporter_t tcp;
    tuya_tls_hander    tls;
    int                socket_fd;
};

static const char *TAG              = "proxy";
static char        s_proxy_host[64] = {0};
static uint16_t    s_proxy_port     = 0;
static char        s_proxy_type[8]  = "http"; /* http | socks5 */

static bool proxy_type_valid(const char *type)
{
    return type && (strcmp(type, "http") == 0 || strcmp(type, "socks5") == 0);
}

static void proxy_type_set(const char *type)
{
    if (type && strcmp(type, "socks5") == 0) {
        memcpy(s_proxy_type, "socks5", sizeof("socks5"));
    } else {
        memcpy(s_proxy_type, "http", sizeof("http"));
    }
}

static bool proxy_type_is_socks5(void)
{
    return strcmp(s_proxy_type, "socks5") == 0;
}

static int proxy_write_all_tcp(tuya_transporter_t tcp, const void *data, int len, int timeout_ms)
{
    int sent = 0;
    while (sent < len) {
        int n = tuya_transporter_write(tcp, (uint8_t *)data + sent, len - sent, timeout_ms);
        if (n <= 0) {
            return -1;
        }
        sent += n;
    }
    return sent;
}

static int proxy_read_exact_tcp(tuya_transporter_t tcp, uint8_t *buf, int len, int timeout_ms)
{
    int      got      = 0;
    uint32_t start_ms = tal_system_get_millisecond();

    while (got < len) {
        uint32_t now_ms = tal_system_get_millisecond();
        if ((int)(now_ms - start_ms) >= timeout_ms) {
            return -1;
        }

        int remain_ms = timeout_ms - (int)(now_ms - start_ms);
        if (remain_ms < 50) {
            remain_ms = 50;
        }

        int n = tuya_transporter_read(tcp, buf + got, len - got, remain_ms);
        if (n == OPRT_RESOURCE_NOT_READY) {
            tal_system_sleep(10);
            continue;
        }
        if (n <= 0) {
            return -1;
        }
        got += n;
    }

    return got;
}

static int proxy_read_headers(tuya_transporter_t tcp, char *buf, int size, int timeout_ms)
{
    if (!buf || size <= 4) {
        return -1;
    }

    int      total    = 0;
    uint32_t start_ms = tal_system_get_millisecond();
    while (total < size - 1) {
        uint32_t now_ms = tal_system_get_millisecond();
        if ((int)(now_ms - start_ms) >= timeout_ms) {
            break;
        }
        int remain_ms = timeout_ms - (int)(now_ms - start_ms);
        if (remain_ms < 50) {
            remain_ms = 50;
        }

        int n = tuya_transporter_read(tcp, (uint8_t *)buf + total, size - total - 1, remain_ms);
        if (n == OPRT_RESOURCE_NOT_READY) {
            tal_system_sleep(10);
            continue;
        }
        if (n <= 0) {
            return -1;
        }
        total += n;
        buf[total] = '\0';
        if (im_find_header_end(buf, total) > 0) {
            return total;
        }
    }

    return -1;
}

static OPERATE_RET proxy_open_transport(proxy_conn_t *conn, int timeout_ms)
{
    if (!conn) {
        return OPRT_INVALID_PARM;
    }

    conn->tcp = tuya_transporter_create(TRANSPORT_TYPE_TCP, NULL);
    if (!conn->tcp) {
        IM_LOGE(TAG, "create tcp transporter failed");
        return OPRT_COM_ERROR;
    }

    tuya_tcp_config_t cfg = {0};
    cfg.isReuse           = TRUE;
    cfg.isDisableNagle    = TRUE;
    cfg.sendTimeoutMs     = timeout_ms;
    cfg.recvTimeoutMs     = timeout_ms;
    (void)tuya_transporter_ctrl(conn->tcp, TUYA_TRANSPORTER_SET_TCP_CONFIG, &cfg);

    OPERATE_RET rt = tuya_transporter_connect(conn->tcp, s_proxy_host, s_proxy_port, timeout_ms);
    if (rt != OPRT_OK) {
        IM_LOGE(TAG, "connect proxy failed %s:%u rt=%d", s_proxy_host, s_proxy_port, rt);
        tuya_transporter_destroy(conn->tcp);
        conn->tcp = NULL;
        return rt;
    }

    return OPRT_OK;
}

static OPERATE_RET open_http_connect_tunnel(proxy_conn_t *conn, const char *host, int port, int timeout_ms)
{
    char req[512] = {0};
    int  req_len  = snprintf(req, sizeof(req),
                             "CONNECT %s:%d HTTP/1.1\r\n"
                               "Host: %s:%d\r\n"
                               "Proxy-Connection: Keep-Alive\r\n"
                               "Connection: Keep-Alive\r\n\r\n",
                             host, port, host, port);
    if (req_len <= 0 || req_len >= (int)sizeof(req)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    if (proxy_write_all_tcp(conn->tcp, req, req_len, timeout_ms) != req_len) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    char header[1024] = {0};
    int  hdr_len      = proxy_read_headers(conn->tcp, header, sizeof(header), timeout_ms);
    if (hdr_len <= 0) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    int code = im_parse_http_status(header);
    if (code != 200) {
        IM_LOGE(TAG, "CONNECT rejected code=%d", code);
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

static OPERATE_RET open_socks5_tunnel(proxy_conn_t *conn, const char *host, int port, int timeout_ms)
{
    if (!conn || !conn->tcp || !host || port <= 0) {
        return OPRT_INVALID_PARM;
    }

    size_t host_len = strlen(host);
    if (host_len == 0 || host_len > 255) {
        IM_LOGE(TAG, "invalid socks5 host len=%u", (unsigned)host_len);
        return OPRT_INVALID_PARM;
    }

    uint8_t greeting[3] = {0x05, 0x01, 0x00};
    if (proxy_write_all_tcp(conn->tcp, greeting, sizeof(greeting), timeout_ms) != (int)sizeof(greeting)) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    uint8_t greet_resp[2] = {0};
    if (proxy_read_exact_tcp(conn->tcp, greet_resp, sizeof(greet_resp), timeout_ms) != (int)sizeof(greet_resp)) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }
    if (greet_resp[0] != 0x05 || greet_resp[1] != 0x00) {
        IM_LOGE(TAG, "SOCKS5 greeting rejected ver=%u method=%u", greet_resp[0], greet_resp[1]);
        return OPRT_COM_ERROR;
    }

    uint8_t req[4 + 1 + 255 + 2] = {0};
    size_t  req_len              = 0;
    req[req_len++]               = 0x05;
    req[req_len++]               = 0x01;
    req[req_len++]               = 0x00;
    req[req_len++]               = 0x03;
    req[req_len++]               = (uint8_t)host_len;
    memcpy(req + req_len, host, host_len);
    req_len += host_len;
    req[req_len++] = (uint8_t)((port >> 8) & 0xFF);
    req[req_len++] = (uint8_t)(port & 0xFF);

    if (proxy_write_all_tcp(conn->tcp, req, (int)req_len, timeout_ms) != (int)req_len) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    uint8_t resp_head[4] = {0};
    if (proxy_read_exact_tcp(conn->tcp, resp_head, sizeof(resp_head), timeout_ms) != (int)sizeof(resp_head)) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    if (resp_head[0] != 0x05 || resp_head[1] != 0x00) {
        IM_LOGE(TAG, "SOCKS5 connect rejected ver=%u rep=%u", resp_head[0], resp_head[1]);
        return OPRT_COM_ERROR;
    }

    int addr_tail_len = 0;
    if (resp_head[3] == 0x01) {
        addr_tail_len = 4 + 2;
    } else if (resp_head[3] == 0x04) {
        addr_tail_len = 16 + 2;
    } else if (resp_head[3] == 0x03) {
        uint8_t name_len = 0;
        if (proxy_read_exact_tcp(conn->tcp, &name_len, 1, timeout_ms) != 1) {
            return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
        }
        addr_tail_len = name_len + 2;
    } else {
        IM_LOGE(TAG, "SOCKS5 unknown atyp=%u", resp_head[3]);
        return OPRT_COM_ERROR;
    }

    if (addr_tail_len > 0) {
        uint8_t tmp[300] = {0};
        if (addr_tail_len > (int)sizeof(tmp)) {
            return OPRT_BUFFER_NOT_ENOUGH;
        }
        if (proxy_read_exact_tcp(conn->tcp, tmp, addr_tail_len, timeout_ms) != addr_tail_len) {
            return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
        }
    }

    return OPRT_OK;
}

OPERATE_RET http_proxy_init(void)
{
    if (IM_SECRET_PROXY_HOST[0] != '\0') {
        snprintf(s_proxy_host, sizeof(s_proxy_host), "%s", IM_SECRET_PROXY_HOST);
    }

    if (IM_SECRET_PROXY_PORT[0] != '\0') {
        s_proxy_port = (uint16_t)atoi(IM_SECRET_PROXY_PORT);
    }

    if (proxy_type_valid(IM_SECRET_PROXY_TYPE)) {
        proxy_type_set(IM_SECRET_PROXY_TYPE);
    }

    char tmp[64] = {0};
    if (im_kv_get_string(IM_NVS_PROXY, IM_NVS_KEY_PROXY_HOST, tmp, sizeof(tmp)) == OPRT_OK) {
        snprintf(s_proxy_host, sizeof(s_proxy_host), "%s", tmp);
    }

    memset(tmp, 0, sizeof(tmp));
    if (im_kv_get_string(IM_NVS_PROXY, IM_NVS_KEY_PROXY_PORT, tmp, sizeof(tmp)) == OPRT_OK) {
        s_proxy_port = (uint16_t)atoi(tmp);
    }

    memset(tmp, 0, sizeof(tmp));
    if (im_kv_get_string(IM_NVS_PROXY, IM_NVS_KEY_PROXY_TYPE, tmp, sizeof(tmp)) == OPRT_OK) {
        if (proxy_type_valid(tmp)) {
            proxy_type_set(tmp);
        }
    }

    if (http_proxy_is_enabled()) {
        IM_LOGI(TAG, "proxy configured: %s:%u (%s)", s_proxy_host, s_proxy_port, s_proxy_type);
    } else {
        IM_LOGI(TAG, "proxy not configured");
    }

    return OPRT_OK;
}

bool http_proxy_is_enabled(void)
{
    return s_proxy_host[0] != '\0' && s_proxy_port > 0;
}

OPERATE_RET http_proxy_set(const char *host, uint16_t port, const char *type)
{
    if (!host || port == 0) {
        return OPRT_INVALID_PARM;
    }

    const char *proxy_type = type ? type : "http";
    if (!proxy_type_valid(proxy_type)) {
        return OPRT_INVALID_PARM;
    }

    snprintf(s_proxy_host, sizeof(s_proxy_host), "%s", host);
    s_proxy_port = port;
    proxy_type_set(proxy_type);

    char port_buf[16] = {0};
    snprintf(port_buf, sizeof(port_buf), "%u", (unsigned)port);

    OPERATE_RET rt = im_kv_set_string(IM_NVS_PROXY, IM_NVS_KEY_PROXY_HOST, host);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = im_kv_set_string(IM_NVS_PROXY, IM_NVS_KEY_PROXY_PORT, port_buf);
    if (rt != OPRT_OK) {
        return rt;
    }

    return im_kv_set_string(IM_NVS_PROXY, IM_NVS_KEY_PROXY_TYPE, proxy_type);
}

OPERATE_RET http_proxy_clear(void)
{
    s_proxy_host[0] = '\0';
    s_proxy_port    = 0;
    proxy_type_set("http");

    (void)im_kv_del(IM_NVS_PROXY, IM_NVS_KEY_PROXY_HOST);
    (void)im_kv_del(IM_NVS_PROXY, IM_NVS_KEY_PROXY_PORT);
    (void)im_kv_del(IM_NVS_PROXY, IM_NVS_KEY_PROXY_TYPE);
    return OPRT_OK;
}

proxy_conn_t *proxy_conn_open(const char *host, int port, int timeout_ms)
{
    if (!host || port <= 0 || timeout_ms <= 0) {
        return NULL;
    }
    if (!http_proxy_is_enabled()) {
        IM_LOGW(TAG, "proxy not configured");
        return NULL;
    }

    proxy_conn_t *conn = im_calloc(1, sizeof(proxy_conn_t));
    if (!conn) {
        return NULL;
    }

    OPERATE_RET rt = proxy_open_transport(conn, timeout_ms);
    if (rt != OPRT_OK) {
        im_free(conn);
        return NULL;
    }

    if (proxy_type_is_socks5()) {
        rt = open_socks5_tunnel(conn, host, port, timeout_ms);
    } else {
        rt = open_http_connect_tunnel(conn, host, port, timeout_ms);
    }

    if (rt != OPRT_OK) {
        IM_LOGE(TAG, "open %s tunnel failed host=%s:%d rt=%d", s_proxy_type, host, port, rt);
        tuya_transporter_close(conn->tcp);
        tuya_transporter_destroy(conn->tcp);
        im_free(conn);
        return NULL;
    }

    rt = tuya_transporter_ctrl(conn->tcp, TUYA_TRANSPORTER_GET_TCP_SOCKET, &conn->socket_fd);
    if (rt != OPRT_OK || conn->socket_fd < 0) {
        IM_LOGE(TAG, "get proxy socket failed rt=%d fd=%d", rt, conn->socket_fd);
        tuya_transporter_close(conn->tcp);
        tuya_transporter_destroy(conn->tcp);
        im_free(conn);
        return NULL;
    }

    uint8_t *cacert      = NULL;
    size_t   cacert_len  = 0;
    bool     verify_peer = false;
    rt                   = im_tls_query_domain_certs(host, &cacert, &cacert_len);
    if (rt == OPRT_OK && cacert && cacert_len > 0) {
        verify_peer = true;
    } else {
        IM_LOGW(TAG, "proxy tls cert unavailable for %s, fallback to no-verify mode rt=%d", host, rt);
    }
    if (verify_peer && cacert_len > (size_t)INT_MAX) {
        IM_LOGW(TAG, "proxy tls cert too large host=%s len=%zu, fallback to no-verify", host, cacert_len);
        verify_peer = false;
    }

    conn->tls = tuya_tls_connect_create();
    if (!conn->tls) {
        IM_LOGE(TAG, "create tls handler failed");
        if (cacert) {
            im_free(cacert);
        }
        tuya_transporter_close(conn->tcp);
        tuya_transporter_destroy(conn->tcp);
        im_free(conn);
        return NULL;
    }

    int timeout_s = timeout_ms / 1000;
    if (timeout_s <= 0) {
        timeout_s = 1;
    }

    tuya_tls_config_t cfg_tls = {
        .mode         = TUYA_TLS_SERVER_CERT_MODE,
        .hostname     = (char *)host,
        .port         = (uint32_t)port,
        .timeout      = timeout_s,
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
        IM_LOGE(TAG, "proxy tls connect failed host=%s rt=%d", host, rt);
        tuya_tls_connect_destroy(conn->tls);
        conn->tls = NULL;
        tuya_transporter_close(conn->tcp);
        tuya_transporter_destroy(conn->tcp);
        im_free(conn);
        return NULL;
    }

    IM_LOGI(TAG, "proxy tunnel ready %s:%d via %s:%u (%s)", host, port, s_proxy_host, s_proxy_port, s_proxy_type);
    return conn;
}

int proxy_conn_write(proxy_conn_t *conn, const char *data, int len)
{
    if (!conn || !conn->tls || !data || len <= 0) {
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

int proxy_conn_read(proxy_conn_t *conn, char *buf, int len, int timeout_ms)
{
    if (!conn || !conn->tls || conn->socket_fd < 0 || !buf || len <= 0 || timeout_ms <= 0) {
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

    int n = tuya_tls_read(conn->tls, (uint8_t *)buf, (uint32_t)len);
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

void proxy_conn_close(proxy_conn_t *conn)
{
    if (!conn) {
        return;
    }
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
    im_free(conn);
}
