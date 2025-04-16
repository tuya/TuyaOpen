/**
 * @file tuya_http.c
 * @brief Implementation of Tuya's HTTP client functionalities.
 *
 * This file provides the implementation of HTTP client functionalities,
 * including managing HTTP certificates, making HTTP requests, and parsing HTTP
 * responses. It utilizes a certificate manager to handle the certificates
 * required for secure HTTP connections.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "iotdns.h"
#include "http_parser.h"
#include "http_client_interface.h"
#include "tal_time_service.h"
#include "tal_log.h"

#ifndef MAX_HTTP_CERT_NUM
#define MAX_HTTP_CERT_NUM 3
#endif

typedef struct {
    char *host;
    uint16_t port;
    uint8_t *cacert;
    uint16_t cacert_len;
    TIME_T timeposix;
} tuya_cert_cache_t;

typedef struct {
    tuya_cert_cache_t cache[MAX_HTTP_CERT_NUM];
    uint8_t count;
} tuya_cert_mgr_t;

static tuya_cert_mgr_t s_tuya_cert_mgr;

/**
 * @brief Saves the HTTP certificate for a given host and port.
 *
 * This function saves the HTTP certificate for a specific host and port in a
 * certificate cache. If the cache is not full, a new entry is created. If the
 * cache is full, the oldest entry is replaced.
 *
 * @param[in] host The host name or IP address of the server.
 * @param[in] port The port number of the server.
 * @param[in] cacert Pointer to the certificate data.
 * @param[in] cacert_len The length of the certificate data.
 *
 * @return OPRT_OK if the certificate is saved successfully, or
 * OPRT_MALLOC_FAILED if memory allocation fails.
 */
int tuya_http_cert_save(char *host, uint16_t port, uint8_t *cacert, uint16_t cacert_len)
{
    tuya_cert_cache_t *cache = s_tuya_cert_mgr.cache;

    if (s_tuya_cert_mgr.count < MAX_HTTP_CERT_NUM) {
        cache = &s_tuya_cert_mgr.cache[s_tuya_cert_mgr.count];
        s_tuya_cert_mgr.count++;
    } else {
        TIME_T timeposix = cache[0].timeposix;
        cache = &s_tuya_cert_mgr.cache[0];
        for (int i = 1; i < s_tuya_cert_mgr.count; i++) {
            if (NULL == cache->host && NULL == cache->cacert) {
                cache = &s_tuya_cert_mgr.cache[i];
                break;
            }
            if (timeposix > cache[i].timeposix) {
                timeposix = cache[i].timeposix;
                cache = &s_tuya_cert_mgr.cache[i];
            }
        }
    }
    if (cache->host) {
        tal_free(cache->host);
        cache->host = NULL;
    }
    if (cache->cacert && cache->cacert_len) {
        tal_free(cache->cacert);
        cache->cacert = NULL;
    }
    cache->host = tal_calloc(1, strlen(host));
    if (NULL == cache->host) {
        return OPRT_MALLOC_FAILED;
    }
    strcpy(cache->host, host);
    cache->port = port;
    cache->cacert = cacert;
    cache->cacert_len = cacert_len;
    cache->timeposix = tal_time_get_posix();

    return OPRT_OK;
}

/**
 * @brief Finds a certificate cache entry based on the host and port.
 *
 * This function searches for a certificate cache entry that matches the given
 * host and port.
 *
 * @param host The host name to search for.
 * @param port The port number to search for.
 * @return A pointer to the matching certificate cache entry, or NULL if no
 * match is found.
 */
tuya_cert_cache_t *tuya_http_cert_find(char *host, uint16_t port)
{
    for (int i = 0; i < s_tuya_cert_mgr.count; i++) {
        if (0 == strcmp(s_tuya_cert_mgr.cache[i].host, host) && s_tuya_cert_mgr.cache[i].port == port) {
            return &s_tuya_cert_mgr.cache[i];
        }
    }

    return NULL;
}

/**
 * @brief Loads the certificate for the specified host and port.
 *
 * This function loads the certificate for the specified host and port from the
 * cache if it exists. If the certificate is not found in the cache, it queries
 * the host for the certificate and saves it in the cache.
 *
 * @param[in] host The host name or IP address.
 * @param[in] port The port number.
 * @param[out] cacert Pointer to the certificate data.
 * @param[out] cacert_len Length of the certificate data.
 *
 * @return OPRT_OK if the certificate is loaded successfully, an error code
 * otherwise.
 */
int tuya_http_cert_load(char *host, uint16_t port, uint8_t **cacert, uint16_t *cacert_len)
{
    int rt = OPRT_OK;

    tuya_cert_cache_t *cert_cache = tuya_http_cert_find(host, port);
    if (cert_cache) {
        *cacert = cert_cache->cacert;
        *cacert_len = cert_cache->cacert_len;
        return rt;
    }

    rt = tuya_iotdns_query_host_certs(host, port, cacert, cacert_len);
    if (OPRT_OK == rt) {
        rt = tuya_http_cert_save(host, port, *cacert, *cacert_len);
    }

    return rt;
}

/**
 * Sends a simple HTTP POST request to the specified URL with the given body and
 * headers.
 *
 * @param url The URL to send the request to.
 * @param body The body of the request.
 * @param headers An array of headers to include in the request.
 * @param headers_count The number of headers in the headers array.
 * @param response A pointer to a structure to store the response from the
 * server.
 * @return Returns an integer indicating the result of the operation. Possible
 * values are:
 *         - OPRT_OK: The request was successful.
 *         - OPRT_COM_ERROR: There was a communication error while parsing the
 * URL.
 *         - OPRT_MALLOC_FAILED: Memory allocation failed for host or path.
 *         - OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR: There was an error sending
 * the HTTP request.
 */
int tuya_http_client_post_simple(char *url, char *body, http_client_header_t *headers, uint8_t headers_count,
                                 http_client_response_t *response)
{
    int rt = OPRT_OK;

    http_client_request_t request = {0};
    struct http_parser_url purl;
    bool is_ssl = false;

    http_parser_url_init(&purl);
    if (0 != http_parser_parse_url(url, strlen(url), 0, &purl)) {
        rt = OPRT_COM_ERROR;
        goto __exit;
    }
    if (0 == memcmp("https", url + purl.field_data[UF_SCHEMA].off, strlen("https"))) {
        is_ssl = true;
        request.port = 443;
    } else {
        request.port = 80;
    }
    if (purl.field_set & 1 << UF_PORT) {
        request.port = purl.port;
    }
    request.body = (const uint8_t *)body;
    request.body_length = body ? strlen(body) : 0;
    request.method = "POST";
    request.headers = headers;
    request.headers_count = headers_count;
    request.host = tal_calloc(1, purl.field_data[UF_HOST].len + 1);
    request.path = tal_calloc(1, strlen(url) - purl.field_data[UF_PATH].off + 1);
    if (NULL == request.host || NULL == request.path) {
        rt = OPRT_MALLOC_FAILED;
        goto __exit;
    }
    memcpy((void *)request.host, url + purl.field_data[UF_HOST].off, purl.field_data[UF_HOST].len);
    memcpy((void *)request.path, url + purl.field_data[UF_PATH].off, strlen(url) - purl.field_data[UF_PATH].off);
    PR_DEBUG("path %s", request.path);
    //! cert load
    if (is_ssl) {
        TUYA_CALL_ERR_GOTO(tuya_http_cert_load((char *)request.host, request.port, (uint8_t **)&request.cacert,
                                               (uint16_t *)&request.cacert_len),
                           __exit);
    }

    http_client_status_t http_status = http_client_request(&request, response);
    if (HTTP_CLIENT_SUCCESS != http_status) {
        PR_ERR("http_request_send error:%d", http_status);
        rt = OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

__exit:
    if (request.host) {
        tal_free((void *)request.host);
    }
    if (request.path) {
        tal_free((void *)request.path);
    }

    return rt;
}

int tuya_http_free(http_client_response_t *response)
{
    return http_client_free(response);
}