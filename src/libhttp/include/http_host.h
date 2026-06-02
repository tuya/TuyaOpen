/**
 * @file http_host.h
 * @brief Thin embedded HTTP host (listen, receive, dispatch, reply).
 * @version 1.0
 * @date 2026-05-12
 * @copyright Copyright (c) 2021-2026 Tuya Inc.
 */
#ifndef __HTTP_HOST_H__
#define __HTTP_HOST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define HTTP_HOST_METHOD_MAX_LEN 12
#define HTTP_HOST_PATH_MAX_LEN   96

/***********************************************************
*********************typedef define*************************
***********************************************************/
typedef void *HTTP_HOST_HANDLE_T;

typedef struct {
    uint16_t port;
    uint8_t backlog;
    uint32_t listen_recv_timeout_ms;
    uint32_t listen_send_timeout_ms;
    uint32_t client_recv_slice_ms;
    uint32_t client_send_timeout_ms;
    uint16_t client_wait_max_rounds;
    uint32_t max_request_size;
    uint32_t stack_depth;
    uint8_t priority;
    const char *thread_name;
} HTTP_HOST_CFG_T;

typedef struct {
    int client_fd;
    const char *raw;
    uint32_t raw_len;
    char method[HTTP_HOST_METHOD_MAX_LEN];
    char path[HTTP_HOST_PATH_MAX_LEN];
    const char *body;
    uint32_t body_len;
    void *user_ctx;
} HTTP_HOST_REQUEST_T;

typedef OPERATE_RET (*HTTP_HOST_REQUEST_CB)(const HTTP_HOST_REQUEST_T *req, void *user_ctx);
typedef void (*HTTP_HOST_IDLE_CB)(HTTP_HOST_HANDLE_T host, void *user_ctx);

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Start HTTP host listener thread.
 * @param[out] host output host handle.
 * @param[in] cfg host configuration; zero fields use defaults.
 * @param[in] on_request request callback.
 * @param[in] on_idle idle callback on accept timeout.
 * @param[in] user_ctx user context passed to callbacks.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_host_start(HTTP_HOST_HANDLE_T *host, const HTTP_HOST_CFG_T *cfg, HTTP_HOST_REQUEST_CB on_request,
                            HTTP_HOST_IDLE_CB on_idle, void *user_ctx);

/**
 * @brief Stop HTTP host and release resources.
 * @param[in] host host handle.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_host_stop(HTTP_HOST_HANDLE_T host);

/**
 * @brief Close listener and exit accept loop without freeing host context.
 * @param[in] host host handle.
 * @return OPRT_OK on success.
 * @note Safe to call from idle callback before network teardown.
 */
OPERATE_RET http_host_request_shutdown(HTTP_HOST_HANDLE_T host);

/**
 * @brief Send HTTP response with Connection: close.
 * @param[in] client_fd client socket descriptor.
 * @param[in] status HTTP status line text (e.g. "200 OK").
 * @param[in] content_type Content-Type header value.
 * @param[in] body response body.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_host_reply(int client_fd, const char *status, const char *content_type, const char *body);

/**
 * @brief Send 404 Not Found response.
 * @param[in] client_fd client socket descriptor.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_host_reply_not_found(int client_fd);

/**
 * @brief Send HTTP 302 redirect response with Connection: close.
 * @param[in] client_fd client socket descriptor.
 * @param[in] location redirect target URL.
 * @return OPRT_OK on success.
 */
OPERATE_RET http_host_reply_redirect(int client_fd, const char *location);

/**
 * @brief Find HTTP header end offset in request buffer.
 * @param[in] req request buffer.
 * @param[in] req_len request length.
 * @return header end offset, or -1 if not found.
 */
int http_host_find_header_end(const char *req, uint32_t req_len);

/**
 * @brief Parse Content-Length from request headers.
 * @param[in] req request buffer.
 * @return content length if present, -1 otherwise.
 */
int http_host_get_content_length(const char *req);

/**
 * @brief Get header value from request headers (case-insensitive name).
 * @param[in] req request buffer.
 * @param[in] req_len request length.
 * @param[in] name header name.
 * @param[out] out output buffer.
 * @param[in] out_len output buffer size.
 * @return OPRT_OK on success, OPRT_NOT_FOUND if missing.
 */
OPERATE_RET http_host_get_header_value(const char *req, uint32_t req_len, const char *name, char *out, uint32_t out_len);

#ifdef __cplusplus
}
#endif

#endif /* __HTTP_HOST_H__ */
