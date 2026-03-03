#include "channels/feishu_bot.h"

#include "bus/message_bus.h"
#include "cJSON.h"
#include "http_client_interface.h"
#include "im_config.h"
#include "im_utils.h"
#include "proxy/http_proxy.h"
#include "certs/tls_cert_bundle.h"
#include "tuya_tls.h"
#include "tuya_transporter.h"

#include "mbedtls/ssl.h"

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>

static const char *TAG = "feishu";

static char          s_app_id[96]        = {0};
static char          s_app_secret[160]   = {0};
static char          s_allow_from[512]   = {0};
static char          s_tenant_token[512] = {0};
static uint32_t      s_tenant_expire_ms  = 0;
static uint8_t      *s_fs_cacert         = NULL;
static size_t        s_fs_cacert_len     = 0;
static THREAD_HANDLE s_ws_thread         = NULL;

#define FS_HOST                    IM_FS_API_HOST
#define FS_HTTP_TIMEOUT_MS         10000
#define FS_HTTP_RESP_BUF_SIZE      (16 * 1024)
#define FS_TOKEN_SAFETY_MARGIN_S   60
#define FS_WS_RX_BUF_SIZE          (64 * 1024)
#define FS_WS_DEFAULT_RECONNECT_MS 5000
#define FS_WS_DEFAULT_PING_MS      (120 * 1000)
/* Poll wait (ms) for receiving frames; smaller = lower latency, too small = more CPU spin */
#define FS_WS_POLL_WAIT_MS         150
#define FS_WS_FRAME_MAX_HEADERS    32
#define FS_WS_FRAME_MAX_KEY        64
#define FS_WS_FRAME_MAX_VALUE      256
#define FS_DEDUP_CACHE_SIZE        512
#define FS_MAX_FRAG_PARTS          8
#ifndef IM_FS_POLL_STACK
#define IM_FS_POLL_STACK (16 * 1024)
#endif

/* -------- allow_from -------- */

static char *trim_ws(char *s)
{
    if (!s) {
        return s;
    }
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }

    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) {
        *(--end) = '\0';
    }

    return s;
}

static char *strip_optional_quotes(char *s)
{
    if (!s) {
        return s;
    }

    size_t len = strlen(s);
    if (len >= 2) {
        char first = s[0];
        char last  = s[len - 1];
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            s[len - 1] = '\0';
            return s + 1;
        }
    }

    return s;
}

static bool sender_allowed_token(const char *allow_id, const char *sender_ids)
{
    if (!allow_id || allow_id[0] == '\0' || !sender_ids || sender_ids[0] == '\0') {
        return false;
    }

    if (strcmp(allow_id, sender_ids) == 0) {
        return true;
    }

    char sender_csv[384] = {0};
    im_safe_copy(sender_csv, sizeof(sender_csv), sender_ids);

    char *saveptr = NULL;
    char *tok     = strtok_r(sender_csv, "|", &saveptr);
    while (tok) {
        char *id = strip_optional_quotes(trim_ws(tok));
        if (id[0] != '\0' && strcmp(id, allow_id) == 0) {
            return true;
        }
        tok = strtok_r(NULL, "|", &saveptr);
    }

    return false;
}

static bool sender_allowed(const char *sender_ids)
{
    if (!sender_ids || sender_ids[0] == '\0') {
        return false;
    }

    if (s_allow_from[0] == '\0') {
        return true;
    }

    char csv[sizeof(s_allow_from)] = {0};
    im_safe_copy(csv, sizeof(csv), s_allow_from);

    char *saveptr = NULL;
    char *tok     = strtok_r(csv, ",", &saveptr);
    while (tok) {
        char *id = strip_optional_quotes(trim_ws(tok));
        if (id[0] != '\0' && sender_allowed_token(id, sender_ids)) {
            return true;
        }
        tok = strtok_r(NULL, ",", &saveptr);
    }

    return false;
}

static void append_sender_id(char *sender_ids, size_t sender_ids_size, const char *id)
{
    if (!sender_ids || sender_ids_size == 0 || !id || id[0] == '\0') {
        return;
    }

    size_t used = strlen(sender_ids);
    if (used >= sender_ids_size - 1) {
        return;
    }

    int n = snprintf(sender_ids + used, sender_ids_size - used, "%s%s", used ? "|" : "", id);
    if (n < 0) {
        sender_ids[used] = '\0';
    }
}

/* -------- dedup ring buffer -------- */

typedef struct {
    uint64_t keys[FS_DEDUP_CACHE_SIZE];
    size_t   idx;
} dedup_ring_t;

static dedup_ring_t s_seen_msg   = {0};
static dedup_ring_t s_seen_event = {0};

static bool dedup_contains(const dedup_ring_t *ring, uint64_t key)
{
    for (size_t i = 0; i < FS_DEDUP_CACHE_SIZE; i++) {
        if (ring->keys[i] == key) {
            return true;
        }
    }
    return false;
}

static void dedup_insert(dedup_ring_t *ring, uint64_t key)
{
    ring->keys[ring->idx] = key;
    ring->idx             = (ring->idx + 1) % FS_DEDUP_CACHE_SIZE;
}

/* -------- TLS + HTTP -------- */

static OPERATE_RET ensure_fs_cert(const char *host)
{
    if (!host || host[0] == '\0') {
        return OPRT_INVALID_PARM;
    }
    if (strcmp(host, FS_HOST) != 0) {
        return OPRT_OK;
    }
    if (s_fs_cacert && s_fs_cacert_len > 0) {
        return OPRT_OK;
    }

    im_free(s_fs_cacert);
    s_fs_cacert     = NULL;
    s_fs_cacert_len = 0;

    OPERATE_RET rt = im_tls_query_domain_certs(host, &s_fs_cacert, &s_fs_cacert_len);
    if (rt != OPRT_OK || !s_fs_cacert || s_fs_cacert_len == 0) {
        im_free(s_fs_cacert);
        s_fs_cacert     = NULL;
        s_fs_cacert_len = 0;
        IM_LOGW(TAG, "cert unavailable for %s, fallback to TLS no-verify mode rt=%d", host, rt);
    }
    return OPRT_OK;
}

static OPERATE_RET fs_http_call_via_proxy(const char *host, const char *path, const char *method, const char *body,
                                          const char *bearer_token, char *resp_buf, size_t resp_buf_size,
                                          uint16_t *status_code)
{
    proxy_conn_t *conn = proxy_conn_open(host, 443, FS_HTTP_TIMEOUT_MS);
    if (!conn) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    int  body_len       = body ? (int)strlen(body) : 0;
    char auth_line[700] = {0};
    if (bearer_token && bearer_token[0]) {
        snprintf(auth_line, sizeof(auth_line), "Authorization: Bearer %s\r\n", bearer_token);
    }

    char req_header[1400] = {0};
    int  req_len          = snprintf(req_header, sizeof(req_header),
                                     "%s %s HTTP/1.1\r\n"
                                               "Host: %s\r\n"
                                               "User-Agent: IM/1.0\r\n"
                                               "Content-Type: application/json\r\n"
                                               "Content-Length: %d\r\n"
                                               "%s"
                                               "Connection: close\r\n"
                                               "\r\n",
                                     method, path, host, body_len, auth_line);

    if (req_len <= 0 || req_len >= (int)sizeof(req_header)) {
        proxy_conn_close(conn);
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    if (proxy_conn_write(conn, req_header, req_len) != req_len) {
        proxy_conn_close(conn);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    if (body_len > 0 && proxy_conn_write(conn, body, body_len) != body_len) {
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

        int n = proxy_conn_read(conn, raw + raw_len, (int)(raw_cap - raw_len - 1), 1000);
        if (n == OPRT_RESOURCE_NOT_READY) {
            if ((int)(tal_system_get_millisecond() - wait_begin_ms) >= 15000) {
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

    resp_buf[0]    = '\0';
    char *body_ptr = strstr(raw, "\r\n\r\n");
    if (body_ptr) {
        body_ptr += 4;
        size_t copy = strlen(body_ptr);
        if (copy > resp_buf_size - 1) {
            copy = resp_buf_size - 1;
        }
        memcpy(resp_buf, body_ptr, copy);
        resp_buf[copy] = '\0';
    }

    im_free(raw);
    return OPRT_OK;
}

static OPERATE_RET fs_http_call_direct(const char *host, const char *path, const char *method, const char *body,
                                       const char *bearer_token, char *resp_buf, size_t resp_buf_size,
                                       uint16_t *status_code)
{
    OPERATE_RET rt = ensure_fs_cert(host);
    if (rt != OPRT_OK) {
        return rt;
    }

    http_client_header_t headers[3]   = {0};
    uint8_t              header_count = 0;
    headers[header_count++]           = (http_client_header_t){.key = "Content-Type", .value = "application/json"};

    char auth[640] = {0};
    if (bearer_token && bearer_token[0] != '\0') {
        snprintf(auth, sizeof(auth), "Bearer %s", bearer_token);
        headers[header_count++] = (http_client_header_t){.key = "Authorization", .value = auth};
    }

    const uint8_t *body_ptr = (const uint8_t *)(body ? body : "");
    size_t         body_len = body ? strlen(body) : 0;

    const uint8_t *cacert     = NULL;
    size_t         cacert_len = 0;
    if (strcmp(host, FS_HOST) == 0) {
        cacert     = s_fs_cacert;
        cacert_len = s_fs_cacert_len;
    }
    bool tls_no_verify = (cacert == NULL || cacert_len == 0);

    http_client_response_t response = {0};
    http_client_status_t   http_rt  = http_client_request(
        &(const http_client_request_t){
               .cacert        = cacert,
               .cacert_len    = cacert_len,
               .tls_no_verify = tls_no_verify,
               .host          = host,
               .port          = 443,
               .method        = method,
               .path          = path,
               .headers       = headers,
               .headers_count = header_count,
               .body          = body_ptr,
               .body_length   = body_len,
               .timeout_ms    = FS_HTTP_TIMEOUT_MS,
        },
        &response);
    if (http_rt != HTTP_CLIENT_SUCCESS) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    if (status_code) {
        *status_code = response.status_code;
    }

    resp_buf[0] = '\0';
    if (response.body && response.body_length > 0) {
        size_t copy = (response.body_length < resp_buf_size - 1) ? response.body_length : (resp_buf_size - 1);
        memcpy(resp_buf, response.body, copy);
        resp_buf[copy] = '\0';
    }

    http_client_free(&response);
    return OPRT_OK;
}

static OPERATE_RET fs_http_call(const char *host, const char *path, const char *method, const char *body,
                                const char *bearer_token, char *resp_buf, size_t resp_buf_size, uint16_t *status_code)
{
    if (!host || !path || !method || !resp_buf || resp_buf_size == 0) {
        return OPRT_INVALID_PARM;
    }

    if (http_proxy_is_enabled()) {
        return fs_http_call_via_proxy(host, path, method, body, bearer_token, resp_buf, resp_buf_size, status_code);
    }

    return fs_http_call_direct(host, path, method, body, bearer_token, resp_buf, resp_buf_size, status_code);
}

static bool fs_response_ok(const char *json_str, const char **out_msg)
{
    static char s_last_err_msg[128] = {0};
    s_last_err_msg[0]               = '\0';

    if (out_msg) {
        *out_msg = NULL;
    }
    if (!json_str || json_str[0] == '\0') {
        return false;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return false;
    }

    bool   ok   = false;
    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (cJSON_IsNumber(code) && (int)code->valuedouble == 0) {
        ok = true;
    }

    if (!ok && out_msg) {
        cJSON *msg = cJSON_GetObjectItem(root, "msg");
        if (cJSON_IsString(msg) && msg->valuestring) {
            im_safe_copy(s_last_err_msg, sizeof(s_last_err_msg), msg->valuestring);
            *out_msg = s_last_err_msg;
        }
    }

    cJSON_Delete(root);
    return ok;
}

/* -------- tenant token -------- */

static bool tenant_token_valid(void)
{
    if (s_tenant_token[0] == '\0' || s_tenant_expire_ms == 0) {
        return false;
    }
    return ((int32_t)(s_tenant_expire_ms - tal_system_get_millisecond()) > 0);
}

static OPERATE_RET refresh_tenant_token(void)
{
    if (s_app_id[0] == '\0' || s_app_secret[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    cJSON *body = cJSON_CreateObject();
    if (!body) {
        return OPRT_MALLOC_FAILED;
    }
    cJSON_AddStringToObject(body, "app_id", s_app_id);
    cJSON_AddStringToObject(body, "app_secret", s_app_secret);
    char *json = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!json) {
        return OPRT_MALLOC_FAILED;
    }

    char *resp = im_malloc(FS_HTTP_RESP_BUF_SIZE);
    if (!resp) {
        cJSON_free(json);
        return OPRT_MALLOC_FAILED;
    }
    memset(resp, 0, FS_HTTP_RESP_BUF_SIZE);

    uint16_t    status = 0;
    OPERATE_RET rt = fs_http_call(FS_HOST, "/open-apis/auth/v3/tenant_access_token/internal", "POST", json, NULL, resp,
                                  FS_HTTP_RESP_BUF_SIZE, &status);
    cJSON_free(json);
    if (rt != OPRT_OK || status != 200) {
        im_free(resp);
        return OPRT_COM_ERROR;
    }

    cJSON *root = cJSON_Parse(resp);
    if (!root) {
        im_free(resp);
        return OPRT_CR_CJSON_ERR;
    }

    OPERATE_RET out    = OPRT_COM_ERROR;
    cJSON      *code   = cJSON_GetObjectItem(root, "code");
    cJSON      *token  = cJSON_GetObjectItem(root, "tenant_access_token");
    cJSON      *expire = cJSON_GetObjectItem(root, "expire");
    if (cJSON_IsNumber(code) && (int)code->valuedouble == 0 && cJSON_IsString(token) && token->valuestring &&
        cJSON_IsNumber(expire) && expire->valuedouble > 0) {
        uint32_t expire_s = (uint32_t)expire->valuedouble;
        if (expire_s > FS_TOKEN_SAFETY_MARGIN_S) {
            expire_s -= FS_TOKEN_SAFETY_MARGIN_S;
        }
        im_safe_copy(s_tenant_token, sizeof(s_tenant_token), token->valuestring);
        s_tenant_expire_ms = tal_system_get_millisecond() + expire_s * 1000u;
        out                = OPRT_OK;
    }

    cJSON_Delete(root);
    im_free(resp);
    return out;
}

static OPERATE_RET ensure_tenant_token(void)
{
    if (tenant_token_valid()) {
        return OPRT_OK;
    }
    return refresh_tenant_token();
}

/* -------- ws endpoint -------- */

typedef struct {
    char     url[768];
    int      reconnect_count;
    uint32_t reconnect_interval_ms;
    uint32_t reconnect_nonce_ms;
    uint32_t ping_interval_ms;
} fs_ws_conf_t;

static int json_int2(cJSON *obj, const char *k1, const char *k2, int defv)
{
    cJSON *item = cJSON_GetObjectItem(obj, k1);
    if (!item && k2) {
        item = cJSON_GetObjectItem(obj, k2);
    }
    if (!cJSON_IsNumber(item)) {
        return defv;
    }
    return (int)item->valuedouble;
}

static const char *json_str2(cJSON *obj, const char *k1, const char *k2)
{
    const char *v = im_json_str(obj, k1, NULL);
    return v ? v : im_json_str(obj, k2, NULL);
}

static OPERATE_RET fs_fetch_ws_conf(fs_ws_conf_t *conf)
{
    if (!conf) {
        return OPRT_INVALID_PARM;
    }
    memset(conf, 0, sizeof(*conf));
    conf->reconnect_count       = -1;
    conf->reconnect_interval_ms = FS_WS_DEFAULT_RECONNECT_MS;
    conf->reconnect_nonce_ms    = 30000;
    conf->ping_interval_ms      = FS_WS_DEFAULT_PING_MS;

    cJSON *body = cJSON_CreateObject();
    if (!body) {
        return OPRT_MALLOC_FAILED;
    }
    cJSON_AddStringToObject(body, "AppID", s_app_id);
    cJSON_AddStringToObject(body, "AppSecret", s_app_secret);
    char *json = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!json) {
        return OPRT_MALLOC_FAILED;
    }

    char *resp = im_malloc(FS_HTTP_RESP_BUF_SIZE);
    if (!resp) {
        cJSON_free(json);
        return OPRT_MALLOC_FAILED;
    }
    memset(resp, 0, FS_HTTP_RESP_BUF_SIZE);

    uint16_t    status = 0;
    OPERATE_RET rt =
        fs_http_call(FS_HOST, "/callback/ws/endpoint", "POST", json, NULL, resp, FS_HTTP_RESP_BUF_SIZE, &status);
    cJSON_free(json);
    if (rt != OPRT_OK || status != 200) {
        im_free(resp);
        return OPRT_COM_ERROR;
    }

    cJSON *root = cJSON_Parse(resp);
    im_free(resp);
    if (!root) {
        return OPRT_CR_CJSON_ERR;
    }

    OPERATE_RET out  = OPRT_COM_ERROR;
    cJSON      *code = cJSON_GetObjectItem(root, "code");
    cJSON      *data = cJSON_GetObjectItem(root, "data");
    if (cJSON_IsNumber(code) && (int)code->valuedouble == 0 && cJSON_IsObject(data)) {
        const char *url = json_str2(data, "URL", "url");
        if (url && url[0] != '\0') {
            im_safe_copy(conf->url, sizeof(conf->url), url);

            cJSON *cc = cJSON_GetObjectItem(data, "ClientConfig");
            if (!cc) {
                cc = cJSON_GetObjectItem(data, "client_config");
            }
            if (cJSON_IsObject(cc)) {
                int rc = json_int2(cc, "ReconnectCount", "reconnect_count", conf->reconnect_count);
                int ri =
                    json_int2(cc, "ReconnectInterval", "reconnect_interval", (int)(conf->reconnect_interval_ms / 1000));
                int rn = json_int2(cc, "ReconnectNonce", "reconnect_nonce", (int)(conf->reconnect_nonce_ms / 1000));
                int pi = json_int2(cc, "PingInterval", "ping_interval", (int)(conf->ping_interval_ms / 1000));

                conf->reconnect_count = rc;
                if (ri > 0) {
                    conf->reconnect_interval_ms = (uint32_t)ri * 1000u;
                }
                if (rn > 0) {
                    conf->reconnect_nonce_ms = (uint32_t)rn * 1000u;
                }
                if (pi > 0) {
                    conf->ping_interval_ms = (uint32_t)pi * 1000u;
                }
            }
            out = OPRT_OK;
        }
    }

    cJSON_Delete(root);
    return out;
}

static int fs_query_param_int(const char *path, const char *key, int defv)
{
    if (!path || !key || key[0] == '\0') {
        return defv;
    }

    char pattern[64] = {0};
    snprintf(pattern, sizeof(pattern), "%s=", key);
    const char *p = strstr(path, pattern);
    if (!p) {
        return defv;
    }
    p += strlen(pattern);
    return atoi(p);
}

static OPERATE_RET fs_parse_ws_url(const char *url, char *host, size_t host_size, uint16_t *port, char *path,
                                   size_t path_size, int *service_id)
{
    if (!url || !host || host_size == 0 || !port || !path || path_size == 0 || !service_id) {
        return OPRT_INVALID_PARM;
    }

    host[0]     = '\0';
    path[0]     = '\0';
    *service_id = 0;

    const char *p            = NULL;
    uint16_t    default_port = 443;

    if (strncmp(url, "wss://", 6) == 0) {
        p            = url + 6;
        default_port = 443;
    } else if (strncmp(url, "ws://", 5) == 0) {
        p            = url + 5;
        default_port = 80;
    } else {
        return OPRT_INVALID_PARM;
    }

    const char *host_begin = p;
    while (*p && *p != '/' && *p != '?') {
        p++;
    }

    const char *host_end = p;
    const char *colon    = NULL;
    for (const char *q = host_begin; q < host_end; q++) {
        if (*q == ':') {
            colon = q;
            break;
        }
    }

    if (colon) {
        size_t host_len = (size_t)(colon - host_begin);
        if (host_len == 0 || host_len >= host_size) {
            return OPRT_BUFFER_NOT_ENOUGH;
        }
        memcpy(host, host_begin, host_len);
        host[host_len] = '\0';

        int parsed_port = atoi(colon + 1);
        if (parsed_port <= 0 || parsed_port > 65535) {
            return OPRT_INVALID_PARM;
        }
        *port = (uint16_t)parsed_port;
    } else {
        size_t host_len = (size_t)(host_end - host_begin);
        if (host_len == 0 || host_len >= host_size) {
            return OPRT_BUFFER_NOT_ENOUGH;
        }
        memcpy(host, host_begin, host_len);
        host[host_len] = '\0';
        *port          = default_port;
    }

    if (*p == '\0') {
        im_safe_copy(path, path_size, "/");
    } else {
        im_safe_copy(path, path_size, p);
    }

    *service_id = fs_query_param_int(path, "service_id", 0);
    return OPRT_OK;
}

/* -------- ws low-level conn -------- */

typedef enum {
    FS_CONN_NONE = 0,
    FS_CONN_PROXY,
    FS_CONN_DIRECT,
} fs_conn_mode_t;

typedef struct {
    fs_conn_mode_t     mode;
    proxy_conn_t      *proxy;
    tuya_transporter_t tcp;
    tuya_tls_hander    tls;
    int                socket_fd;
    uint8_t            rx_buf[FS_WS_RX_BUF_SIZE];
    size_t             rx_len;
} fs_ws_conn_t;

static OPERATE_RET fs_direct_open(fs_ws_conn_t *conn, const char *host, int port, int timeout_ms)
{
    if (!conn || !host || port <= 0 || timeout_ms <= 0) {
        return OPRT_INVALID_PARM;
    }

    conn->tcp = tuya_transporter_create(TRANSPORT_TYPE_TCP, NULL);
    if (!conn->tcp) {
        return OPRT_COM_ERROR;
    }
    conn->mode = FS_CONN_DIRECT;

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
        IM_LOGW(TAG, "ws cert unavailable for %s, fallback to no-verify mode rt=%d", host, rt);
    }
    if (verify_peer && cacert_len > (size_t)INT_MAX) {
        IM_LOGW(TAG, "ws cert too large for tuya_tls host=%s len=%zu, fallback to no-verify", host, cacert_len);
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

    return OPRT_OK;
}

static void fs_conn_close(fs_ws_conn_t *conn)
{
    if (!conn) {
        return;
    }

    if (conn->mode == FS_CONN_PROXY) {
        if (conn->proxy) {
            proxy_conn_close(conn->proxy);
            conn->proxy = NULL;
        }
    } else if (conn->mode == FS_CONN_DIRECT) {
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

    conn->mode   = FS_CONN_NONE;
    conn->rx_len = 0;
}

static OPERATE_RET fs_conn_open(fs_ws_conn_t *conn, const char *host, int port, int timeout_ms)
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
        conn->mode = FS_CONN_PROXY;
        return OPRT_OK;
    }

    OPERATE_RET rt = fs_direct_open(conn, host, port, timeout_ms);
    if (rt != OPRT_OK) {
        fs_conn_close(conn);
        return rt;
    }

    return OPRT_OK;
}

static int fs_conn_write(fs_ws_conn_t *conn, const uint8_t *data, int len)
{
    if (!conn || !data || len <= 0) {
        return -1;
    }

    if (conn->mode == FS_CONN_PROXY) {
        return proxy_conn_write(conn->proxy, (const char *)data, len);
    }

    if (conn->mode != FS_CONN_DIRECT || !conn->tls) {
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

/*
 * Layout-compatible prefix of the internal tuya_mbedtls_context_t (tuya_tls.c)
 * so we can call mbedtls_ssl_get_bytes_avail() to detect data already
 * decrypted but not yet consumed by the application.
 */
typedef struct {
    tuya_tls_config_t   _cfg;
    mbedtls_ssl_context ssl_ctx;
} fs_tls_compat_t;

static size_t fs_tls_bytes_avail(tuya_tls_hander tls)
{
    if (!tls) {
        return 0;
    }
    return mbedtls_ssl_get_bytes_avail(&((fs_tls_compat_t *)tls)->ssl_ctx);
}

static int fs_conn_read(fs_ws_conn_t *conn, uint8_t *buf, int len, int timeout_ms)
{
    if (!conn || !buf || len <= 0 || timeout_ms <= 0) {
        return -1;
    }

    if (conn->mode == FS_CONN_PROXY) {
        return proxy_conn_read(conn->proxy, (char *)buf, len, timeout_ms);
    }

    if (conn->mode != FS_CONN_DIRECT || !conn->tls || conn->socket_fd < 0) {
        return -1;
    }

    /*
     * mbedtls decrypts a full TLS record at once but may return only part of
     * it to the caller; the remainder stays in the SSL context's internal
     * buffer.  select() only monitors the raw TCP socket and is blind to
     * that buffered plaintext, so it would block until the *next* TCP
     * segment arrives — causing multi-second stalls.
     *
     * Fix: skip select() when mbedtls already has data ready.
     */
    if (fs_tls_bytes_avail(conn->tls) == 0) {
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

/* -------- ws frame codec -------- */

static OPERATE_RET fs_ws_send_frame(fs_ws_conn_t *conn, uint8_t opcode, const uint8_t *payload, size_t payload_len)
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

    int n = fs_conn_write(conn, frame, (int)frame_len);
    im_free(frame);

    return (n == (int)frame_len) ? OPRT_OK : OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
}

static OPERATE_RET fs_ws_handshake(fs_ws_conn_t *conn, const char *host, const char *path)
{
    if (!conn || !host || !path) {
        return OPRT_INVALID_PARM;
    }

    const char *ws_key    = "dGhlIHNhbXBsZSBub25jZQ==";
    char        req[1024] = {0};
    int         req_len   = snprintf(req, sizeof(req),
                                     "GET %s HTTP/1.1\r\n"
                                               "Host: %s\r\n"
                                               "Upgrade: websocket\r\n"
                                               "Connection: Upgrade\r\n"
                                               "Sec-WebSocket-Key: %s\r\n"
                                               "Sec-WebSocket-Version: 13\r\n"
                                               "User-Agent: IM/1.0\r\n\r\n",
                                     path, host, ws_key);
    if (req_len <= 0 || req_len >= (int)sizeof(req)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    if (fs_conn_write(conn, (const uint8_t *)req, req_len) != req_len) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    char     header[4096] = {0};
    int      total        = 0;
    int      header_end   = -1;
    uint32_t start_ms     = tal_system_get_millisecond();

    while ((int)(tal_system_get_millisecond() - start_ms) < FS_HTTP_TIMEOUT_MS && total < (int)sizeof(header) - 1) {
        int n = fs_conn_read(conn, (uint8_t *)header + total, (int)sizeof(header) - total - 1, 1000);
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
        IM_LOGE(TAG, "feishu ws handshake failed http=%u", status);
        return OPRT_COM_ERROR;
    }

    size_t remain = (size_t)(total - header_end);
    conn->rx_len  = 0;
    if (remain > 0) {
        if (remain > sizeof(conn->rx_buf)) {
            return OPRT_BUFFER_NOT_ENOUGH;
        }
        memcpy(conn->rx_buf, header + header_end, remain);
        conn->rx_len = remain;
    }

    IM_LOGI(TAG, "feishu ws handshake success!");
    return OPRT_OK;
}

static OPERATE_RET fs_ws_decode_one_frame(fs_ws_conn_t *conn, uint8_t *opcode, uint8_t **payload, size_t *payload_len,
                                          size_t *consumed)
{
    if (!conn || !opcode || !payload || !payload_len || !consumed) {
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

    if (plen > (uint64_t)(FS_WS_RX_BUF_SIZE - 16)) {
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

static void fs_ws_consume_rx(fs_ws_conn_t *conn, size_t consumed)
{
    if (!conn || consumed == 0 || consumed > conn->rx_len) {
        return;
    }

    if (consumed < conn->rx_len) {
        memmove(conn->rx_buf, conn->rx_buf + consumed, conn->rx_len - consumed);
    }
    conn->rx_len -= consumed;
}

static OPERATE_RET fs_ws_poll_frame(fs_ws_conn_t *conn, int wait_ms, uint8_t *opcode, uint8_t **payload,
                                    size_t *payload_len)
{
    if (!conn || !opcode || !payload || !payload_len) {
        return OPRT_INVALID_PARM;
    }

    *payload     = NULL;
    *payload_len = 0;

    size_t      consumed = 0;
    OPERATE_RET rt       = fs_ws_decode_one_frame(conn, opcode, payload, payload_len, &consumed);
    if (rt == OPRT_OK) {
        fs_ws_consume_rx(conn, consumed);
        return OPRT_OK;
    }
    if (rt != OPRT_RESOURCE_NOT_READY) {
        return rt;
    }

    uint8_t tmp[1024] = {0};
    int     n         = fs_conn_read(conn, tmp, sizeof(tmp), wait_ms);
    if (n == OPRT_RESOURCE_NOT_READY) {
        return OPRT_RESOURCE_NOT_READY;
    }
    if (n <= 0) {
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    if (conn->rx_len + (size_t)n > sizeof(conn->rx_buf)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    memcpy(conn->rx_buf + conn->rx_len, tmp, (size_t)n);
    conn->rx_len += (size_t)n;

    consumed = 0;
    rt       = fs_ws_decode_one_frame(conn, opcode, payload, payload_len, &consumed);
    if (rt == OPRT_OK) {
        fs_ws_consume_rx(conn, consumed);
    }
    return rt;
}

/* -------- protobuf frame codec -------- */

typedef struct {
    char key[FS_WS_FRAME_MAX_KEY];
    char value[FS_WS_FRAME_MAX_VALUE];
} fs_pb_header_t;

typedef struct {
    uint64_t       seq_id;
    uint64_t       log_id;
    int32_t        service;
    int32_t        method;
    fs_pb_header_t headers[FS_WS_FRAME_MAX_HEADERS];
    size_t         header_count;
    uint8_t       *payload;
    size_t         payload_len;
} fs_pb_frame_t;

typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
} fs_buf_t;

static void fs_pb_frame_init(fs_pb_frame_t *f)
{
    if (!f) {
        return;
    }
    memset(f, 0, sizeof(*f));
}

static void fs_pb_frame_free(fs_pb_frame_t *f)
{
    if (!f) {
        return;
    }
    im_free(f->payload);
    f->payload     = NULL;
    f->payload_len = 0;
}

static fs_pb_frame_t *fs_pb_frame_new(void)
{
    return im_calloc(1, sizeof(fs_pb_frame_t));
}

static void fs_pb_frame_delete(fs_pb_frame_t *f)
{
    if (!f) {
        return;
    }
    fs_pb_frame_free(f);
    im_free(f);
}

static bool pb_read_varint(const uint8_t *buf, size_t len, size_t *off, uint64_t *out)
{
    if (!buf || !off || !out) {
        return false;
    }

    uint64_t value = 0;
    int      shift = 0;
    while (*off < len && shift <= 63) {
        uint8_t b = buf[(*off)++];
        value |= ((uint64_t)(b & 0x7F) << shift);
        if ((b & 0x80) == 0) {
            *out = value;
            return true;
        }
        shift += 7;
    }

    return false;
}

static bool pb_skip_field(const uint8_t *buf, size_t len, size_t *off, uint8_t wire)
{
    if (!buf || !off) {
        return false;
    }

    uint64_t v = 0;
    switch (wire) {
    case 0:
        return pb_read_varint(buf, len, off, &v);
    case 1:
        if (*off + 8 > len) {
            return false;
        }
        *off += 8;
        return true;
    case 2:
        if (!pb_read_varint(buf, len, off, &v)) {
            return false;
        }
        if (v > len || *off > len - (size_t)v) {
            return false;
        }
        *off += (size_t)v;
        return true;
    case 5:
        if (*off + 4 > len) {
            return false;
        }
        *off += 4;
        return true;
    default:
        return false;
    }
}

static bool fs_pb_parse_header(const uint8_t *buf, size_t len, fs_pb_header_t *hdr)
{
    if (!buf || !hdr) {
        return false;
    }

    hdr->key[0]   = '\0';
    hdr->value[0] = '\0';

    size_t off = 0;
    while (off < len) {
        uint64_t tag = 0;
        if (!pb_read_varint(buf, len, &off, &tag)) {
            return false;
        }

        uint32_t field = (uint32_t)(tag >> 3);
        uint8_t  wire  = (uint8_t)(tag & 0x07);

        if (wire == 2 && (field == 1 || field == 2)) {
            uint64_t slen = 0;
            if (!pb_read_varint(buf, len, &off, &slen)) {
                return false;
            }
            if (slen > len || off > len - (size_t)slen) {
                return false;
            }

            size_t copy = (size_t)slen;
            if (field == 1) {
                if (copy > sizeof(hdr->key) - 1) {
                    copy = sizeof(hdr->key) - 1;
                }
                memcpy(hdr->key, buf + off, copy);
                hdr->key[copy] = '\0';
            } else {
                if (copy > sizeof(hdr->value) - 1) {
                    copy = sizeof(hdr->value) - 1;
                }
                memcpy(hdr->value, buf + off, copy);
                hdr->value[copy] = '\0';
            }

            off += (size_t)slen;
        } else {
            if (!pb_skip_field(buf, len, &off, wire)) {
                return false;
            }
        }
    }

    return true;
}

static bool fs_pb_parse_frame(const uint8_t *buf, size_t len, fs_pb_frame_t *frame)
{
    if (!buf || !frame) {
        return false;
    }

    fs_pb_frame_init(frame);

    size_t off = 0;
    while (off < len) {
        uint64_t tag = 0;
        if (!pb_read_varint(buf, len, &off, &tag)) {
            goto __FAIL;
        }

        uint32_t field = (uint32_t)(tag >> 3);
        uint8_t  wire  = (uint8_t)(tag & 0x07);
        uint64_t v     = 0;

        switch (field) {
        case 1: /* seq_id */
        case 2: /* log_id */
        case 3: /* service */
        case 4: /* method */
            if (wire != 0 || !pb_read_varint(buf, len, &off, &v)) {
                goto __FAIL;
            }
            if (field == 1)      frame->seq_id  = v;
            else if (field == 2) frame->log_id  = v;
            else if (field == 3) frame->service = (int32_t)v;
            else                 frame->method  = (int32_t)v;
            break;
        case 5: { /* header */
            if (wire != 2) {
                goto __FAIL;
            }
            uint64_t mlen = 0;
            if (!pb_read_varint(buf, len, &off, &mlen) || mlen > len || off > len - (size_t)mlen) {
                goto __FAIL;
            }
            if (frame->header_count < FS_WS_FRAME_MAX_HEADERS) {
                (void)fs_pb_parse_header(buf + off, (size_t)mlen, &frame->headers[frame->header_count]);
                frame->header_count++;
            }
            off += (size_t)mlen;
            break;
        }
        case 8: { /* payload */
            if (wire != 2) {
                goto __FAIL;
            }
            uint64_t blen = 0;
            if (!pb_read_varint(buf, len, &off, &blen) || blen > len || off > len - (size_t)blen) {
                goto __FAIL;
            }
            im_free(frame->payload);
            frame->payload     = NULL;
            frame->payload_len = 0;
            if (blen > 0) {
                frame->payload = im_malloc((size_t)blen + 1);
                if (!frame->payload) {
                    goto __FAIL;
                }
                memcpy(frame->payload, buf + off, (size_t)blen);
                frame->payload[blen] = '\0';
                frame->payload_len   = (size_t)blen;
            }
            off += (size_t)blen;
            break;
        }
        default:
            if (!pb_skip_field(buf, len, &off, wire)) {
                goto __FAIL;
            }
            break;
        }
    }
    return true;

__FAIL:
    fs_pb_frame_free(frame);
    return false;
}

static bool fs_buf_reserve(fs_buf_t *b, size_t need)
{
    if (!b) {
        return false;
    }
    if (b->len + need <= b->cap) {
        return true;
    }

    size_t new_cap = b->cap ? b->cap : 128;
    while (new_cap < b->len + need) {
        new_cap *= 2;
    }

    uint8_t *p = im_realloc(b->data, new_cap);
    if (!p) {
        return false;
    }

    b->data = p;
    b->cap  = new_cap;
    return true;
}

static bool fs_buf_append(fs_buf_t *b, const uint8_t *data, size_t n)
{
    if (!b || (n > 0 && !data)) {
        return false;
    }
    if (!fs_buf_reserve(b, n)) {
        return false;
    }

    if (n > 0) {
        memcpy(b->data + b->len, data, n);
    }
    b->len += n;
    return true;
}

static bool pb_write_varint(fs_buf_t *b, uint64_t v)
{
    uint8_t tmp[10] = {0};
    size_t  n       = 0;
    do {
        uint8_t c = (uint8_t)(v & 0x7F);
        v >>= 7;
        if (v) {
            c |= 0x80;
        }
        tmp[n++] = c;
    } while (v && n < sizeof(tmp));

    return fs_buf_append(b, tmp, n);
}

static bool pb_write_key(fs_buf_t *b, uint32_t field, uint8_t wire)
{
    return pb_write_varint(b, ((uint64_t)field << 3) | wire);
}

static bool pb_write_bytes_field(fs_buf_t *b, uint32_t field, const uint8_t *data, size_t n)
{
    if (!pb_write_key(b, field, 2)) {
        return false;
    }
    if (!pb_write_varint(b, (uint64_t)n)) {
        return false;
    }
    return fs_buf_append(b, data, n);
}

static bool pb_write_str_field(fs_buf_t *b, uint32_t field, const char *s)
{
    if (!s) {
        s = "";
    }
    return pb_write_bytes_field(b, field, (const uint8_t *)s, strlen(s));
}

static bool fs_pb_encode_frame(const fs_pb_frame_t *frame, uint8_t **out, size_t *out_len)
{
    if (!frame || !out || !out_len) {
        return false;
    }

    fs_buf_t b = {0};

    if (!pb_write_key(&b, 1, 0) || !pb_write_varint(&b, frame->seq_id)) {
        goto __FAIL;
    }
    if (!pb_write_key(&b, 2, 0) || !pb_write_varint(&b, frame->log_id)) {
        goto __FAIL;
    }
    if (!pb_write_key(&b, 3, 0) || !pb_write_varint(&b, (uint64_t)frame->service)) {
        goto __FAIL;
    }
    if (!pb_write_key(&b, 4, 0) || !pb_write_varint(&b, (uint64_t)frame->method)) {
        goto __FAIL;
    }

    for (size_t i = 0; i < frame->header_count; i++) {
        fs_buf_t hb = {0};
        if (!pb_write_str_field(&hb, 1, frame->headers[i].key) ||
            !pb_write_str_field(&hb, 2, frame->headers[i].value)) {
            im_free(hb.data);
            goto __FAIL;
        }

        if (!pb_write_key(&b, 5, 2) || !pb_write_varint(&b, (uint64_t)hb.len) || !fs_buf_append(&b, hb.data, hb.len)) {
            im_free(hb.data);
            goto __FAIL;
        }

        im_free(hb.data);
    }

    if (frame->payload && frame->payload_len > 0) {
        if (!pb_write_bytes_field(&b, 8, frame->payload, frame->payload_len)) {
            goto __FAIL;
        }
    }

    *out     = b.data;
    *out_len = b.len;
    return true;

__FAIL:
    im_free(b.data);
    return false;
}

static const char *fs_pb_get_header(const fs_pb_frame_t *frame, const char *key)
{
    if (!frame || !key) {
        return NULL;
    }

    for (size_t i = 0; i < frame->header_count; i++) {
        if (strcmp(frame->headers[i].key, key) == 0) {
            return frame->headers[i].value;
        }
    }
    return NULL;
}

/* -------- payload split combine -------- */

typedef struct {
    bool     active;
    char     message_id[96];
    uint32_t sum;
    bool     got[FS_MAX_FRAG_PARTS];
    uint8_t *parts[FS_MAX_FRAG_PARTS];
    size_t   lens[FS_MAX_FRAG_PARTS];
    uint32_t expire_ms;
} fs_frag_state_t;

static fs_frag_state_t s_frag = {0};

static void fs_frag_clear(void)
{
    for (size_t i = 0; i < FS_MAX_FRAG_PARTS; i++) {
        im_free(s_frag.parts[i]);
        s_frag.parts[i] = NULL;
        s_frag.lens[i]  = 0;
        s_frag.got[i]   = false;
    }
    s_frag.active        = false;
    s_frag.message_id[0] = '\0';
    s_frag.sum           = 0;
    s_frag.expire_ms     = 0;
}

static OPERATE_RET fs_frag_merge(const char *message_id, uint32_t sum, uint32_t seq, const uint8_t *payload,
                                 size_t payload_len, uint8_t **out_payload, size_t *out_len)
{
    if (!out_payload || !out_len || !payload) {
        return OPRT_INVALID_PARM;
    }

    *out_payload = NULL;
    *out_len     = 0;

    if (sum <= 1) {
        uint8_t *copy = im_malloc(payload_len + 1);
        if (!copy) {
            return OPRT_MALLOC_FAILED;
        }
        memcpy(copy, payload, payload_len);
        copy[payload_len] = '\0';
        *out_payload      = copy;
        *out_len          = payload_len;
        return OPRT_OK;
    }

    if (!message_id || message_id[0] == '\0' || sum > FS_MAX_FRAG_PARTS || seq >= sum) {
        return OPRT_INVALID_PARM;
    }

    uint32_t now = tal_system_get_millisecond();
    if (!s_frag.active || strcmp(s_frag.message_id, message_id) != 0 || s_frag.sum != sum ||
        (int32_t)(now - s_frag.expire_ms) >= 0) {
        fs_frag_clear();
        s_frag.active = true;
        im_safe_copy(s_frag.message_id, sizeof(s_frag.message_id), message_id);
        s_frag.sum = sum;
    }

    if (!s_frag.got[seq]) {
        uint8_t *part = im_malloc(payload_len + 1);
        if (!part) {
            return OPRT_MALLOC_FAILED;
        }
        memcpy(part, payload, payload_len);
        part[payload_len] = '\0';

        s_frag.parts[seq] = part;
        s_frag.lens[seq]  = payload_len;
        s_frag.got[seq]   = true;
    }
    s_frag.expire_ms = now + 5000;

    for (uint32_t i = 0; i < sum; i++) {
        if (!s_frag.got[i]) {
            return OPRT_RESOURCE_NOT_READY;
        }
    }

    size_t total = 0;
    for (uint32_t i = 0; i < sum; i++) {
        total += s_frag.lens[i];
    }

    uint8_t *merged = im_malloc(total + 1);
    if (!merged) {
        fs_frag_clear();
        return OPRT_MALLOC_FAILED;
    }

    size_t off = 0;
    for (uint32_t i = 0; i < sum; i++) {
        memcpy(merged + off, s_frag.parts[i], s_frag.lens[i]);
        off += s_frag.lens[i];
    }
    merged[total] = '\0';

    fs_frag_clear();
    *out_payload = merged;
    *out_len     = total;
    return OPRT_OK;
}

/* -------- message parsing -------- */

static void append_text(char *out, size_t out_size, const char *text)
{
    if (!out || out_size == 0 || !text || text[0] == '\0') {
        return;
    }

    size_t cur = strlen(out);
    if (cur >= out_size - 1) {
        return;
    }

    if (cur > 0 && out[cur - 1] != ' ' && out[cur - 1] != '\n') {
        int n = snprintf(out + cur, out_size - cur, " ");
        if (n <= 0) {
            return;
        }
        cur += (size_t)n;
    }

    snprintf(out + cur, out_size - cur, "%s", text);
}

static void append_prefixed(char *out, size_t out_size, const char *prefix, const char *text)
{
    if (!text || text[0] == '\0') {
        return;
    }

    char buf[384] = {0};
    snprintf(buf, sizeof(buf), "%s%s", prefix ? prefix : "", text);
    append_text(out, out_size, buf);
}

static void parse_post_block(cJSON *lang_obj, char *out, size_t out_size)
{
    if (!cJSON_IsObject(lang_obj) || !out || out_size == 0) {
        return;
    }

    const char *title = json_str2(lang_obj, "title", NULL);
    if (title && title[0] != '\0') {
        append_text(out, out_size, title);
    }

    cJSON *content = cJSON_GetObjectItem(lang_obj, "content");
    if (!cJSON_IsArray(content)) {
        return;
    }

    cJSON *block = NULL;
    cJSON_ArrayForEach(block, content)
    {
        if (!cJSON_IsArray(block)) {
            continue;
        }

        cJSON *elem = NULL;
        cJSON_ArrayForEach(elem, block)
        {
            if (!cJSON_IsObject(elem)) {
                continue;
            }
            const char *tag = json_str2(elem, "tag", NULL);
            if (!tag) {
                continue;
            }

            if (strcmp(tag, "text") == 0 || strcmp(tag, "a") == 0) {
                const char *txt = json_str2(elem, "text", NULL);
                if (txt) {
                    append_text(out, out_size, txt);
                }
            } else if (strcmp(tag, "at") == 0) {
                const char *uname = json_str2(elem, "user_name", NULL);
                if (uname && uname[0] != '\0') {
                    char atbuf[96] = {0};
                    snprintf(atbuf, sizeof(atbuf), "@%s", uname);
                    append_text(out, out_size, atbuf);
                }
            }
        }
    }
}

static void parse_interactive_node(cJSON *node, char *out, size_t out_size);

static void parse_interactive_element(cJSON *element, char *out, size_t out_size)
{
    if (!cJSON_IsObject(element) || !out || out_size == 0) {
        return;
    }

    const char *tag = json_str2(element, "tag", NULL);
    if (!tag || tag[0] == '\0') {
        return;
    }

    if (strcmp(tag, "markdown") == 0 || strcmp(tag, "lark_md") == 0) {
        append_text(out, out_size, json_str2(element, "content", NULL));
        return;
    }

    if (strcmp(tag, "div") == 0) {
        cJSON *txt = cJSON_GetObjectItem(element, "text");
        if (cJSON_IsObject(txt)) {
            append_text(out, out_size, json_str2(txt, "content", "text"));
        } else if (cJSON_IsString(txt)) {
            append_text(out, out_size, txt->valuestring);
        }

        cJSON *fields = cJSON_GetObjectItem(element, "fields");
        if (cJSON_IsArray(fields)) {
            cJSON *field = NULL;
            cJSON_ArrayForEach(field, fields)
            {
                cJSON *field_text = cJSON_GetObjectItem(field, "text");
                if (cJSON_IsObject(field_text)) {
                    append_text(out, out_size, json_str2(field_text, "content", "text"));
                }
            }
        }
        return;
    }

    if (strcmp(tag, "a") == 0) {
        append_prefixed(out, out_size, "link: ", json_str2(element, "href", NULL));
        append_text(out, out_size, json_str2(element, "text", NULL));
        return;
    }

    if (strcmp(tag, "button") == 0) {
        cJSON *txt = cJSON_GetObjectItem(element, "text");
        if (cJSON_IsObject(txt)) {
            append_text(out, out_size, json_str2(txt, "content", "text"));
        } else if (cJSON_IsString(txt)) {
            append_text(out, out_size, txt->valuestring);
        }

        const char *url = json_str2(element, "url", NULL);
        if (!url) {
            cJSON *multi = cJSON_GetObjectItem(element, "multi_url");
            if (cJSON_IsObject(multi)) {
                url = json_str2(multi, "url", NULL);
            }
        }
        append_prefixed(out, out_size, "link: ", url);
        return;
    }

    if (strcmp(tag, "img") == 0) {
        cJSON *alt = cJSON_GetObjectItem(element, "alt");
        if (cJSON_IsObject(alt)) {
            const char *alt_text = json_str2(alt, "content", "text");
            if (alt_text && alt_text[0] != '\0') {
                append_text(out, out_size, alt_text);
            } else {
                append_text(out, out_size, "[image]");
            }
        } else {
            append_text(out, out_size, "[image]");
        }
        return;
    }

    if (strcmp(tag, "plain_text") == 0) {
        append_text(out, out_size, json_str2(element, "content", NULL));
        return;
    }

    if (strcmp(tag, "note") == 0) {
        cJSON *elements = cJSON_GetObjectItem(element, "elements");
        if (cJSON_IsArray(elements)) {
            cJSON *e = NULL;
            cJSON_ArrayForEach(e, elements)
            {
                parse_interactive_element(e, out, out_size);
            }
        }
        return;
    }

    if (strcmp(tag, "column_set") == 0) {
        cJSON *columns = cJSON_GetObjectItem(element, "columns");
        if (cJSON_IsArray(columns)) {
            cJSON *col = NULL;
            cJSON_ArrayForEach(col, columns)
            {
                cJSON *elements = cJSON_GetObjectItem(col, "elements");
                if (cJSON_IsArray(elements)) {
                    cJSON *e = NULL;
                    cJSON_ArrayForEach(e, elements)
                    {
                        parse_interactive_element(e, out, out_size);
                    }
                }
            }
        }
        return;
    }

    cJSON *elements = cJSON_GetObjectItem(element, "elements");
    if (cJSON_IsArray(elements)) {
        cJSON *e = NULL;
        cJSON_ArrayForEach(e, elements)
        {
            parse_interactive_element(e, out, out_size);
        }
    }
}

static void parse_interactive_node(cJSON *node, char *out, size_t out_size)
{
    if (!node || !out || out_size == 0) {
        return;
    }

    if (cJSON_IsString(node)) {
        cJSON *parsed = cJSON_Parse(node->valuestring);
        if (parsed) {
            parse_interactive_node(parsed, out, out_size);
            cJSON_Delete(parsed);
        } else {
            append_text(out, out_size, node->valuestring);
        }
        return;
    }

    if (!cJSON_IsObject(node)) {
        return;
    }

    cJSON *title = cJSON_GetObjectItem(node, "title");
    if (cJSON_IsObject(title)) {
        append_prefixed(out, out_size, "title: ", json_str2(title, "content", "text"));
    } else if (cJSON_IsString(title)) {
        append_prefixed(out, out_size, "title: ", title->valuestring);
    }

    cJSON *elements = cJSON_GetObjectItem(node, "elements");
    if (cJSON_IsArray(elements)) {
        cJSON *e = NULL;
        cJSON_ArrayForEach(e, elements)
        {
            parse_interactive_element(e, out, out_size);
        }
    }

    cJSON *card = cJSON_GetObjectItem(node, "card");
    if (cJSON_IsObject(card)) {
        parse_interactive_node(card, out, out_size);
    }

    cJSON *header = cJSON_GetObjectItem(node, "header");
    if (cJSON_IsObject(header)) {
        cJSON *header_title = cJSON_GetObjectItem(header, "title");
        if (cJSON_IsObject(header_title)) {
            append_prefixed(out, out_size, "title: ", json_str2(header_title, "content", "text"));
        } else if (cJSON_IsString(header_title)) {
            append_prefixed(out, out_size, "title: ", header_title->valuestring);
        }
    }
}

static void parse_share_card_content(const char *msg_type, cJSON *obj, char *out, size_t out_size)
{
    if (!msg_type || !out || out_size == 0) {
        return;
    }

    static const struct {
        const char *type;
        const char *label;
        const char *id_key;
    } share_types[] = {
        {"share_chat",           "shared chat",           "chat_id"  },
        {"share_user",           "shared user",           "user_id"  },
        {"share_calendar_event", "shared calendar event", "event_key"},
        {"merge_forward",        "merged forward messages", NULL     },
        {"system",               "system message",          NULL     },
    };

    for (size_t i = 0; i < sizeof(share_types) / sizeof(share_types[0]); i++) {
        if (strcmp(msg_type, share_types[i].type) != 0) {
            continue;
        }
        if (share_types[i].id_key && cJSON_IsObject(obj)) {
            const char *val = json_str2(obj, share_types[i].id_key, NULL);
            if (val && val[0] != '\0') {
                snprintf(out, out_size, "[%s: %s]", share_types[i].label, val);
                return;
            }
        }
        snprintf(out, out_size, "[%s]", share_types[i].label);
        return;
    }

    snprintf(out, out_size, "[%s]", msg_type);
}

static void extract_message_text(const char *msg_type, const char *content_json, char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (!msg_type) {
        return;
    }

    if (!content_json || content_json[0] == '\0') {
        snprintf(out, out_size, "[%s]", msg_type);
        return;
    }

    cJSON *obj = cJSON_Parse(content_json);
    if (!obj) {
        snprintf(out, out_size, "[%s]", msg_type);
        return;
    }

    if (strcmp(msg_type, "text") == 0) {
        const char *text = json_str2(obj, "text", NULL);
        if (text) {
            im_safe_copy(out, out_size, text);
        }
    } else if (strcmp(msg_type, "post") == 0) {
        if (cJSON_GetObjectItem(obj, "content")) {
            parse_post_block(obj, out, out_size);
        } else {
            static const char *post_langs[] = {"zh_cn", "en_us", "ja_jp"};
            for (size_t i = 0; i < sizeof(post_langs) / sizeof(post_langs[0]) && out[0] == '\0'; i++) {
                parse_post_block(cJSON_GetObjectItem(obj, post_langs[i]), out, out_size);
            }
        }
    } else if (strcmp(msg_type, "interactive") == 0) {
        parse_interactive_node(obj, out, out_size);
        if (out[0] == '\0') {
            snprintf(out, out_size, "[interactive message]");
        }
    } else if (strcmp(msg_type, "share_chat") == 0 || strcmp(msg_type, "share_user") == 0 ||
               strcmp(msg_type, "share_calendar_event") == 0 || strcmp(msg_type, "merge_forward") == 0 ||
               strcmp(msg_type, "system") == 0) {
        parse_share_card_content(msg_type, obj, out, out_size);
    } else {
        snprintf(out, out_size, "[%s]", msg_type);
    }

    cJSON_Delete(obj);
}

static void publish_inbound_feishu(const char *chat_id, const char *text)
{
    if (!chat_id || !text || chat_id[0] == '\0' || text[0] == '\0') {
        return;
    }

    im_msg_t in = {0};
    strncpy(in.channel, IM_CHAN_FEISHU, sizeof(in.channel) - 1);
    strncpy(in.chat_id, chat_id, sizeof(in.chat_id) - 1);
    in.content = im_strdup(text);
    if (!in.content) {
        return;
    }
    OPERATE_RET rt = message_bus_push_inbound(&in);
    if (rt != OPRT_OK) {
        IM_LOGW(TAG, "push inbound failed rt=%d", rt);
        im_free(in.content);
    }
}

static void handle_event_payload(const uint8_t *payload, size_t payload_len)
{
    if (!payload || payload_len == 0) {
        return;
    }

    cJSON *root = cJSON_ParseWithLength((const char *)payload, payload_len);
    if (!root) {
        IM_LOGW(TAG, "[feishu] event payload parse failed len=%u", (unsigned)payload_len);
        return;
    }

    const char *event_type = NULL;
    const char *event_id   = NULL;
    cJSON      *header     = cJSON_GetObjectItem(root, "header");
    if (cJSON_IsObject(header)) {
        event_type = json_str2(header, "event_type", NULL);
        event_id   = json_str2(header, "event_id", NULL);
    }
    if (!event_type) {
        event_type = json_str2(root, "type", NULL);
    }

    if (!event_type || strcmp(event_type, "im.message.receive_v1") != 0) {
        IM_LOGD(TAG, "[feishu] skip event type=%s", event_type ? event_type : "(null)");
        cJSON_Delete(root);
        return;
    }

    /* Dedup by event_id: same push may be redelivered (e.g. after reconnect) */
    if (event_id && event_id[0] != '\0') {
        uint64_t ev_key = im_fnv1a64(event_id);
        if (dedup_contains(&s_seen_event, ev_key)) {
            IM_LOGI(TAG, "[feishu] duplicate event_id dropped event_id=%s", event_id);
            cJSON_Delete(root);
            return;
        }
    }

    cJSON *event   = cJSON_GetObjectItem(root, "event");
    cJSON *sender  = event ? cJSON_GetObjectItem(event, "sender") : NULL;
    cJSON *message = event ? cJSON_GetObjectItem(event, "message") : NULL;
    if (!cJSON_IsObject(sender) || !cJSON_IsObject(message)) {
        IM_LOGW(TAG, "[feishu] event missing sender or message object");
        cJSON_Delete(root);
        return;
    }

    const char *sender_type = json_str2(sender, "sender_type", NULL);
    if (sender_type && strcmp(sender_type, "bot") == 0) {
        cJSON_Delete(root);
        return;
    }

    cJSON      *sender_id_obj   = cJSON_GetObjectItem(sender, "sender_id");
    const char *sender_open_id  = NULL;
    const char *sender_user_id  = NULL;
    const char *sender_union_id = NULL;
    if (cJSON_IsObject(sender_id_obj)) {
        sender_open_id  = json_str2(sender_id_obj, "open_id", NULL);
        sender_user_id  = json_str2(sender_id_obj, "user_id", NULL);
        sender_union_id = json_str2(sender_id_obj, "union_id", NULL);
    }

    char sender_identity[384] = {0};
    append_sender_id(sender_identity, sizeof(sender_identity), sender_open_id);
    append_sender_id(sender_identity, sizeof(sender_identity), sender_user_id);
    append_sender_id(sender_identity, sizeof(sender_identity), sender_union_id);
    if (sender_identity[0] == '\0') {
        IM_LOGW(TAG, "feishu sender id missing, drop message");
        cJSON_Delete(root);
        return;
    }

    if (!sender_open_id || sender_open_id[0] == '\0') {
        IM_LOGW(TAG, "feishu sender open_id missing, drop message");
        cJSON_Delete(root);
        return;
    }

    if (!sender_allowed(sender_identity)) {
        IM_LOGW(TAG, "feishu access denied sender=%s", sender_identity);
        cJSON_Delete(root);
        return;
    }

    /* Dedup by message_id: same user message may be repeated/replayed */
    const char *message_id = json_str2(message, "message_id", NULL);
    if (message_id && message_id[0]) {
        uint64_t msg_key = im_fnv1a64(message_id);
        if (dedup_contains(&s_seen_msg, msg_key)) {
            IM_LOGI(TAG, "[feishu] duplicate message_id dropped message_id=%s", message_id);
            if (event_id && event_id[0] != '\0') {
                dedup_insert(&s_seen_event, im_fnv1a64(event_id));
            }
            cJSON_Delete(root);
            return;
        }
        dedup_insert(&s_seen_msg, msg_key);
    }
    if (event_id && event_id[0] != '\0') {
        dedup_insert(&s_seen_event, im_fnv1a64(event_id));
    }

    const char *chat_id      = json_str2(message, "chat_id", NULL);
    const char *chat_type    = json_str2(message, "chat_type", NULL);
    const char *msg_type     = json_str2(message, "message_type", NULL);
    const char *content_json = json_str2(message, "content", NULL);

    char text[2048] = {0};
    extract_message_text(msg_type ? msg_type : "unknown", content_json, text, sizeof(text));
    if (text[0] == '\0') {
        IM_LOGW(TAG, "[feishu] message text empty msg_type=%s (unsupported or empty content)",
                  msg_type ? msg_type : "null");
        cJSON_Delete(root);
        return;
    }

    const char *reply_to = NULL;
    if (chat_type && strcmp(chat_type, "group") == 0) {
        reply_to = chat_id;
    } else {
        reply_to = sender_open_id;
    }

    if (!reply_to || reply_to[0] == '\0') {
        cJSON_Delete(root);
        return;
    }

    IM_LOGI(TAG, "[feishu] inbound chat=%s type=%s len=%u", reply_to, msg_type ? msg_type : "?",
              (unsigned)strlen(text));

    publish_inbound_feishu(reply_to, text);
    cJSON_Delete(root);
}

/* -------- ws message handler -------- */

static OPERATE_RET send_pb_frame(fs_ws_conn_t *conn, const fs_pb_frame_t *frame)
{
    uint8_t *bin     = NULL;
    size_t   bin_len = 0;
    if (!fs_pb_encode_frame(frame, &bin, &bin_len)) {
        return OPRT_COM_ERROR;
    }

    OPERATE_RET rt = fs_ws_send_frame(conn, 0x2, bin, bin_len);
    im_free(bin);
    return rt;
}

static OPERATE_RET send_ping_frame(fs_ws_conn_t *conn, int service_id)
{
    fs_pb_frame_t *ping = fs_pb_frame_new();
    if (!ping) {
        return OPRT_MALLOC_FAILED;
    }

    ping->seq_id       = 0;
    ping->log_id       = 0;
    ping->service      = service_id;
    ping->method       = 0;
    ping->header_count = 1;
    im_safe_copy(ping->headers[0].key, sizeof(ping->headers[0].key), "type");
    im_safe_copy(ping->headers[0].value, sizeof(ping->headers[0].value), "ping");

    OPERATE_RET rt = send_pb_frame(conn, ping);
    fs_pb_frame_delete(ping);
    return rt;
}

static void handle_control_pb_frame(const fs_pb_frame_t *frame, uint32_t *ping_interval_ms)
{
    const char *type = fs_pb_get_header(frame, "type");
    if (!type) {
        return;
    }

    if (strcmp(type, "pong") == 0 && frame->payload && frame->payload_len > 0) {
        cJSON *obj = cJSON_ParseWithLength((const char *)frame->payload, frame->payload_len);
        if (obj) {
            int p = json_int2(obj, "PingInterval", "ping_interval", -1);
            if (p > 0) {
                *ping_interval_ms = (uint32_t)p * 1000u;
            }
            cJSON_Delete(obj);
        }
    }
}

static int pb_header_int(const fs_pb_frame_t *frame, const char *key, int defv)
{
    const char *v = fs_pb_get_header(frame, key);
    return v ? atoi(v) : defv;
}

static OPERATE_RET handle_data_pb_frame(fs_ws_conn_t *conn, const fs_pb_frame_t *frame)
{
    const char *type   = fs_pb_get_header(frame, "type");
    const char *msg_id = fs_pb_get_header(frame, "message_id");
    uint32_t    sum    = (uint32_t)pb_header_int(frame, "sum", 1);
    uint32_t    seq    = (uint32_t)pb_header_int(frame, "seq", 0);

    uint8_t    *payload     = NULL;
    size_t      payload_len = 0;
    OPERATE_RET rt          = fs_frag_merge(msg_id, sum, seq, frame->payload ? frame->payload : (const uint8_t *)"",
                                            frame->payload_len, &payload, &payload_len);
    if (rt == OPRT_RESOURCE_NOT_READY) {
        return OPRT_OK;
    }
    if (rt != OPRT_OK) {
        return rt;
    }

    /* Feishu requires ACK within 3s or it will resend. Send ACK before business logic to avoid timeout from HTTP etc. in handle_event_payload. */
    static const char ack_ok[] = "{\"code\":200}";
    fs_pb_frame_t    *ack      = fs_pb_frame_new();
    if (!ack) {
        im_free(payload);
        return OPRT_MALLOC_FAILED;
    }
    ack->seq_id       = frame->seq_id;
    ack->log_id       = frame->log_id;
    ack->service      = frame->service;
    ack->method       = frame->method;
    ack->header_count = frame->header_count;
    if (ack->header_count > FS_WS_FRAME_MAX_HEADERS) {
        ack->header_count = FS_WS_FRAME_MAX_HEADERS;
    }
    for (size_t i = 0; i < ack->header_count; i++) {
        im_safe_copy(ack->headers[i].key, sizeof(ack->headers[i].key), frame->headers[i].key);
        im_safe_copy(ack->headers[i].value, sizeof(ack->headers[i].value), frame->headers[i].value);
    }
    ack->payload     = (uint8_t *)ack_ok;
    ack->payload_len = sizeof(ack_ok) - 1;
    rt               = send_pb_frame(conn, ack);
    ack->payload     = NULL;
    ack->payload_len = 0;
    fs_pb_frame_delete(ack);
    if (rt != OPRT_OK) {
        im_free(payload);
        return rt;
    }

    if (payload && payload_len > 0) {
        if (type && strcmp(type, "event") == 0) {
            handle_event_payload(payload, payload_len);
        } else {
            IM_LOGI(TAG, "[feishu] ws data frame type=%s (only type=event handled for messages)",
                      type ? type : "(null)");
        }
    }
    im_free(payload);
    return OPRT_OK;
}

/* -------- ws main loop -------- */

static void feishu_ws_task(void *arg)
{
    (void)arg;

    while (1) {
        if (s_app_id[0] == '\0' || s_app_secret[0] == '\0') {
            tal_system_sleep(3000);
            continue;
        }

        OPERATE_RET rt = ensure_tenant_token();
        if (rt != OPRT_OK) {
            IM_LOGW(TAG, "ensure tenant credential failed rt=%d", rt);
            tal_system_sleep(FS_WS_DEFAULT_RECONNECT_MS);
            continue;
        }

        fs_ws_conf_t conf;
        rt = fs_fetch_ws_conf(&conf);
        if (rt != OPRT_OK) {
            IM_LOGW(TAG, "fetch ws endpoint failed rt=%d", rt);
            tal_system_sleep(FS_WS_DEFAULT_RECONNECT_MS);
            continue;
        }

        uint32_t reconnect_ms = conf.reconnect_interval_ms ? conf.reconnect_interval_ms : FS_WS_DEFAULT_RECONNECT_MS;

        char     ws_host[128] = {0};
        char     ws_path[640] = {0};
        uint16_t ws_port      = 443;
        int      service_id   = 0;

        rt = fs_parse_ws_url(conf.url, ws_host, sizeof(ws_host), &ws_port, ws_path, sizeof(ws_path), &service_id);
        if (rt != OPRT_OK) {
            IM_LOGW(TAG, "parse ws endpoint failed rt=%d", rt);
            tal_system_sleep(reconnect_ms);
            continue;
        }

        fs_ws_conn_t *conn = im_calloc(1, sizeof(fs_ws_conn_t));
        if (!conn) {
            tal_system_sleep(reconnect_ms);
            continue;
        }

        rt = fs_conn_open(conn, ws_host, ws_port, FS_HTTP_TIMEOUT_MS);
        if (rt != OPRT_OK) {
            IM_LOGW(TAG, "feishu ws connect failed rt=%d", rt);
            im_free(conn);
            tal_system_sleep(reconnect_ms);
            continue;
        }

        rt = fs_ws_handshake(conn, ws_host, ws_path);
        if (rt != OPRT_OK) {
            IM_LOGW(TAG, "feishu ws handshake failed rt=%d", rt);
            fs_conn_close(conn);
            im_free(conn);
            tal_system_sleep(reconnect_ms);
            continue;
        }

        uint32_t ping_interval_ms = conf.ping_interval_ms ? conf.ping_interval_ms : FS_WS_DEFAULT_PING_MS;
        uint32_t next_ping_ms     = tal_system_get_millisecond() + ping_interval_ms;

        IM_LOGI(TAG, "feishu ws online!");

        while (1) {
            uint32_t now = tal_system_get_millisecond();
            if ((int32_t)(now - next_ping_ms) >= 0) {
                OPERATE_RET ping_rt = send_ping_frame(conn, service_id);
                if (ping_rt != OPRT_OK) {
                    IM_LOGW(TAG, "feishu ws ping failed rt=%d", ping_rt);
                    break;
                }
                next_ping_ms = now + ping_interval_ms;
            }

            uint8_t  opcode      = 0;
            uint8_t *payload     = NULL;
            size_t   payload_len = 0;
            rt = fs_ws_poll_frame(conn, FS_WS_POLL_WAIT_MS, &opcode, &payload, &payload_len);
            if (rt == OPRT_RESOURCE_NOT_READY) {
                continue;
            }
            if (rt != OPRT_OK) {
                im_free(payload);
                IM_LOGW(TAG, "feishu ws poll failed rt=%d", rt);
                break;
            }

            if (opcode == 0x2 && payload && payload_len > 0) {
                fs_pb_frame_t *pb = fs_pb_frame_new();
                if (!pb) {
                    im_free(payload);
                    break;
                }

                if (fs_pb_parse_frame(payload, payload_len, pb)) {
                    if (pb->method == 0) {
                        handle_control_pb_frame(pb, &ping_interval_ms);
                    } else if (pb->method == 1) {
                        OPERATE_RET hrt = handle_data_pb_frame(conn, pb);
                        if (hrt != OPRT_OK) {
                            fs_pb_frame_delete(pb);
                            im_free(payload);
                            break;
                        }
                    }
                }
                fs_pb_frame_delete(pb);
            } else if (opcode == 0x8) {
                im_free(payload);
                IM_LOGW(TAG, "feishu ws closed by peer");
                break;
            } else if (opcode == 0x9) {
                (void)fs_ws_send_frame(conn, 0xA, payload, payload_len);
            }

            im_free(payload);
        }

        fs_conn_close(conn);
        im_free(conn);
        tal_system_sleep(reconnect_ms);
    }
}

/* -------- public APIs -------- */

OPERATE_RET feishu_bot_init(void)
{
    if (IM_SECRET_FS_APP_ID[0] != '\0') {
        im_safe_copy(s_app_id, sizeof(s_app_id), IM_SECRET_FS_APP_ID);
    }
    if (IM_SECRET_FS_APP_SECRET[0] != '\0') {
        im_safe_copy(s_app_secret, sizeof(s_app_secret), IM_SECRET_FS_APP_SECRET);
    }
#ifdef IM_SECRET_FS_ALLOW_FROM
    if (IM_SECRET_FS_ALLOW_FROM[0] != '\0') {
        im_safe_copy(s_allow_from, sizeof(s_allow_from), IM_SECRET_FS_ALLOW_FROM);
    }
#endif

    char tmp[512] = {0};
    if (im_kv_get_string(IM_NVS_FS, IM_NVS_KEY_FS_APP_ID, tmp, sizeof(tmp)) == OPRT_OK && tmp[0] != '\0') {
        im_safe_copy(s_app_id, sizeof(s_app_id), tmp);
    }

    memset(tmp, 0, sizeof(tmp));
    if (im_kv_get_string(IM_NVS_FS, IM_NVS_KEY_FS_APP_SECRET, tmp, sizeof(tmp)) == OPRT_OK && tmp[0] != '\0') {
        im_safe_copy(s_app_secret, sizeof(s_app_secret), tmp);
    }

#ifdef IM_NVS_KEY_FS_ALLOW_FROM
    memset(tmp, 0, sizeof(tmp));
    if (im_kv_get_string(IM_NVS_FS, IM_NVS_KEY_FS_ALLOW_FROM, tmp, sizeof(tmp)) == OPRT_OK && tmp[0] != '\0') {
        im_safe_copy(s_allow_from, sizeof(s_allow_from), tmp);
    }
#endif

    s_tenant_token[0]  = '\0';
    s_tenant_expire_ms = 0;
    fs_frag_clear();

    IM_LOGI(TAG, "feishu init credentials=%s allow_from=%s",
              (s_app_id[0] && s_app_secret[0]) ? "configured" : "empty", s_allow_from[0] ? "configured" : "open");

    return OPRT_OK;
}

OPERATE_RET feishu_bot_start(void)
{
    if (s_app_id[0] == '\0' || s_app_secret[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    OPERATE_RET rt = ensure_tenant_token();
    if (rt != OPRT_OK) {
        IM_LOGW(TAG, "initial tenant credential fetch failed rt=%d", rt);
        return rt;
    }

    if (s_ws_thread) {
        return OPRT_OK;
    }

    THREAD_CFG_T cfg = {0};
    cfg.stackDepth   = IM_FS_POLL_STACK;
    cfg.priority     = THREAD_PRIO_1;
    cfg.thrdname     = "im_fs_ws";

    rt = tal_thread_create_and_start(&s_ws_thread, NULL, NULL, feishu_ws_task, NULL, &cfg);
    if (rt != OPRT_OK) {
        s_ws_thread = NULL;
        return rt;
    }

    IM_LOGI(TAG, "feishu ws service started");
    return OPRT_OK;
}

OPERATE_RET feishu_send_message(const char *chat_id, const char *text)
{
    if (!chat_id || !text) {
        return OPRT_INVALID_PARM;
    }
    if (s_app_id[0] == '\0' || s_app_secret[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    OPERATE_RET rt = ensure_tenant_token();
    if (rt != OPRT_OK) {
        return rt;
    }

    const char *rid_type = (strncmp(chat_id, "oc_", 3) == 0) ? "chat_id" : "open_id";

    cJSON *content = cJSON_CreateObject();
    if (!content) {
        return OPRT_MALLOC_FAILED;
    }
    cJSON_AddStringToObject(content, "text", text);
    char *content_json = cJSON_PrintUnformatted(content);
    cJSON_Delete(content);
    if (!content_json) {
        return OPRT_MALLOC_FAILED;
    }

    cJSON *body = cJSON_CreateObject();
    if (!body) {
        cJSON_free(content_json);
        return OPRT_MALLOC_FAILED;
    }
    cJSON_AddStringToObject(body, "receive_id", chat_id);
    cJSON_AddStringToObject(body, "msg_type", "text");
    cJSON_AddStringToObject(body, "content", content_json);
    char *body_json = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    cJSON_free(content_json);
    if (!body_json) {
        return OPRT_MALLOC_FAILED;
    }

    char path[192] = {0};
    snprintf(path, sizeof(path), "/open-apis/im/v1/messages?receive_id_type=%s", rid_type);

    char *resp = im_malloc(FS_HTTP_RESP_BUF_SIZE);
    if (!resp) {
        cJSON_free(body_json);
        return OPRT_MALLOC_FAILED;
    }
    memset(resp, 0, FS_HTTP_RESP_BUF_SIZE);

    uint16_t status = 0;
    rt = fs_http_call(FS_HOST, path, "POST", body_json, s_tenant_token, resp, FS_HTTP_RESP_BUF_SIZE, &status);

    const char *err_msg = NULL;
    bool        ok      = (rt == OPRT_OK && status == 200 && fs_response_ok(resp, &err_msg));
    if (!ok) {
        if (refresh_tenant_token() == OPRT_OK) {
            memset(resp, 0, FS_HTTP_RESP_BUF_SIZE);
            status = 0;
            rt = fs_http_call(FS_HOST, path, "POST", body_json, s_tenant_token, resp, FS_HTTP_RESP_BUF_SIZE, &status);
            ok = (rt == OPRT_OK && status == 200 && fs_response_ok(resp, &err_msg));
        }
    }

    cJSON_free(body_json);

    if (!ok) {
        IM_LOGE(TAG, "feishu send failed rid=%s type=%s rt=%d http=%u", chat_id, rid_type, rt, status);
    } else {
        IM_LOGI(TAG, "feishu send success rid=%s type=%s bytes=%u", chat_id, rid_type, (unsigned)strlen(text));
    }
    im_free(resp);

    return ok ? OPRT_OK : OPRT_COM_ERROR;
}

OPERATE_RET feishu_set_app_id(const char *app_id)
{
    if (!app_id) {
        return OPRT_INVALID_PARM;
    }
    im_safe_copy(s_app_id, sizeof(s_app_id), app_id);
    s_tenant_token[0]  = '\0';
    s_tenant_expire_ms = 0;
    return im_kv_set_string(IM_NVS_FS, IM_NVS_KEY_FS_APP_ID, app_id);
}

OPERATE_RET feishu_set_app_secret(const char *app_secret)
{
    if (!app_secret) {
        return OPRT_INVALID_PARM;
    }
    im_safe_copy(s_app_secret, sizeof(s_app_secret), app_secret);
    s_tenant_token[0]  = '\0';
    s_tenant_expire_ms = 0;
    return im_kv_set_string(IM_NVS_FS, IM_NVS_KEY_FS_APP_SECRET, app_secret);
}

OPERATE_RET feishu_set_allow_from(const char *allow_from_csv)
{
    if (!allow_from_csv) {
        return OPRT_INVALID_PARM;
    }

    im_safe_copy(s_allow_from, sizeof(s_allow_from), allow_from_csv);
#ifdef IM_NVS_KEY_FS_ALLOW_FROM
    return im_kv_set_string(IM_NVS_FS, IM_NVS_KEY_FS_ALLOW_FROM, allow_from_csv);
#else
    return OPRT_OK;
#endif
}
