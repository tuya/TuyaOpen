#include "tuya_error_code.h"
#include "http_client_interface.h"
#include "transport_interface.h"
#include "core_http_client.h"
#include "tuya_tls.h"
#include "tal_log.h"

#define log_debug PR_DEBUG
#define log_error PR_ERR

#define HEADER_BUFFER_LENGTH (255)
#define DEFAULT_HTTP_PORT    (80)
#define DEFAULT_HTTPS_PORT   (443)
static http_client_status_t core_http_request_send(const TransportInterface_t *pTransportInterface,
                                                   const HTTPRequestInfo_t *requestInfo, http_client_header_t *headers,
                                                   uint8_t headers_count, const uint8_t *pRequestBodyBuf,
                                                   size_t reqBodyBufLen, HTTPResponse_t *response)
{
    /* Represents header data that will be sent in an HTTP request. */
    HTTPRequestHeaders_t requestHeaders;

    /* Return value of all methods from the HTTP Client library API. */
    HTTPStatus_t httpStatus = HTTPSuccess;

    if (NULL == requestInfo || NULL == response) {
        return HTTP_CLIENT_SERIALIZE_FAULT;
    }
    /* Initialize all HTTP Client library API structs to 0. */
    (void)memset(&requestHeaders, 0, sizeof(requestHeaders));
    /* Set the buffer used for storing request headers. */
    requestHeaders.bufferLen = HEADER_BUFFER_LENGTH + headers_count * 32;
    requestHeaders.pBuffer = tal_malloc(requestHeaders.bufferLen);
    if (requestHeaders.pBuffer == NULL) {
        return HTTP_CLIENT_MALLOC_FAULT;
    }

    httpStatus = HTTPClient_InitializeRequestHeaders(&requestHeaders, requestInfo);
    int i;
    for (i = 0; i < headers_count; i++) {
        log_debug("HTTP header add key=%s", headers[i].key);
        httpStatus |= HTTPClient_AddHeader(&requestHeaders, headers[i].key, strlen(headers[i].key), headers[i].value,
                                           strlen(headers[i].value));
    }

    if (httpStatus != HTTPSuccess) {
        log_error("HTTP header error:%d", httpStatus);
        tal_free(requestHeaders.pBuffer);
        return HTTP_CLIENT_SERIALIZE_FAULT;
    }

    log_debug("Sending HTTP %.*s request to %.*s", (int32_t)requestInfo->methodLen, requestInfo->pMethod,
              (int32_t)requestInfo->hostLen, requestInfo->pHost);

    /* Send the request and receive the response. */
    httpStatus = HTTPClient_Request(pTransportInterface, &requestHeaders, (uint8_t *)pRequestBodyBuf, reqBodyBufLen,
                                    response, 0);

    /* Release headers buffer */
    tal_free(requestHeaders.pBuffer);

    if (httpStatus != HTTPSuccess) {
        log_error("Failed to send HTTP %.*s request to %.*s: Error=%s.", (int32_t)requestInfo->methodLen,
                  requestInfo->pMethod, (int32_t)requestInfo->hostLen, requestInfo->pHost,
                  HTTPClient_strerror(httpStatus));
        return HTTP_CLIENT_SEND_FAULT;
    }

    // log_debug("Response Headers:\r\n%.*s\r\n"
    //           "Response Status:\r\n%u\r\n"
    //           "Response Body:\r\n%.*s\r\n",
    //           (int32_t)response->headersLen, response->pHeaders, response->statusCode, (int32_t)response->bodyLen,
    //           response->pBody);

    return HTTP_CLIENT_SUCCESS;
}

http_client_status_t http_client_request(const http_client_request_t *request, http_client_response_t *response)
{
    http_client_status_t rt = HTTP_CLIENT_SUCCESS;
    int ret = OPRT_OK;

    /* TLS pre init */
    NetworkContext_t network;
    bool use_tls = (request->cacert != NULL) || request->tls_no_verify;
    TUYA_TRANSPORT_TYPE_E transport_type = use_tls ? TRANSPORT_TYPE_TLS : TRANSPORT_TYPE_TCP;
    network = tuya_transporter_create(transport_type, NULL);
    if (NULL == network) {
        return HTTP_CLIENT_MALLOC_FAULT;
    }

    if (transport_type == TRANSPORT_TYPE_TLS) {
        bool verify_peer = !request->tls_no_verify;
        if (request->tls_no_verify) {
            log_debug("TLS no-verify fallback enabled for host=%s", request->host);
        }
        tuya_tls_config_t tls_config = {
            .ca_cert = request->cacert ? (char *)request->cacert : NULL,
            .ca_cert_size = request->cacert ? request->cacert_len : 0,
            .hostname = (char *)request->host,
            .port = (request->port == 0) ? DEFAULT_HTTPS_PORT : request->port,
            .timeout = request->timeout_ms,
            .mode = TUYA_TLS_SERVER_CERT_MODE,
            .verify = verify_peer,
        };

        ret = tuya_transporter_ctrl(network, TUYA_TRANSPORTER_SET_TLS_CONFIG, &tls_config);
        if (OPRT_OK != ret) {
            log_error("network_tls_init fail:%d", ret);
            return ret;
        }

        ret = tuya_transporter_connect(network, tls_config.hostname, tls_config.port, tls_config.timeout);
        if (OPRT_OK != ret) {
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
            return HTTP_CLIENT_SEND_FAULT;
        }

        log_debug("tls connencted!");
    } else {
        ret = tuya_transporter_connect(network, request->host, (request->port == 0) ? DEFAULT_HTTP_PORT : request->port,
                                       request->timeout_ms);
        if (OPRT_OK != ret) {
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
            return HTTP_CLIENT_SEND_FAULT;
        }
    }
    /* http client TransportInterface */
    TransportInterface_t pTransportInterface = {.pNetworkContext = (NetworkContext_t *)&network,
                                                .recv = (TransportRecv_t)NetworkTransportRecv,
                                                .send = (TransportSend_t)NetworkTransportSend};

    /* http client request object make */
    HTTPRequestInfo_t requestInfo = {
        .pMethod = request->method,
        .methodLen = strlen(request->method),
        .pHost = request->host,
        .hostLen = strlen(request->host),
        .pPath = request->path,
        .pathLen = strlen(request->path),
    };

    HTTPResponse_t http_response = {0};

    /* HTTP request send */
    log_debug("http request send!");
    rt = core_http_request_send((const TransportInterface_t *)&pTransportInterface,
                                (const HTTPRequestInfo_t *)&requestInfo, request->headers, request->headers_count,
                                (const uint8_t *)request->body, request->body_length, &http_response);

    /* tls disconnect */
    tuya_transporter_close(network);
    tuya_transporter_destroy(network);

    if (OPRT_OK != rt) {
        log_error("http_request_send error:%d", rt);
        return rt;
    }

    /* Response copy out */
    response->status_code = http_response.statusCode;
    response->body = http_response.pBody;
    response->body_length = http_response.bodyLen;
    response->headers = http_response.pHeaders;
    response->headers_length = http_response.headersLen;
    response->buffer = http_response.pBuffer;
    response->buffer_length = http_response.bufferLen;

    return HTTP_CLIENT_SUCCESS;
}

int http_client_free(http_client_response_t *response)
{
    if (NULL == response) {
        return OPRT_INVALID_PARM;
    }

    if (response->buffer) {
        tal_free(response->buffer);
    }

    if (response->body) {
        tal_free(response->body);
    }

    return OPRT_OK;
}
