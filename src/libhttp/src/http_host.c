/**
 * @file http_host.c
 * @brief Thin embedded HTTP host implementation.
 * @version 1.0
 * @date 2026-05-12
 * @copyright Copyright (c) 2021-2026 Tuya Inc.
 */
#include "http_host.h"

#include "tal_api.h"
#include "tal_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define HTTP_HOST_DEFAULT_PORT                 80
#define HTTP_HOST_DEFAULT_BACKLOG              2
#define HTTP_HOST_DEFAULT_LISTEN_RECV_MS       2500
#define HTTP_HOST_DEFAULT_LISTEN_SEND_MS       2500
#define HTTP_HOST_DEFAULT_CLIENT_RECV_SLICE_MS 30
#define HTTP_HOST_DEFAULT_CLIENT_SEND_MS       2500
#define HTTP_HOST_DEFAULT_CLIENT_WAIT_MAX      120
#define HTTP_HOST_DEFAULT_MAX_REQUEST_SIZE     1536
#define HTTP_HOST_DEFAULT_STACK_DEPTH          (8 * 1024)
#define HTTP_HOST_DEFAULT_PRIORITY             THREAD_PRIO_2
#define HTTP_HOST_DEFAULT_THREAD_NAME          "http_host"
#define HTTP_HOST_STOP_POLL_MS                 50
#define HTTP_HOST_STOP_WAIT_MAX                200

/***********************************************************
*********************typedef define*************************
***********************************************************/
typedef struct {
    HTTP_HOST_CFG_T cfg;
    int listen_fd;
    THREAD_HANDLE thread;
    BOOL_T stop_requested;
    BOOL_T shutdown_requested;
    BOOL_T thread_running;
    HTTP_HOST_REQUEST_CB on_request;
    HTTP_HOST_IDLE_CB on_idle;
    void *user_ctx;
} HTTP_HOST_CTX_T;

/***********************************************************
********************function declaration********************
***********************************************************/
static OPERATE_RET __http_host_send_all(int fd, const char *data, uint32_t len);
static void __http_host_apply_cfg_defaults(HTTP_HOST_CFG_T *cfg);
static void __http_host_close_listen(HTTP_HOST_CTX_T *ctx);
static void __http_host_task(void *arg);

/**
 * @brief Apply default values for unset configuration fields.
 * @param[in,out] cfg host configuration.
 * @return none
 */
static void __http_host_apply_cfg_defaults(HTTP_HOST_CFG_T *cfg)
{
    if (cfg == NULL) {
        return;
    }

    if (cfg->port == 0) {
        cfg->port = HTTP_HOST_DEFAULT_PORT;
    }
    if (cfg->backlog == 0) {
        cfg->backlog = HTTP_HOST_DEFAULT_BACKLOG;
    }
    if (cfg->listen_recv_timeout_ms == 0) {
        cfg->listen_recv_timeout_ms = HTTP_HOST_DEFAULT_LISTEN_RECV_MS;
    }
    if (cfg->listen_send_timeout_ms == 0) {
        cfg->listen_send_timeout_ms = HTTP_HOST_DEFAULT_LISTEN_SEND_MS;
    }
    if (cfg->client_recv_slice_ms == 0) {
        cfg->client_recv_slice_ms = HTTP_HOST_DEFAULT_CLIENT_RECV_SLICE_MS;
    }
    if (cfg->client_send_timeout_ms == 0) {
        cfg->client_send_timeout_ms = HTTP_HOST_DEFAULT_CLIENT_SEND_MS;
    }
    if (cfg->client_wait_max_rounds == 0) {
        cfg->client_wait_max_rounds = HTTP_HOST_DEFAULT_CLIENT_WAIT_MAX;
    }
    if (cfg->max_request_size == 0) {
        cfg->max_request_size = HTTP_HOST_DEFAULT_MAX_REQUEST_SIZE;
    }
    if (cfg->stack_depth == 0) {
        cfg->stack_depth = HTTP_HOST_DEFAULT_STACK_DEPTH;
    }
    if (cfg->priority == 0) {
        cfg->priority = HTTP_HOST_DEFAULT_PRIORITY;
    }
    if (cfg->thread_name == NULL) {
        cfg->thread_name = HTTP_HOST_DEFAULT_THREAD_NAME;
    }
}

/**
 * @brief Close listen socket if open.
 * @param[in,out] ctx host context.
 * @return none
 */
static void __http_host_close_listen(HTTP_HOST_CTX_T *ctx)
{
    OPERATE_RET rt = OPRT_OK;

    if ((ctx == NULL) || (ctx->listen_fd < 0)) {
        return;
    }

    rt = tal_net_close(ctx->listen_fd);
    if (rt != OPRT_OK) {
        PR_ERR("http_host tal_net_close listen ret:%d", rt);
    }
    ctx->listen_fd = -1;
}

/**
 * @brief Send full data to socket.
 * @param[in] fd socket descriptor.
 * @param[in] data output data buffer.
 * @param[in] len output data length.
 * @return OPRT_OK on success.
 */
static OPERATE_RET __http_host_send_all(int fd, const char *data, uint32_t len)
{
    uint32_t sent = 0;

    if ((fd < 0) || (data == NULL)) {
        return OPRT_INVALID_PARM;
    }

    while (sent < len) {
        int ret = tal_net_send(fd, data + sent, len - sent);
        if (ret <= 0) {
            return OPRT_SEND_ERR;
        }
        sent += (uint32_t)ret;
    }

    return OPRT_OK;
}

/**
 * @brief Find HTTP header end offset in request buffer.
 * @param[in] req request buffer.
 * @param[in] req_len request length.
 * @return header end offset, or -1 if not found.
 */
int http_host_find_header_end(const char *req, uint32_t req_len)
{
    uint32_t i = 0;

    if ((req == NULL) || (req_len < 4)) {
        return -1;
    }

    for (i = 0; (i + 3) < req_len; i++) {
        if ((req[i] == '\r') && (req[i + 1] == '\n') && (req[i + 2] == '\r') && (req[i + 3] == '\n')) {
            return (int)(i + 4);
        }
    }

    return -1;
}

/**
 * @brief Parse Content-Length from request headers.
 * @param[in] req request buffer.
 * @return content length if present, -1 otherwise.
 */
int http_host_get_content_length(const char *req)
{
    const char *p = NULL;

    if (req == NULL) {
        return -1;
    }

    p = strstr(req, "Content-Length:");
    if (p == NULL) {
        p = strstr(req, "content-length:");
    }
    if (p == NULL) {
        return -1;
    }

    p += strlen("Content-Length:");
    while ((*p == ' ') || (*p == '\t')) {
        p++;
    }

    return atoi(p);
}

/**
 * @brief Compare header name case-insensitively.
 * @param[in] a first string.
 * @param[in] b second string.
 * @param[in] len compare length.
 * @return 0 if equal.
 */
static int __http_host_header_name_equal(const char *a, const char *b, uint32_t len)
{
    uint32_t i = 0;

    if ((a == NULL) || (b == NULL)) {
        return -1;
    }

    for (i = 0; i < len; i++) {
        char ca = a[i];
        char cb = b[i];

        if ((ca >= 'A') && (ca <= 'Z')) {
            ca = (char)(ca - 'A' + 'a');
        }
        if ((cb >= 'A') && (cb <= 'Z')) {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Get header value from request headers (case-insensitive name).
 * @param[in] req request buffer.
 * @param[in] req_len request length.
 * @param[in] name header name.
 * @param[out] out output buffer.
 * @param[in] out_len output buffer size.
 * @return OPRT_OK on success, OPRT_NOT_FOUND if missing.
 */
OPERATE_RET http_host_get_header_value(const char *req, uint32_t req_len, const char *name, char *out, uint32_t out_len)
{
    int header_end = 0;
    const char *line = NULL;
    const char *line_end = NULL;
    uint32_t name_len = 0;

    if ((req == NULL) || (name == NULL) || (out == NULL) || (out_len == 0)) {
        return OPRT_INVALID_PARM;
    }

    header_end = http_host_find_header_end(req, req_len);
    if (header_end <= 0) {
        return OPRT_NOT_FOUND;
    }

    name_len = (uint32_t)strlen(name);
    line = req;
    while (line < (req + header_end)) {
        line_end = strstr(line, "\r\n");
        if (line_end == NULL) {
            break;
        }

        if ((__http_host_header_name_equal(line, name, name_len) == 0) && (line[name_len] == ':')) {
            const char *value = line + name_len + 1;
            uint32_t value_len = 0;

            while ((*value == ' ') || (*value == '\t')) {
                value++;
            }

            value_len = (uint32_t)(line_end - value);
            if (value_len >= out_len) {
                value_len = out_len - 1;
            }
            if (value_len > 0) {
                memcpy(out, value, value_len);
            }
            out[value_len] = '\0';
            return OPRT_OK;
        }

        line = line_end + 2;
    }

    return OPRT_NOT_FOUND;
}

/**
 * @brief Send HTTP response with Connection: close.
 * @param[in] client_fd client socket descriptor.
 * @param[in] status HTTP status line text.
 * @param[in] content_type Content-Type header value.
 * @param[in] body response body.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_host_reply(int client_fd, const char *status, const char *content_type, const char *body)
{
    char hdr[256] = {0};
    uint32_t body_len = 0;
    OPERATE_RET rt = OPRT_OK;

    if ((status == NULL) || (content_type == NULL) || (body == NULL)) {
        return OPRT_INVALID_PARM;
    }

    body_len = (uint32_t)strlen(body);
    (void)snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 %s\r\n"
                   "Content-Type: %s\r\n"
                   "Content-Length: %u\r\n"
                   "Connection: close\r\n\r\n",
                   status, content_type, body_len);

    rt = __http_host_send_all(client_fd, hdr, (uint32_t)strlen(hdr));
    if (rt != OPRT_OK) {
        return rt;
    }

    return __http_host_send_all(client_fd, body, body_len);
}

/**
 * @brief Send 404 Not Found response.
 * @param[in] client_fd client socket descriptor.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_host_reply_not_found(int client_fd)
{
    return http_host_reply(client_fd, "404 Not Found", "text/plain; charset=utf-8", "404 not found");
}

/**
 * @brief Send HTTP 302 redirect response with Connection: close.
 * @param[in] client_fd client socket descriptor.
 * @param[in] location redirect target URL.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_host_reply_redirect(int client_fd, const char *location)
{
    char hdr[384] = {0};
    const char *body = "Redirect";
    uint32_t body_len = (uint32_t)strlen(body);
    OPERATE_RET rt = OPRT_OK;

    if (location == NULL) {
        return OPRT_INVALID_PARM;
    }

    (void)snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 302 Found\r\n"
                   "Location: %s\r\n"
                   "Content-Type: text/html; charset=utf-8\r\n"
                   "Content-Length: %u\r\n"
                   "Connection: close\r\n\r\n",
                   location, body_len);

    rt = __http_host_send_all(client_fd, hdr, (uint32_t)strlen(hdr));
    if (rt != OPRT_OK) {
        return rt;
    }

    return __http_host_send_all(client_fd, body, body_len);
}

/**
 * @brief Close listener and exit accept loop.
 * @param[in] host host handle.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_host_request_shutdown(HTTP_HOST_HANDLE_T host)
{
    HTTP_HOST_CTX_T *ctx = (HTTP_HOST_CTX_T *)host;

    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }

    ctx->shutdown_requested = TRUE;
    __http_host_close_listen(ctx);
    return OPRT_OK;
}

/**
 * @brief Stop HTTP host and release resources.
 * @param[in] host host handle.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_host_stop(HTTP_HOST_HANDLE_T host)
{
    HTTP_HOST_CTX_T *ctx = (HTTP_HOST_CTX_T *)host;
    uint32_t wait_round = 0;

    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }

    ctx->stop_requested = TRUE;
    (void)http_host_request_shutdown(host);

    while ((ctx->thread_running == TRUE) && (wait_round < HTTP_HOST_STOP_WAIT_MAX)) {
        tal_system_sleep(HTTP_HOST_STOP_POLL_MS);
        wait_round++;
    }

    if (ctx->thread_running == TRUE) {
        PR_WARN("http_host stop timeout");
        return OPRT_TIMEOUT;
    }

    return OPRT_OK;
}

/**
 * @brief Start HTTP host listener thread.
 * @param[out] host output host handle.
 * @param[in] cfg host configuration.
 * @param[in] on_request request callback.
 * @param[in] on_idle idle callback on accept timeout.
 * @param[in] user_ctx user context passed to callbacks.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_host_start(HTTP_HOST_HANDLE_T *host, const HTTP_HOST_CFG_T *cfg, HTTP_HOST_REQUEST_CB on_request,
                            HTTP_HOST_IDLE_CB on_idle, void *user_ctx)
{
    HTTP_HOST_CTX_T *ctx = NULL;
    THREAD_CFG_T thread_cfg = {0};
    OPERATE_RET rt = OPRT_OK;

    if ((host == NULL) || (cfg == NULL) || (on_request == NULL)) {
        return OPRT_INVALID_PARM;
    }

    ctx = (HTTP_HOST_CTX_T *)tal_malloc(sizeof(HTTP_HOST_CTX_T));
    if (ctx == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(ctx, 0, sizeof(HTTP_HOST_CTX_T));

    ctx->cfg = *cfg;
    __http_host_apply_cfg_defaults(&ctx->cfg);
    ctx->listen_fd = -1;
    ctx->on_request = on_request;
    ctx->on_idle = on_idle;
    ctx->user_ctx = user_ctx;

    thread_cfg.stackDepth = ctx->cfg.stack_depth;
    thread_cfg.priority = ctx->cfg.priority;
    thread_cfg.thrdname = (char *)ctx->cfg.thread_name;

    ctx->thread_running = TRUE;
    rt = tal_thread_create_and_start(&ctx->thread, NULL, NULL, __http_host_task, ctx, &thread_cfg);
    if (rt != OPRT_OK) {
        tal_free(ctx);
        return rt;
    }

    *host = ctx;
    return OPRT_OK;
}

/**
 * @brief HTTP host service thread.
 * @param[in] arg host context pointer.
 * @return none
 */
static void __http_host_task(void *arg)
{
    HTTP_HOST_CTX_T *ctx = (HTTP_HOST_CTX_T *)arg;
    OPERATE_RET rt = OPRT_OK;
    char *req_buf = NULL;
    THREAD_HANDLE self_thread = NULL;

    if (ctx == NULL) {
        return;
    }

    self_thread = ctx->thread;
    req_buf = (char *)tal_malloc(ctx->cfg.max_request_size);
    if (req_buf == NULL) {
        PR_ERR("http_host request buffer alloc failed");
        goto __EXIT;
    }

    ctx->listen_fd = tal_net_socket_create(PROTOCOL_TCP);
    if (ctx->listen_fd < 0) {
        PR_ERR("http_host create listen socket failed");
        goto __EXIT;
    }

    rt = tal_net_set_reuse(ctx->listen_fd);
    if (rt != OPRT_OK) {
        goto __EXIT;
    }

    TUYA_CALL_ERR_LOG(tal_net_set_timeout(ctx->listen_fd, ctx->cfg.listen_recv_timeout_ms, TRANS_RECV));
    TUYA_CALL_ERR_LOG(tal_net_set_timeout(ctx->listen_fd, ctx->cfg.listen_send_timeout_ms, TRANS_SEND));
    rt = tal_net_bind(ctx->listen_fd, TY_IPADDR_ANY, ctx->cfg.port);
    if (rt != OPRT_OK) {
        goto __EXIT;
    }

    rt = tal_net_listen(ctx->listen_fd, ctx->cfg.backlog);
    if (rt != OPRT_OK) {
        goto __EXIT;
    }
    PR_NOTICE("http_host started at port %u", ctx->cfg.port);

    while ((ctx->stop_requested == FALSE) && (ctx->shutdown_requested == FALSE)) {
        TUYA_IP_ADDR_T client_addr = 0;
        uint16_t client_port = 0;
        int client_fd = tal_net_accept(ctx->listen_fd, &client_addr, &client_port);

        if (client_fd <= 0) {
            tal_system_sleep(HTTP_HOST_STOP_POLL_MS);
            if (ctx->on_idle != NULL) {
                ctx->on_idle((HTTP_HOST_HANDLE_T)ctx, ctx->user_ctx);
            }
            if ((ctx->stop_requested == TRUE) || (ctx->shutdown_requested == TRUE)) {
                break;
            }
            continue;
        }

        do {
            int total_len = 0;
            int header_end = -1;
            int content_len = 0;
            int wait_round = 0;
            uint32_t req_cap = ctx->cfg.max_request_size;

            TUYA_CALL_ERR_LOG(tal_net_set_timeout(client_fd, ctx->cfg.client_recv_slice_ms, TRANS_RECV));
            TUYA_CALL_ERR_LOG(tal_net_set_timeout(client_fd, ctx->cfg.client_send_timeout_ms, TRANS_SEND));

            memset(req_buf, 0, req_cap);
            while ((total_len < (int)(req_cap - 1)) && (wait_round < (int)ctx->cfg.client_wait_max_rounds)) {
                int recv_len = tal_net_recv(client_fd, req_buf + total_len, req_cap - 1 - (uint32_t)total_len);
                if (recv_len > 0) {
                    total_len += recv_len;
                    req_buf[total_len] = '\0';
                    header_end = http_host_find_header_end(req_buf, (uint32_t)total_len);
                    if (header_end > 0) {
                        if ((strncmp(req_buf, "POST ", 5) == 0) || (strncmp(req_buf, "PUT ", 4) == 0)) {
                            content_len = http_host_get_content_length(req_buf);
                            if ((content_len <= 0) || (total_len >= (header_end + content_len))) {
                                break;
                            }
                        } else {
                            break;
                        }
                    }
                    continue;
                }

                if (recv_len == OPRT_TIMEOUT) {
                    wait_round++;
                    continue;
                }
                break;
            }

            if (total_len <= 0) {
                PR_WARN("http_host request receive timeout or disconnected");
                break;
            }

            if ((strncmp(req_buf, "POST ", 5) == 0) || (strncmp(req_buf, "PUT ", 4) == 0)) {
                int he = http_host_find_header_end(req_buf, (uint32_t)total_len);
                int cl = http_host_get_content_length(req_buf);
                if ((he > 0) && (cl > 0)) {
                    int max_body = (int)req_cap - 1 - he;
                    if ((max_body < 0) || (cl > max_body) || (total_len < (he + cl))) {
                        PR_WARN("http_host post body incomplete or oversize, ignore");
                        break;
                    }
                }
            }

            if (ctx->on_request != NULL) {
                HTTP_HOST_REQUEST_T request = {0};
                int body_off = http_host_find_header_end(req_buf, (uint32_t)total_len);

                request.client_fd = client_fd;
                request.raw = req_buf;
                request.raw_len = (uint32_t)total_len;
                (void)sscanf(req_buf, "%11s %95s", request.method, request.path);
                if (body_off > 0) {
                    request.body = req_buf + body_off;
                    if (total_len > body_off) {
                        request.body_len = (uint32_t)(total_len - body_off);
                    }
                }
                request.user_ctx = ctx->user_ctx;
                (void)ctx->on_request(&request, ctx->user_ctx);
            }
        } while (0);

        tal_net_close(client_fd);
        if (ctx->on_idle != NULL) {
            ctx->on_idle((HTTP_HOST_HANDLE_T)ctx, ctx->user_ctx);
        }
        if ((ctx->stop_requested == TRUE) || (ctx->shutdown_requested == TRUE)) {
            break;
        }
    }

__EXIT:
    __http_host_close_listen(ctx);
    if (req_buf != NULL) {
        tal_free(req_buf);
    }
    ctx->thread_running = FALSE;
    tal_thread_delete(self_thread);
    tal_free(ctx);
}
