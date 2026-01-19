/**
 * @file http_session.h
 * @brief HTTP Session API for persistent HTTP connections
 * @version 0.1
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __HTTP_SESSION_H__
#define __HTTP_SESSION_H__

#include "tuya_cloud_types.h"
#include "core_http_client.h"
#include "http_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef void * http_session_t;

// HTTP version
typedef enum {
    HTTP_VER_1_0 = 0,
    HTTP_VER_1_1 = 1,
} http_ver_t;

// HTTP redirect check macro
#define HTTP_RESP_REDIR(code) ((code) >= 300 && (code) < 400)

// HTTP request structure
typedef struct {
    enum http_method type;        // HTTP method (HTTP_GET, HTTP_POST, etc.)
    http_ver_t version;            // HTTP version
    const char *content;          // Request body content
    int content_len;              // Content length
    unsigned int download_offset;  // Range request offset
    unsigned int download_size;   // Range request size
} http_req_t;

// HTTP response structure
typedef struct {
    const char *protocol;         // Protocol string (e.g., "HTTP")
    http_ver_t version;            // HTTP version
    int status_code;              // HTTP status code (200, 404, etc.)
    const char *location;          // Location header (for redirects)
    const char *server;            // Server header
    const char *content_type;      // Content-Type header
    const char *content_encoding;  // Content-Encoding header
    bool_t keep_alive_ack;         // Keep-Alive acknowledgment
    int keep_alive_timeout;       // Keep-Alive timeout
    int keep_alive_max;            // Keep-Alive max requests
    bool_t chunked;                // Transfer-Encoding: chunked
    unsigned int content_length;   // Content-Length
} http_resp_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Open an HTTP session
 * 
 * @param[out] session Pointer to session handle
 * @param[in] url URL to connect to
 * @param[in] timeout_ms Connection timeout in milliseconds
 * 
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET http_open_session(http_session_t *session, const char *url, uint32_t timeout_ms);

/**
 * @brief Send HTTP request
 * 
 * @param[in] session Session handle
 * @param[in] req Request structure
 * @param[in] req_flags Request flags
 * 
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET http_send_request(http_session_t session, const http_req_t *req,  uint32_t req_flags);

/**
 * @brief Get HTTP response header
 * 
 * @param[in] session Session handle
 * @param[out] response Pointer to response structure (allocated by function)
 * 
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET http_get_response_hdr(http_session_t session, http_resp_t **response);

/**
 * @brief Free HTTP response header structure
 * 
 * This function frees the memory allocated by http_get_response_hdr.
 * Call this function when you no longer need the response header.
 * 
 * @param[in] response Pointer to response structure (set to NULL on success)
 * 
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET http_free_response_hdr(http_resp_t **response);

/**
 * @brief Close HTTP session
 * 
 * @param[in,out] session Pointer to session handle (set to NULL on success)
 * 
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET http_close_session(http_session_t *session);

/**
 * @brief Set session timeout
 * 
 * @param[in] session Session handle
 * @param[in] timeout_ms Timeout in milliseconds
 * 
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET http_set_timeout(http_session_t session, uint32_t timeout_ms);


/**
 * @brief Read content from HTTP response
 * 
 * @param[in] session Session handle
 * @param[out] buf Buffer to store read data
 * @param[in] max_len Maximum length to read
 * 
 * @return Number of bytes read (>0), 0 on EOF, negative on error
 */
int http_read_content(http_session_t session, void *buf, unsigned int max_len);

#ifdef __cplusplus
}
#endif

#endif /* __HTTP_SESSION_H__ */

