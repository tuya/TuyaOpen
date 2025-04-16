/**
 * @file iotdns.c
 * @brief Implementation of IoT DNS resolution functions.
 *
 * This file contains the implementation of functions to resolve IoT DNS
 * queries. It includes mechanisms to construct DNS request payloads, send them
 * to the Tuya IoT DNS service, and decode the responses to obtain URLs for
 * HTTPS and MQTT services. The file utilizes cJSON for parsing JSON responses
 * and mbedtls for any required encryption tasks.
 *
 * The DNS resolution process is crucial for devices to dynamically discover the
 * URLs of the Tuya cloud services they need to communicate with, based on their
 * region and environment settings.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "tuya_cloud_types.h"
#include "tuya_config_defaults.h"
#include "tuya_endpoint.h"
#include "tal_log.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "tuya_error_code.h"
#include "tal_memory.h"
#include "http_client_interface.h"
#include "tuya_register_center.h"
#include "mix_method.h"

#define IOTDNS_REQUEST_FMT                                                                                             \
    "{\"config\":[{\"key\":\"httpsSelfUrl\",\"need_ca\":true},{\"key\":"                                               \
    "\"mqttsSelfUrl\",\"need_ca\":true}],\"region\":\"%s\",\"env\":\"%s\"}"

#define IOTDNS_REQUEST_FMT_NOREGION                                                                                    \
    "{\"config\":[{\"key\":\"httpsSelfUrl\",\"need_ca\":true},{\"key\":"                                               \
    "\"mqttsSelfUrl\",\"need_ca\":true}],\"env\":\"%s\"}"

static int iotdns_response_decode(const uint8_t *input, size_t ilen, tuya_endpoint_t *endport)
{
    cJSON *root = cJSON_Parse((const char *)input);
    if (root == NULL) {
        return OPRT_CJSON_PARSE_ERR;
    }

    if (cJSON_GetObjectItem(root, "httpsSelfUrl") == NULL || cJSON_GetObjectItem(root, "mqttsSelfUrl") == NULL) {
        cJSON_Delete(root);
        return OPRT_CR_CJSON_ERR;
    }

    char *httpsSelfUrl = cJSON_GetObjectItem(cJSON_GetObjectItem(root, "httpsSelfUrl"), "addr")->valuestring;
    char *mqttsSelfUrl = cJSON_GetObjectItem(cJSON_GetObjectItem(root, "mqttsSelfUrl"), "addr")->valuestring;
    char *caArr0 = cJSON_GetArrayItem(cJSON_GetObjectItem(root, "caArr"), 0)->valuestring;
    PR_DEBUG("httpsSelfUrl:%s", httpsSelfUrl);
    PR_DEBUG("mqttsSelfUrl:%s", mqttsSelfUrl);

    /* ATOP url decode */
    int port = 443;
    sscanf(httpsSelfUrl, "https://%64[^/]%16[^\n]", endport->atop.host, endport->atop.path);
    endport->atop.port = (uint16_t)port;
    PR_DEBUG("endport->atop.host = \"%s\"", endport->atop.host);
    PR_DEBUG("endport->atop.port = %d", endport->atop.port);
    PR_DEBUG("endport->atop.path = \"%s\"", endport->atop.path);

    /* MQTT host decode */
    sscanf(mqttsSelfUrl, "%64[^:]:%5d[^\n]", endport->mqtt.host, &port);
    endport->mqtt.port = (uint16_t)port;
    PR_DEBUG("endport->mqtt.host = \"%s\"", endport->mqtt.host);
    PR_DEBUG("endport->mqtt.port = %d", endport->mqtt.port);

    /* cert decode */
    // base64 decode buffer
    size_t caArr0_len = strlen(caArr0);
    size_t buffer_len = caArr0_len * 3 / 4;
    uint8_t *caArr_raw = tal_malloc(buffer_len);
    size_t caArr_raw_len = 0;

    // base64 decode
    if (mbedtls_base64_decode(caArr_raw, buffer_len, &caArr_raw_len, (const uint8_t *)caArr0, caArr0_len) != 0) {
        PR_ERR("base64 decode error");
        tal_free(caArr_raw);
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
    }

    endport->cert = caArr_raw;
    endport->cert_len = caArr_raw_len;

    cJSON_Delete(root);
    return OPRT_OK;
}

static int iotdns_base_request(char *body, char *path, http_client_response_t *http_response)
{
    http_client_status_t http_status;

    /* HTTP headers */
    http_client_header_t headers[] = {
        {.key = "User-Agent", .value = "TUYA_OPEN_SDK"},
        {.key = "Content-Type", .value = "application/x-www-form-urlencoded;charset=UTF-8"},
    };
    uint8_t headers_count = sizeof(headers) / sizeof(http_client_header_t);

    register_center_t rcs;
    tuya_register_center_get(&rcs);

    /* HTTP Request send */
    PR_DEBUG("http request send!");
    http_status = http_client_request(
        &(const http_client_request_t){
            .cacert = rcs.ca_cert,
            .cacert_len = rcs.ca_cert_len,
            .host = rcs.urlx ? rcs.urlx : rcs.url0,
            .port = 443,
            .method = "POST",
            .path = path,
            .headers = headers,
            .headers_count = headers_count,
            .body = (const uint8_t *)body,
            .body_length = strlen(body),
            .timeout_ms = HTTP_TIMEOUT_MS_DEFAULT,
        },
        http_response);

    if (HTTP_CLIENT_SUCCESS != http_status) {
        PR_ERR("http_request_send error:%d", http_status);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    return OPRT_OK;
}

/**
 * @brief Retrieves the cloud endpoint for the specified region and environment.
 *
 * This function sends an HTTP POST request to retrieve the cloud endpoint for
 * the specified region and environment.
 *
 * @param region The region to retrieve the cloud endpoint for. Can be NULL.
 * @param env The environment to retrieve the cloud endpoint for.
 * @param endpoint Pointer to a tuya_endpoint_t structure to store the retrieved
 * endpoint.
 *
 * @return Returns OPRT_OK on success, or an error code on failure.
 *         Possible error codes:
 *         - OPRT_INVALID_PARM: Invalid parameter (env or endpoint is NULL).
 *         - OPRT_MALLOC_FAILED: Memory allocation failed.
 *         - OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR: Error sending HTTP request.
 */
int iotdns_cloud_endpoint_get(const char *region, const char *env, tuya_endpoint_t *endpoint)
{
    if (NULL == env || NULL == endpoint) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;
    http_client_status_t http_status;

    /* POST data buffer */
    size_t body_length = 0;
    char *body_buffer = tal_malloc(128);
    if (NULL == body_buffer) {
        PR_ERR("body_buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    if (region) {
        body_length = sprintf(body_buffer, IOTDNS_REQUEST_FMT, region, env);
    } else {
        body_length = sprintf(body_buffer, IOTDNS_REQUEST_FMT_NOREGION, env);
    }
    PR_DEBUG("out post data len:%d, data:%s", body_length, body_buffer);

    /* HTTP headers */
    http_client_header_t headers[] = {
        {.key = "Content-Type", .value = "application/x-www-form-urlencoded;charset=UTF-8"},
    };
    uint8_t headers_count = sizeof(headers) / sizeof(http_client_header_t);

    http_client_response_t http_response = {0};

    register_center_t rcs;
    tuya_register_center_get(&rcs);

    /* HTTP Request send */
    PR_DEBUG("http request send!");
    http_status = http_client_request(
        &(const http_client_request_t){
            .cacert = rcs.ca_cert,
            .cacert_len = rcs.ca_cert_len,
            .host = rcs.urlx ? rcs.urlx : rcs.url0,
            .port = 443,
            .method = "POST",
            .path = "/v2/url_config",
            .headers = headers,
            .headers_count = headers_count,
            .body = (const uint8_t *)body_buffer,
            .body_length = body_length,
            .timeout_ms = HTTP_TIMEOUT_MS_DEFAULT,
        },
        &http_response);

    /* Release http buffer */
    tal_free(body_buffer);

    if (HTTP_CLIENT_SUCCESS != http_status) {
        PR_ERR("http_request_send error:%d", http_status);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    /* Decoded response data */
    rt = iotdns_response_decode(http_response.body, http_response.body_length, endpoint);
    if (region) {
        strcpy(endpoint->region, region);
    }
    http_client_free(&http_response);

    return rt;
}

static int iotdns_query_domain_certs_parser(const uint8_t *input, uint8_t **cacert, uint16_t *cacert_len)
{
    int rt = OPRT_OK;

    cJSON *root = cJSON_Parse((char *)input);
    if (NULL == root) {
        PR_ERR("json parse fail. Rev:%s", input);
        return OPRT_CJSON_PARSE_ERR;
    }

    cJSON *errcode = cJSON_GetObjectItem(root, "errcode");
    if (errcode) {
        PR_ERR("errorCode:%s", errcode->valuestring);
        rt = OPRT_COM_ERROR;
        goto __exit;
    }
    cJSON *item = cJSON_GetArrayItem(root, 0);
    if (item == NULL) {
        rt = OPRT_CJSON_GET_ERR;
        goto __exit;
    }
    cJSON *ca = cJSON_GetObjectItem(item, "ca");
    if (item == NULL) {
        rt = OPRT_CJSON_GET_ERR;
        goto __exit;
    }
    *cacert = tal_calloc(1, strlen(ca->valuestring) + 1);
    if (NULL == *cacert) {
        rt = OPRT_MALLOC_FAILED;
        goto __exit;
    }
    *cacert_len = tuya_base64_decode(ca->valuestring, *cacert);
__exit:
    cJSON_Delete(root);

    return rt;
}

/**
 * @brief Queries the host certificates using the IoT DNS service.
 *
 * This function queries the host certificates for a given host and port using
 * the IoT DNS service.
 *
 * @param[in] host The host name or IP address.
 * @param[in] port The port number.
 * @param[out] cacert A pointer to the buffer that will hold the retrieved CA
 * certificate.
 * @param[out] cacert_len A pointer to the variable that will hold the length of
 * the retrieved CA certificate.
 *
 * @return OPRT_OK if the operation is successful, otherwise an error code.
 */
int tuya_iotdns_query_host_certs(char *host, uint16_t port, uint8_t **cacert, uint16_t *cacert_len)
{
    if (NULL == host) {
        return OPRT_INVALID_PARM;
    }

    /* POST data buffer */
    char *body_buffer = tal_malloc(256);
    if (NULL == body_buffer) {
        PR_ERR("body_buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }
    sprintf(body_buffer, "[{\"host\":\"%s\", \"port\":%d, \"need_ca\":true}]", host, port);

    PR_DEBUG("iotdns query %s", body_buffer);

    http_client_response_t http_response;

    int rt = iotdns_base_request(body_buffer, "/device/dns_query", &http_response);
    tal_free(body_buffer);
    if (OPRT_OK == rt) {
        rt = iotdns_query_domain_certs_parser(http_response.body, cacert, cacert_len);
        http_client_free(&http_response);
    }

    return OPRT_OK;
}

/**
 * @brief Queries the domain certificates for a given URL.
 *
 * This function queries the domain certificates for a given URL and returns the
 * certificate and its length.
 *
 * @param url The URL for which to query the domain certificates.
 * @param cacert A pointer to a pointer that will store the domain certificate.
 * @param cacert_len A pointer to a variable that will store the length of the
 * domain certificate.
 *
 * @return Returns OPRT_MALLOC_FAILED if memory allocation fails, otherwise
 * returns the result of the tuya_iotdns_query_host_certs function.
 */
int tuya_iotdns_query_domain_certs(char *url, uint8_t **cacert, uint16_t *cacert_len)
{
    char *p_tmp_url = tal_malloc(strlen(url) + 128);
    if (p_tmp_url == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    strcpy(p_tmp_url, url);

    char *p_search_head = p_tmp_url;
    char *p_tmp = strstr(p_search_head, "://");

    if (p_tmp != NULL) {
        *p_tmp = 0;
        p_search_head = p_tmp + strlen("://");
    }
    p_tmp = strstr(p_search_head, "/");
    if (p_tmp != NULL) {
        *p_tmp = 0;
    }
    char *p_host = p_search_head;
    uint32_t port = 443;
    p_tmp = strstr(p_search_head, ":");
    if (p_tmp != NULL) {
        *p_tmp = 0;
        char *p_port = p_tmp + strlen(":");
        port = atoi(p_port);
    }

    int rt = tuya_iotdns_query_host_certs(p_host, port, cacert, cacert_len);

    tal_free(p_tmp_url);

    return rt;
}
