/**
 * @file http_manager.c
 * @brief http_manager module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "http_manager.h"

#include <stdbool.h>
#include <string.h>
#include <strings.h>

#include "http_client_interface.h"
#include "http_parser.h"
#include "iotdns.h"
#include "tal_api.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tuya_error_code.h"
#include "transport_interface.h"
#include "core_http_client.h"
#include "tuya_transporter.h"
#include "tuya_tls.h"

#include "http_port_internal.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define HTTP_TIMEOUT_MS_DEFAULT (60 * 1000)

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define HTTP_MEMORY_MALLOC tal_psram_malloc
#define HTTP_MEMORY_FREE   tal_psram_free
#else
#define HTTP_MEMORY_MALLOC tal_malloc
#define HTTP_MEMORY_FREE   tal_free
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
static S_HTTP_MANAGER s_http_manager = {0};

/***********************************************************
********************function declaration********************
***********************************************************/
static void *http_mem_calloc(size_t count, size_t size)
{
    size_t total = count * size;
    void *ptr = HTTP_MEMORY_MALLOC(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

static inline void http_safe_free(void *ptr)
{
    if (ptr) {
        HTTP_MEMORY_FREE(ptr);
    }
}

static SESSION_ID http_session_create_internal(const char *url, BOOL_T is_persistent);
static SESSION_ID http_session_create(const char *url, BOOL_T is_persistent);
static SESSION_ID http_session_create_tls(const char *url, BOOL_T is_persistent, tuya_tls_config_t *config);
static OPERATE_RET http_session_destroy(SESSION_ID id);
static OPERATE_RET http_session_send(const SESSION_ID session, const http_req_t *req, http_hdr_field_sel_t field_flags);
static OPERATE_RET http_session_receive(SESSION_ID session, http_resp_t **resp);
static OPERATE_RET http_session_receive_data(SESSION_ID session, http_resp_t *pResp, uint8_t **pDataOut);
static OPERATE_RET http_parse_url(const char *url, char **host, char **path, uint16_t *port, BOOL_T *use_tls);
static const char *http_method_to_string(http_method_t method);
static void http_session_ctx_reset_response(http_session_ctx_t *ctx);

/***********************************************************
***********************function define**********************
***********************************************************/
S_HTTP_MANAGER *get_http_manager_instance(VOID_T)
{
    if (!s_http_manager.inited) {
        memset(&s_http_manager, 0, sizeof(s_http_manager));
        if (tal_mutex_create_init(&s_http_manager.mutex) != OPRT_OK) {
            PR_ERR("create http manager mutex failed");
            return NULL;
        }

        s_http_manager.inited = true;
        s_http_manager.create_http_session = http_session_create;
        s_http_manager.create_http_session_tls = http_session_create_tls;
        s_http_manager.send_http_request = http_session_send;
        s_http_manager.receive_http_response = http_session_receive;
        s_http_manager.destory_http_session = http_session_destroy;
        s_http_manager.receive_http_data = http_session_receive_data;
    }

    return &s_http_manager;
}

static SESSION_ID http_session_create_internal(const char *url, BOOL_T is_persistent)
{
    if (NULL == url) {
        return NULL;
    }

    http_session_ctx_t *ctx = http_mem_calloc(1, sizeof(http_session_ctx_t));
    if (!ctx) {
        PR_ERR("alloc session ctx failed");
        return NULL;
    }

    size_t url_len = strlen(url);
    ctx->url = HTTP_MEMORY_MALLOC(url_len + 1);
    if (!ctx->url) {
        PR_ERR("alloc session url failed");
        HTTP_MEMORY_FREE(ctx);
        return NULL;
    }
    memcpy(ctx->url, url, url_len);
    ctx->url[url_len] = '\0';

    S_HTTP_SESSION *session = http_mem_calloc(1, sizeof(S_HTTP_SESSION));
    if (!session) {
        PR_ERR("alloc session failed");
        http_safe_free(ctx->url);
        HTTP_MEMORY_FREE(ctx);
        return NULL;
    }

    session->s = (http_session_t)ctx;
    session->is_persistent = is_persistent;
    session->state = HTTP_DISCONNECT;
    strncpy(session->url, url, MAX_HTTP_URL_LEN - 1);
    session->url[MAX_HTTP_URL_LEN - 1] = '\0';

    tal_mutex_lock(s_http_manager.mutex);
    int idx = -1;
    for (int i = 0; i < MAX_HTTP_SESSION_NUM; ++i) {
        if (NULL == s_http_manager.session[i]) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        tal_mutex_unlock(s_http_manager.mutex);
        PR_ERR("no free session slot");
        http_safe_free(ctx->url);
        HTTP_MEMORY_FREE(ctx);
        HTTP_MEMORY_FREE(session);
        return NULL;
    }

    s_http_manager.session[idx] = session;
    tal_mutex_unlock(s_http_manager.mutex);

    return session;
}

static SESSION_ID http_session_create_tls(const char *url, BOOL_T is_persistent, tuya_tls_config_t *config)
{
    (void)config;
    return http_session_create_internal(url, is_persistent);
}

static SESSION_ID http_session_create(const char *url, BOOL_T is_persistent)
{
    return http_session_create_internal(url, is_persistent);
}

static void http_session_ctx_reset_response(http_session_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    /* Cleanup legacy response */
    if (ctx->response.buffer || ctx->response.body) {
        http_client_free(&ctx->response);
        memset(&ctx->response, 0, sizeof(ctx->response));
    }

    /* Cleanup streaming mode resources */
    if (ctx->streaming_mode) {
        /* Close and destroy network transport */
        if (ctx->network) {
            tuya_transporter_close(ctx->network);
            tuya_transporter_destroy(ctx->network);
            ctx->network = NULL;
        }

        /* Free header buffer */
        if (ctx->header_buffer) {
            HTTP_MEMORY_FREE(ctx->header_buffer);
            ctx->header_buffer = NULL;
        }

        /* Reset HTTP response */
        memset(&ctx->http_response, 0, sizeof(HTTPResponse_t));
        memset(&ctx->transport, 0, sizeof(TransportInterface_t));
        memset(&ctx->request_headers, 0, sizeof(HTTPRequestHeaders_t));

        ctx->streaming_mode = false;
        ctx->total_body_length = 0;
        ctx->bytes_read = 0;
    }

    ctx->resp_info.content_length = 0;
    ctx->resp_info.status_code = 0;
    ctx->resp_info.chunked = false;
    ctx->resp_info.keep_alive_ack = false;
    ctx->read_offset = 0;
    ctx->response_ready = false;

    http_safe_free(ctx->host);
    ctx->host = NULL;
    http_safe_free(ctx->path);
    ctx->path = NULL;
}

static OPERATE_RET http_session_destroy(SESSION_ID id)
{
    if (!id) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(s_http_manager.mutex);
    for (int i = 0; i < MAX_HTTP_SESSION_NUM; ++i) {
        if (s_http_manager.session[i] == id) {
            s_http_manager.session[i] = NULL;
            tal_mutex_unlock(s_http_manager.mutex);

            http_session_ctx_t *ctx = (http_session_ctx_t *)id->s;
            http_session_ctx_reset_response(ctx);
            http_safe_free(ctx->url);
            HTTP_MEMORY_FREE(ctx);
            HTTP_MEMORY_FREE(id);
            return OPRT_OK;
        }
    }

    tal_mutex_unlock(s_http_manager.mutex);
    return OPRT_NOT_FOUND;
}

static const char *http_method_to_string(http_method_t method)
{
    switch (method) {
    case HTTP_GET:
        return "GET";
    case HTTP_POST:
        return "POST";
    case HTTP_PUT:
        return "PUT";
    case HTTP_DELETE:
        return "DELETE";
    case HTTP_HEAD:
        return "HEAD";
    case HTTP_OPTIONS:
        return "OPTIONS";
    case HTTP_TRACE:
        return "TRACE";
    case HTTP_CONNECT:
        return "CONNECT";
    default:
        return NULL;
    }
}

static OPERATE_RET http_parse_url(const char *url, char **host, char **path, uint16_t *port, BOOL_T *use_tls)
{
    if (!url || !host || !path || !port || !use_tls) {
        return OPRT_INVALID_PARM;
    }

    struct http_parser_url parsed;
    http_parser_url_init(&parsed);
    if (http_parser_parse_url(url, strlen(url), 0, &parsed) != 0) {
        PR_ERR("parse url failed: %s", url);
        return OPRT_INVALID_PARM;
    }

    size_t schema_len = parsed.field_data[UF_SCHEMA].len;
    *use_tls = false;
    if (schema_len) {
        const char *schema = url + parsed.field_data[UF_SCHEMA].off;
        if ((schema_len == strlen("https")) && (strncasecmp(schema, "https", schema_len) == 0)) {
            *use_tls = true;
        }
    }

    size_t host_len = parsed.field_data[UF_HOST].len;
    if (!host_len) {
        PR_ERR("url host missing");
        return OPRT_INVALID_PARM;
    }

    *host = http_mem_calloc(1, host_len + 1);
    if (!*host) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(*host, url + parsed.field_data[UF_HOST].off, host_len);

    size_t path_len = parsed.field_data[UF_PATH].len;
    size_t query_len = parsed.field_data[UF_QUERY].len;
    size_t total_len = (path_len ? path_len : 1) + (query_len ? (query_len + 1) : 0);
    *path = http_mem_calloc(1, total_len + 1);
    if (!*path) {
        http_safe_free(*host);
        *host = NULL;
        return OPRT_MALLOC_FAILED;
    }

    size_t offset = 0;
    if (path_len) {
        memcpy(*path, url + parsed.field_data[UF_PATH].off, path_len);
        offset = path_len;
    } else {
        (*path)[0] = '/';
        offset = 1;
    }

    if (query_len) {
        (*path)[offset++] = '?';
        memcpy(*path + offset, url + parsed.field_data[UF_QUERY].off, query_len);
    }

    *port = parsed.port ? parsed.port : (*use_tls ? 443 : 80);

    return OPRT_OK;
}

static OPERATE_RET http_session_send(const SESSION_ID session, const http_req_t *req, http_hdr_field_sel_t field_flags)
{
    (void)field_flags;
    if (!session || !req || !req->resource) {
        return OPRT_INVALID_PARM;
    }

    http_session_ctx_t *ctx = (http_session_ctx_t *)session->s;
    if (!ctx) {
        return OPRT_INVALID_PARM;
    }

    /* Clean up any previous session state before starting new request */
    http_session_ctx_reset_response(ctx);

    char *host = NULL;
    char *path = NULL;
    uint16_t port = 0;
    BOOL_T use_tls = false;

    OPERATE_RET rt = http_parse_url(req->resource, &host, &path, &port, &use_tls);
    if (OPRT_OK != rt) {
        return rt;
    }

    const char *method = http_method_to_string(req->type);
    if (!method) {
        http_safe_free(host);
        http_safe_free(path);
        return OPRT_INVALID_PARM;
    }

    /* Store host and path in context for cleanup */
    ctx->host = host;
    ctx->path = path;
    ctx->port = port;
    ctx->use_tls = use_tls;

    /* Create and connect network transport */
    TUYA_TRANSPORT_TYPE_E transport_type = use_tls ? TRANSPORT_TYPE_TLS : TRANSPORT_TYPE_TCP;
    ctx->network = tuya_transporter_create(transport_type, NULL);
    if (!ctx->network) {
        PR_ERR("Failed to create transporter");
        http_safe_free(host);
        http_safe_free(path);
        return OPRT_MALLOC_FAILED;
    }

    /* Configure TLS if needed */
    if (use_tls) {
        uint8_t *cacert = NULL;
        uint16_t cacert_len = 0;
        rt = tuya_iotdns_query_domain_certs(req->resource ? (char *)req->resource : ctx->url, &cacert, &cacert_len);
        if (OPRT_OK != rt) {
            tuya_transporter_destroy(ctx->network);
            ctx->network = NULL;
            http_safe_free(host);
            http_safe_free(path);
            return rt;
        }

        tuya_tls_config_t tls_config = {
            .ca_cert = (char *)cacert,
            .ca_cert_size = cacert_len,
            .hostname = host,
            .port = port,
            .timeout = HTTP_TIMEOUT_MS_DEFAULT,
            .mode = TUYA_TLS_SERVER_CERT_MODE,
            .verify = true,
        };

        rt = tuya_transporter_ctrl(ctx->network, TUYA_TRANSPORTER_SET_TLS_CONFIG, &tls_config);
        http_safe_free(cacert);

        if (OPRT_OK != rt) {
            PR_ERR("Failed to set TLS config: %d", rt);
            tuya_transporter_destroy(ctx->network);
            ctx->network = NULL;
            http_safe_free(host);
            http_safe_free(path);
            return rt;
        }
    }

    /* Connect to server */
    rt = tuya_transporter_connect(ctx->network, host, port, HTTP_TIMEOUT_MS_DEFAULT);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to connect to server: %d", rt);
        tuya_transporter_close(ctx->network);
        tuya_transporter_destroy(ctx->network);
        ctx->network = NULL;
        http_safe_free(host);
        http_safe_free(path);
        return rt;
    }

    /* Setup transport interface for coreHTTP */
    ctx->transport.pNetworkContext = &ctx->network;
    ctx->transport.send = (TransportSend_t)NetworkTransportSend;
    ctx->transport.recv = (TransportRecv_t)NetworkTransportRecv;

    /* Allocate header buffer */
    ctx->header_buffer = HTTP_MEMORY_MALLOC(512);
    if (!ctx->header_buffer) {
        PR_ERR("Failed to allocate header buffer");
        tuya_transporter_close(ctx->network);
        tuya_transporter_destroy(ctx->network);
        ctx->network = NULL;
        http_safe_free(host);
        http_safe_free(path);
        return OPRT_MALLOC_FAILED;
    }

    /* Setup HTTP request */
    HTTPRequestInfo_t requestInfo = {
        .pHost = host,
        .hostLen = strlen(host),
        .pMethod = method,
        .methodLen = strlen(method),
        .pPath = path,
        .pathLen = strlen(path),
        .reqFlags = 0,
    };

    /* Initialize request headers */
    ctx->request_headers.bufferLen = 512;
    ctx->request_headers.pBuffer = ctx->header_buffer;

    HTTPStatus_t httpStatus = HTTPClient_InitializeRequestHeaders(&ctx->request_headers, &requestInfo);
    if (httpStatus != HTTPSuccess) {
        PR_ERR("Failed to initialize request headers: %d", httpStatus);
        http_safe_free(ctx->header_buffer);
        ctx->header_buffer = NULL;
        tuya_transporter_close(ctx->network);
        tuya_transporter_destroy(ctx->network);
        ctx->network = NULL;
        http_safe_free(host);
        http_safe_free(path);
        return OPRT_COM_ERROR;
    }

    /* Initialize HTTP response structure */
    memset(&ctx->http_response, 0, sizeof(HTTPResponse_t));
    ctx->http_response.pBuffer = ctx->header_buffer;
    ctx->http_response.bufferLen = 512;

    /* Send request with HTTP_SEND_DISABLE_RECV_BODY_FLAG to only receive headers */
    httpStatus = HTTPClient_Request(&ctx->transport, &ctx->request_headers, (const uint8_t *)req->content,
                                    (req->content && req->content_len > 0) ? (size_t)req->content_len : 0,
                                    &ctx->http_response, HTTP_SEND_DISABLE_RECV_BODY_FLAG);

    if (httpStatus != HTTPSuccess) {
        PR_ERR("HTTPClient_Request failed: %d", httpStatus);
        http_safe_free(ctx->header_buffer);
        ctx->header_buffer = NULL;
        tuya_transporter_close(ctx->network);
        tuya_transporter_destroy(ctx->network);
        ctx->network = NULL;
        http_safe_free(host);
        http_safe_free(path);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    /* Setup response info (don't call http_session_ctx_reset_response as it would cleanup our streaming resources) */
    ctx->resp_info.status_code = ctx->http_response.statusCode;
    ctx->resp_info.version = HTTP_VER_1_1;
    ctx->resp_info.chunked = (ctx->http_response.respFlags & HTTP_RESPONSE_CONNECTION_CLOSE_FLAG) ? false : false;
    ctx->resp_info.content_length = ctx->http_response.contentLength;
    ctx->resp_info.keep_alive_ack =
        (ctx->http_response.respFlags & HTTP_RESPONSE_CONNECTION_KEEP_ALIVE_FLAG) ? true : false;

    /* Enable streaming mode */
    ctx->streaming_mode = true;
    ctx->total_body_length = ctx->http_response.contentLength;
    ctx->bytes_read = 0;
    ctx->response_ready = true;

    session->state = HTTP_UPLOADING;
    memcpy(&session->req, req, sizeof(http_req_t));

    return OPRT_OK;
}

static OPERATE_RET http_session_receive(SESSION_ID session, http_resp_t **resp)
{
    if (!session || !resp) {
        return OPRT_INVALID_PARM;
    }

    http_session_ctx_t *ctx = (http_session_ctx_t *)session->s;
    if (!ctx || !ctx->response_ready) {
        return OPRT_COM_ERROR;
    }

    *resp = &ctx->resp_info;
    session->state = HTTP_CONNECTED;
    return OPRT_OK;
}

static OPERATE_RET http_session_receive_data(SESSION_ID session, http_resp_t *pResp, uint8_t **pDataOut)
{
    if (!session || !pResp || !pDataOut) {
        return OPRT_INVALID_PARM;
    }

    http_session_ctx_t *ctx = (http_session_ctx_t *)session->s;
    if (!ctx || !ctx->response_ready || !ctx->response.body) {
        return OPRT_COM_ERROR;
    }

    size_t total = ctx->response.body_length;
    uint8_t *buf = HTTP_MEMORY_MALLOC(total + 1);
    if (!buf) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(buf, ctx->response.body, total);
    buf[total] = '\0';

    *pDataOut = buf;
    (void)pResp;
    return OPRT_OK;
}
