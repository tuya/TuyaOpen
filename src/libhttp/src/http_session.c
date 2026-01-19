/**
 * @file http_session.c
 * @brief HTTP Session implementation for persistent HTTP connections
 * @version 0.1
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 */
#include <string.h>
#include <stdlib.h>

#include "tal_api.h"
#include "tuya_transporter.h"
#include "transport_interface.h"
#include "tuya_tls.h"
#include "core_http_client.h"
#include "iotdns.h"
#include "http_session.h"

#define DEFAULT_HTTP_PORT    (80)
#define DEFAULT_HTTPS_PORT   (443)
#define DEFAULT_TIMEOUT_MS   (5000)
#define MAX_URL_LEN          (1024)
#define HEADER_BUFFER_LENGTH (2048)


#define HTTP

// HTTP session context
typedef struct {
    char *url;                      // Original URL
    char *host;                     // Parsed host
    char *path;                     // Parsed path
    uint16_t port;                  // Port number
    bool use_tls;                   // Use TLS/HTTPS
    uint8_t *cacert;                // CA certificate (for TLS, set by user)
    uint16_t cacert_len;            // CA certificate length
    NetworkContext_t network;        // Network transport context
    TransportInterface_t transport;  // Transport interface
    HTTPResponse_t response;        // HTTP response
    bool response_ready;            // Response received flag
    uint32_t timeout_ms;            // Session timeout
    bool connected;                 // Connection state
    size_t total_body_length;       // Total body length (from Content-Length)
    size_t bytes_read;              // Bytes read from response body
} http_session_ctx_t;

// Helper function to parse URL
static OPERATE_RET http_parse_url(const char *url, char **host, char **path, uint16_t *port, bool *use_tls)
{
    if (!url || !host || !path || !port || !use_tls) {
        return OPRT_INVALID_PARM;
    }

    *use_tls = false;
    *port = DEFAULT_HTTP_PORT;

    // Check for https://
    if (strncmp(url, "https://", 8) == 0) {
        *use_tls = true;
        *port = DEFAULT_HTTPS_PORT;
        url += 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        url += 7;
    } else {
        // Assume http:// if no scheme
        *port = DEFAULT_HTTP_PORT;
    }

    // Find host
    const char *host_start = url;
    const char *host_end = strchr(url, '/');
    if (!host_end) {
        host_end = strchr(url, ':');
        if (!host_end) {
            host_end = url + strlen(url);
        }
    }

    // Check for port in host
    const char *port_start = strchr(host_start, ':');
    if (port_start && port_start < host_end) {
        size_t host_len = port_start - host_start;
        *host = HTTP_MALLOC(host_len + 1);
        if (!*host) {
            return OPRT_MALLOC_FAILED;
        }
        memcpy(*host, host_start, host_len);
        (*host)[host_len] = '\0';

        // Parse port
        *port = (uint16_t)atoi(port_start + 1);
    } else {
        size_t host_len = host_end - host_start;
        *host = HTTP_MALLOC(host_len + 1);
        if (!*host) {
            return OPRT_MALLOC_FAILED;
        }
        memcpy(*host, host_start, host_len);
        (*host)[host_len] = '\0';
    }

    // Get path
    if (*host_end == '/') {
        *path = HTTP_MALLOC(strlen(host_end) + 1);
        if (!*path) {
            HTTP_FREE(*host);
            return OPRT_MALLOC_FAILED;
        }
        strcpy(*path, host_end);
    } else {
        *path = HTTP_MALLOC(2);
        if (!*path) {
            HTTP_FREE(*host);
            return OPRT_MALLOC_FAILED;
        }
        strcpy(*path, "/");
    }

    return OPRT_OK;
}

// Helper function to parse response headers
static OPERATE_RET http_parse_response_headers(const uint8_t *headers, size_t headers_len, http_resp_t *resp_info)
{
    if (!headers || !resp_info) {
        return OPRT_INVALID_PARM;
    }

    resp_info->version = HTTP_VER_1_1;
    resp_info->protocol = "HTTP";

    // Simple header parsing (can be enhanced)
    // Note: We need to keep a copy of headers for parsing, but string pointers
    // should point to the original headers buffer, not the temporary copy
    char *header_str = HTTP_MALLOC(headers_len + 1);
    if (!header_str) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(header_str, headers, headers_len);
    header_str[headers_len] = '\0';

    // Note: pHeaders only contains header fields, not the status line
    // Status line (e.g., "HTTP/1.1 200 OK") is already parsed by coreHTTP
    // and stored in ctx->response.statusCode. We'll use that value instead.
    // So we skip status line parsing here and go directly to header fields.

    // Parse headers (pHeaders contains only header fields, not status line)
    char *line = strtok(header_str, "\r\n");
    while (line != NULL) {
        if (strncasecmp(line, "Location:", 9) == 0) {
            // Calculate offset in original headers buffer
            size_t offset = (line + 9) - header_str;
            resp_info->location = (const char *)(headers + offset);
            while (*resp_info->location == ' ') {
                offset++;
                resp_info->location = (const char *)(headers + offset);
            }
        } else if (strncasecmp(line, "Server:", 7) == 0) {
            size_t offset = (line + 7) - header_str;
            resp_info->server = (const char *)(headers + offset);
            while (*resp_info->server == ' ') {
                offset++;
                resp_info->server = (const char *)(headers + offset);
            }
        } else if (strncasecmp(line, "Content-Type:", 13) == 0) {
            size_t offset = (line + 13) - header_str;
            resp_info->content_type = (const char *)(headers + offset);
            while (*resp_info->content_type == ' ') {
                offset++;
                resp_info->content_type = (const char *)(headers + offset);
            }
        } else if (strncasecmp(line, "Content-Encoding:", 17) == 0) {
            size_t offset = (line + 17) - header_str;
            resp_info->content_encoding = (const char *)(headers + offset);
            while (*resp_info->content_encoding == ' ') {
                offset++;
                resp_info->content_encoding = (const char *)(headers + offset);
            }
        } else if (strncasecmp(line, "Content-Length:", 15) == 0) {
            resp_info->content_length = (unsigned int)atoi(line + 15);
        } else if (strncasecmp(line, "Transfer-Encoding:", 18) == 0) {
            if (strstr(line, "chunked")) {
                resp_info->chunked = true;
            }
        } else if (strncasecmp(line, "Connection:", 10) == 0) {
            // Connection: Keep-Alive indicates server supports persistent connection
            if (strstr(line, "keep-alive") || strstr(line, "Keep-Alive")) {
                resp_info->keep_alive_ack = true;
            }
        } else if (strncasecmp(line, "Keep-Alive:", 11) == 0) {
            // Parse "Keep-Alive: timeout=5, max=1000"
            // This header only provides parameters, not the Keep-Alive acknowledgment
            char *timeout_str = strstr(line, "timeout=");
            if (timeout_str) {
                timeout_str += 8; // Skip "timeout="
                resp_info->keep_alive_timeout = atoi(timeout_str);
            }
            char *max_str = strstr(line, "max=");
            if (max_str) {
                max_str += 4; // Skip "max="
                resp_info->keep_alive_max = atoi(max_str);
            }
        }
        
        // Get next line
        line = strtok(NULL, "\r\n");
    }

    HTTP_FREE(header_str);
    return OPRT_OK;
}

// Internal function to establish connection
static OPERATE_RET http_connect_session_internal(http_session_ctx_t *ctx)
{
    if (!ctx) {
        return OPRT_INVALID_PARM;
    }

    if (ctx->connected) {
        return OPRT_OK; // Already connected
    }

    OPERATE_RET rt;

    // Setup TLS if needed
    if (ctx->use_tls) {
        tuya_iotdns_query_domain_certs(ctx->host, &ctx->cacert, &ctx->cacert_len);

        if (!ctx->cacert || ctx->cacert_len == 0) {
            PR_ERR("CA certificate not set for TLS connection");
            return OPRT_INVALID_PARM;
        }
        
        tuya_tls_config_t tls_config = {
            .ca_cert = (char *)ctx->cacert,
            .ca_cert_size = ctx->cacert_len,
            .hostname = ctx->host,
            .port = ctx->port,
            .timeout = ctx->timeout_ms,
            .mode = TUYA_TLS_SERVER_CERT_MODE,
            .verify = true,
        };

        rt = tuya_transporter_ctrl(ctx->network, TUYA_TRANSPORTER_SET_TLS_CONFIG, &tls_config);
        if (OPRT_OK != rt) {
            PR_ERR("tls config failed:%d", rt);
            return rt;
        }
    }else {
        tuya_tcp_config_t config = {0};
        memset(&config, 0, sizeof(tuya_tcp_config_t));
        config.sendTimeoutMs = ctx->timeout_ms;// add http send timeout to prevent from blocking
        config.bindAddr = 0;
        rt = tuya_transporter_ctrl(ctx->network, TUYA_TRANSPORTER_SET_TCP_CONFIG, &config); 
        if (OPRT_OK != rt) {
            PR_ERR("tcp config failed:%d", rt);
            return rt;
        }
    }

    // Connect
    if (ctx->use_tls) {
        tuya_tls_config_t tls_config = {
            .hostname = ctx->host,
            .port = ctx->port,
            .timeout = ctx->timeout_ms,
        };
        rt = tuya_transporter_connect(ctx->network, tls_config.hostname, tls_config.port, tls_config.timeout);
    } else {
        rt = tuya_transporter_connect(ctx->network, ctx->host, ctx->port, ctx->timeout_ms);
    }

    if (OPRT_OK != rt) {
        PR_ERR("connect failed:%d", rt);
        return rt;
    }

    ctx->connected = true;

    // Setup transport interface
    ctx->transport.pNetworkContext = (NetworkContext_t *)&ctx->network;
    ctx->transport.recv = (TransportRecv_t)NetworkTransportRecv;
    ctx->transport.send = (TransportSend_t)NetworkTransportSend;

    if (ctx->use_tls) {
        PR_DEBUG("tls connected!");
    }

    return OPRT_OK;
}

OPERATE_RET http_open_session(http_session_t *session, const char *url, uint32_t timeout_ms)
{
    if (!session || !url) {
        return OPRT_INVALID_PARM;
    }

    http_session_ctx_t *ctx = HTTP_MALLOC(sizeof(http_session_ctx_t));
    if (!ctx) {
        return OPRT_MALLOC_FAILED;
    }
    memset(ctx, 0, sizeof(http_session_ctx_t));

    ctx->url = HTTP_MALLOC(strlen(url) + 1);
    if (!ctx->url) {
        HTTP_FREE(ctx);
        return OPRT_MALLOC_FAILED;
    }
    memset(ctx->url, 0, strlen(url) + 1);
    strcpy(ctx->url, url);

    ctx->timeout_ms = (timeout_ms > 0) ? timeout_ms : DEFAULT_TIMEOUT_MS;

    // Parse URL
    OPERATE_RET rt = http_parse_url(url, &ctx->host, &ctx->path, &ctx->port, &ctx->use_tls);
    if (OPRT_OK != rt) {
        HTTP_FREE(ctx->url);
        HTTP_FREE(ctx);
        return rt;
    }
    
    // Create transport
    TUYA_TRANSPORT_TYPE_E transport_type = ctx->use_tls ? TRANSPORT_TYPE_TLS : TRANSPORT_TYPE_TCP;
    ctx->network = tuya_transporter_create(transport_type, NULL);
    if (!ctx->network) {
        if (ctx->cacert) {
            HTTP_FREE(ctx->cacert);
        }
        HTTP_FREE(ctx->path);
        HTTP_FREE(ctx->host);
        HTTP_FREE(ctx->url);
        HTTP_FREE(ctx);
        return OPRT_MALLOC_FAILED;
    }

    // Establish connection
    rt = http_connect_session_internal(ctx);
    if (OPRT_OK != rt) {
        if (ctx->cacert) {
            HTTP_FREE(ctx->cacert);
        }
        tuya_transporter_destroy(ctx->network);
        HTTP_FREE(ctx->path);
        HTTP_FREE(ctx->host);
        HTTP_FREE(ctx->url);
        HTTP_FREE(ctx);
        return rt;
    }

    *session = (http_session_t)ctx;
    return OPRT_OK;
}

OPERATE_RET http_send_request(http_session_t session, const http_req_t *req, uint32_t req_flags)
{
    if (!session || !req) {
        return OPRT_INVALID_PARM;
    }

    http_session_ctx_t *ctx = (http_session_ctx_t *)session;

    // Check if connection is established
    if (!ctx->connected) {
        PR_ERR("session not connected");
        return OPRT_COM_ERROR;
    }

    // Prepare request
    const char *method_str = "GET";
    switch (req->type) {
        case HTTP_GET: method_str = "GET"; break;
        case HTTP_POST: method_str = "POST"; break;
        case HTTP_PUT: method_str = "PUT"; break;
        case HTTP_DELETE: method_str = "DELETE"; break;
        case HTTP_HEAD: method_str = "HEAD"; break;
        default: method_str = "GET"; break;
    }

    HTTPRequestInfo_t requestInfo = {
        .pMethod = method_str,
        .methodLen = strlen(method_str),
        .pHost = ctx->host,
        .hostLen = strlen(ctx->host),
        .pPath = ctx->path,
        .pathLen = strlen(ctx->path),
        .reqFlags = req_flags,
    };

    // Prepare request headers
    HTTPRequestHeaders_t requestHeaders;
    memset(&requestHeaders, 0, sizeof(requestHeaders));
    requestHeaders.bufferLen = HEADER_BUFFER_LENGTH;
    requestHeaders.pBuffer = HTTP_MALLOC(requestHeaders.bufferLen);
    if (!requestHeaders.pBuffer) {
        return OPRT_MALLOC_FAILED;
    }

    HTTPStatus_t httpStatus = HTTPClient_InitializeRequestHeaders(&requestHeaders, &requestInfo);
    if (httpStatus != HTTPSuccess) {
        PR_ERR("HTTP header init error:%d", httpStatus);
        HTTP_FREE(requestHeaders.pBuffer);
        return OPRT_COM_ERROR;
    }

    // Check if this is a range request (for partial download)
    bool is_range_request = (req->download_size > 0 || req->download_offset > 0);
    
    if(is_range_request) {
        int32_t range_start = req->download_offset;
        int32_t range_end = (req->download_size > 0) ? \
                             req->download_offset + req->download_size - 1 :\
                             HTTP_RANGE_REQUEST_END_OF_FILE;

        httpStatus = HTTPClient_AddRangeHeader(&requestHeaders, range_start, range_end);
        if (httpStatus != HTTPSuccess) {
            PR_ERR("HTTP range header add error:%d", httpStatus);
            HTTP_FREE(requestHeaders.pBuffer);
            return OPRT_COM_ERROR;
        }
    } 

    PR_DEBUG("Sending HTTP %s request to %s%s", 
              method_str, ctx->host, ctx->path);

    // Send request
    // Use HTTP_SEND_DISABLE_RECV_BODY_FLAG for streaming mode to only receive headers
    uint32_t send_flags = HTTP_SEND_DISABLE_RECV_BODY_FLAG ;
    httpStatus = HTTPClient_Request(&ctx->transport, &requestHeaders,
                                   (const uint8_t *)req->content, 
                                   (req->content && req->content_len > 0) ? (size_t)req->content_len : 0,
                                   &ctx->response, send_flags);

    HTTP_FREE(requestHeaders.pBuffer);

    if (httpStatus != HTTPSuccess) {
        PR_ERR("Failed to send HTTP %s request to %s%s: Error=%s", method_str, ctx->host, ctx->path, 
               HTTPClient_strerror(httpStatus));
        return OPRT_COM_ERROR;
    }

    PR_DEBUG("Response Headers:\r\n%.*s\r\n",
             (int32_t)ctx->response.headersLen, ctx->response.pHeaders);
  
    PR_DEBUG("Response Status:\r\n%u\r\n", ctx->response.statusCode);
    
    // Get content length from response headers
    ctx->total_body_length = ctx->response.contentLength;
    ctx->bytes_read = 0;
    PR_DEBUG("total_body_length=%zu", ctx->total_body_length);

    ctx->response_ready = true;

    return OPRT_OK;
}

OPERATE_RET http_get_response_hdr(http_session_t session, http_resp_t **response)
{
    if (!session || !response) {
        return OPRT_INVALID_PARM;
    }

    http_session_ctx_t *ctx = (http_session_ctx_t *)session;
    http_resp_t *p_resp_info = NULL;

    if (!ctx->response_ready) {
        return OPRT_COM_ERROR;
    }

    // Allocate response structure
    p_resp_info = HTTP_MALLOC(sizeof(http_resp_t));
    if (p_resp_info == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(p_resp_info, 0, sizeof(http_resp_t));

    // Parse response headers
    http_parse_response_headers(ctx->response.pHeaders, ctx->response.headersLen, p_resp_info);

    // Use status code already parsed by coreHTTP (more reliable than parsing from string)
    p_resp_info->status_code = (int)ctx->response.statusCode;
    PR_DEBUG("status_code=%d", p_resp_info->status_code);

    // For chunked encoding, coreHTTP has already decoded the chunks
    // Use bodyLen as the actual content length
    if (p_resp_info->chunked) {
        // For chunked responses, content_length may be 0, use bodyLen instead
        p_resp_info->content_length = ctx->response.bodyLen;
        PR_DEBUG("Chunked response detected, decoded body length: %zu", ctx->response.bodyLen);
    } else {
        // For non-chunked responses, use contentLength from response headers
        // Note: We use HTTP_SEND_DISABLE_RECV_BODY_FLAG, so bodyLen is not reliable
        // Use contentLength parsed by coreHTTP from Content-Length header
        if (ctx->response.contentLength > 0) {
            p_resp_info->content_length = (unsigned int)ctx->response.contentLength;
        } else if (p_resp_info->content_length == 0) {
            // If both are 0, it might be a response without body (e.g., 204 No Content)
            PR_DEBUG("Content-Length is 0, may be a response without body");
        }
    }

    *response = p_resp_info;
    
    return OPRT_OK;
}

OPERATE_RET http_free_response_hdr(http_resp_t **response)
{
    if (!response || !*response) {
        return OPRT_INVALID_PARM;
    }

    http_resp_t *resp = *response;
    
    // Free the response structure
    // Note: The string pointers (reason_phrase, location, server, etc.)
    // point to locations within the parsed header string, which is freed
    // in http_parse_response_headers. These pointers should not be freed here.
    HTTP_FREE(resp);
    
    *response = NULL;
    
    return OPRT_OK;
}

OPERATE_RET http_close_session(http_session_t *session)
{
    if (!session || !*session) {
        return OPRT_INVALID_PARM;
    }

    http_session_ctx_t *ctx = (http_session_ctx_t *)*session;

    if (ctx->connected) {
        tuya_transporter_close(ctx->network);
        tuya_transporter_destroy(ctx->network);
        ctx->connected = false;
    }

    ctx->response_ready = false;
    ctx->total_body_length = 0;
    ctx->bytes_read = 0;

    // Free response body (allocated by coreHTTP)
    // Note: pBody is always separately allocated in HTTPClient_Request
    // (for chunked: HTTP_MALLOC(HTTP_MAX_RESPONSE_CHUNK_SIZE_BYTES + 1)
    //  for non-chunked: HTTP_MALLOC(contentLength + 1)
    //  for disabled body: HTTP_MALLOC(bodyLen + 1))
    if (ctx->response.pBody) {
        HTTP_FREE((void *)ctx->response.pBody);
        ctx->response.pBody = NULL;
    }
    
    // Free response buffer (allocated by coreHTTP)
    // Note: pBuffer is always allocated by HTTPClient_Request in HTTP_RECV_INIT
    // (HTTP_MALLOC(HTTP_MAX_RESPONSE_HEADERS_SIZE_BYTES + 1))
    if (ctx->response.pBuffer) {
        HTTP_FREE(ctx->response.pBuffer);
        ctx->response.pBuffer = NULL;
    }
    
    // Free CA certificate
    if (ctx->cacert) {
        HTTP_FREE(ctx->cacert);
        ctx->cacert = NULL;
    }

    // Free parsed strings
    if (ctx->url) {
        HTTP_FREE(ctx->url);
    }
    if (ctx->host) {
        HTTP_FREE(ctx->host);
    }
    if (ctx->path) {
        HTTP_FREE(ctx->path);
    }

    HTTP_FREE(ctx);
    *session = NULL;

    return OPRT_OK;
}

OPERATE_RET http_set_timeout(http_session_t session, uint32_t timeout_ms)
{
    if (!session) {
        return OPRT_INVALID_PARM;
    }

    http_session_ctx_t *ctx = (http_session_ctx_t *)session;
    ctx->timeout_ms = timeout_ms;

    // Note: Actual timeout is set during connect/request
    return OPRT_OK;
}

int http_read_content(http_session_t session, void *buf, unsigned int max_len)
{
    if (!session || !buf || max_len == 0) {
        return -1; // Invalid parameter
    }

    http_session_ctx_t *ctx = (http_session_ctx_t *)session;

    if (!ctx->response_ready) {
        return -1; // Response not ready
    }

    // Use HTTPClient_Recv to read data directly from network
    // Response body is not pre-loaded, it's read on-demand via HTTPClient_Recv
    
    // Check if we've already read all data
    if (ctx->total_body_length > 0 && ctx->bytes_read >= ctx->total_body_length) {
        return 0; // EOF
    }

    // Use HTTPClient_Recv to read data directly into caller's buffer
    int32_t bytes_read = HTTPClient_Recv(&ctx->transport, &ctx->response, 
                                         (uint8_t *)buf, (size_t)max_len);

    if (bytes_read < 0) {
        PR_ERR("HTTPClient_Recv failed: %d", bytes_read);
        return -1; // Network error or read failure
    }

    // Update bytes read counter
    ctx->bytes_read += bytes_read;
    // PR_DEBUG("http_read_content read %d total_len %zu all_read %zu", bytes_read, ctx->total_body_length, ctx->bytes_read);

    return (int)bytes_read;
}

