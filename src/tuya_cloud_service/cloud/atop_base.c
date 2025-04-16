/**
 * @file atop_base.c
 * @brief This file contains the implementation of the ATOP protocol base
 * functions, including URL parameter encoding and decoding, request data
 * encoding, response data decoding, and response result parsing.
 *
 * The ATOP base functions are designed to facilitate communication between
 * devices and the Tuya cloud platform. They provide mechanisms for secure data
 * transmission through encryption and decryption, as well as data integrity
 * verification through MD5 signatures.
 *
 * The functions implemented in this file are essential for devices to correctly
 * format requests to the Tuya cloud platform and to parse responses from the
 * platform. They ensure that data is transmitted securely and efficiently,
 * adhering to the ATOP protocol specifications.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include <string.h>
#include <strings.h>
#include "atop_base.h"
#include "tuya_config_defaults.h"
#include "tal_log.h"
#include "tuya_endpoint.h"
#include "http_client_interface.h"
#include "cJSON.h"
#include "tal_security.h"
#include "mbedtls/base64.h"
#include "tal_memory.h"
#include "cipher_wrapper.h"
#include "uni_random.h"

#define MD5SUM_LENGTH               (16)
#define POST_DATA_PREFIX            (5) // 'data='
#define MAX_URL_LENGTH              (255)
#define DEFAULT_RESPONSE_BUFFER_LEN (1024)
#define AES_GCM128_NONCE_LEN        12
#define AES_GCM128_TAG_LEN          16

typedef struct {
    char *key;
    char *value;
} url_param_t;

static int atop_url_params_sign(const char *key, url_param_t *params, int param_num, uint8_t *out, size_t *olen)
{
    int rt = OPRT_OK;
    int printlen = 0;
    int i = 0;
    uint8_t digest[MD5SUM_LENGTH];

    char *buffer = tal_malloc(512);
    TUYA_CHECK_NULL_RETURN(buffer, OPRT_MALLOC_FAILED);

    for (i = 0; i < param_num; ++i) {
        printlen += sprintf(buffer + printlen, "%s=%s||", params[i].key, params[i].value);
    }
    printlen += sprintf(buffer + printlen, "%s", (char *)key);

    // make md5 digest bin
    tal_md5_ret((const uint8_t *)buffer, printlen, digest);
    tal_free(buffer);

    // make digest hex
    for (i = 0; i < MD5SUM_LENGTH; i++) {
        *olen += sprintf((char *)out + i * 2, "%02x", digest[i]);
    }
    return rt;
}

static int atop_url_params_encode(const char *key, url_param_t *params, int param_num, char *out, size_t *olen)
{
    int rt = OPRT_OK;
    char *buffer = out;
    int printlen = 0;
    size_t sign_len = 0;
    int i;

    // attach url params
    for (i = 0; i < param_num; i++) {
        printlen += sprintf(buffer + printlen, "%s=%s&", params[i].key, params[i].value);
    }

    // attach md5 signature
    printlen += sprintf(buffer + printlen, "sign=");
    rt = atop_url_params_sign(key, params, param_num, (uint8_t *)buffer + printlen, &sign_len);
    if (rt != 0) {
        PR_ERR("atop_url_params_sign error:%d", rt);
        return rt;
    }
    printlen += sign_len;
    *olen = printlen;
    return rt;
}

static int atop_request_data_encode(const char *key, const uint8_t *input, int ilen, uint8_t *output, size_t *olen)
{
    if (key == NULL || input == NULL || ilen == 0 || output == NULL || olen == NULL) {
        return OPRT_INVALID_PARM;
    }

    int ret = 0;
    int printlen = 0;
    int i;

    /* Encode buffer */
    size_t encrypt_olen = 0;
    size_t buflen = AES_GCM128_NONCE_LEN + ilen + AES_GCM128_TAG_LEN;
    uint8_t *encrypted_buffer = tal_malloc(buflen);
    if (encrypted_buffer == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    /* Nonce */
    uni_random_string((char *)encrypted_buffer, AES_GCM128_NONCE_LEN);

    /* AES128-GCM */
    ret = mbedtls_cipher_auth_encrypt_wrapper(&(const cipher_params_t){.cipher_type = MBEDTLS_CIPHER_AES_128_GCM,
                                                                       .key = (unsigned char *)key,
                                                                       .key_len = 16,
                                                                       .nonce = encrypted_buffer,
                                                                       .nonce_len = AES_GCM128_NONCE_LEN,
                                                                       .ad = NULL,
                                                                       .ad_len = 0,
                                                                       .data = (unsigned char *)input,
                                                                       .data_len = ilen},
                                              encrypted_buffer + AES_GCM128_NONCE_LEN, &encrypt_olen,
                                              encrypted_buffer + AES_GCM128_NONCE_LEN + ilen, AES_GCM128_TAG_LEN);
    if (ret != OPRT_OK) {
        PR_ERR("mbedtls_cipher_auth_encrypt_wrapper:0x%x", ret);
        return ret;
    }

    // output the hex data
    printlen = sprintf((char *)output, "%s", "data=");
    for (i = 0; i < (int)buflen; i++) {
        printlen += sprintf((char *)output + printlen, "%02X", (uint8_t)(encrypted_buffer[i]));
    }

    tal_free(encrypted_buffer);
    *olen = printlen;
    return ret;
}

static int atop_response_result_decrpyt(const char *key, const uint8_t *input, int ilen, uint8_t *output, size_t *olen)
{
    if (key == NULL || input == NULL || ilen == 0 || output == NULL || olen == NULL) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;

    rt = mbedtls_cipher_auth_decrypt_wrapper(
        &(const cipher_params_t){.cipher_type = MBEDTLS_CIPHER_AES_128_GCM,
                                 .key = (unsigned char *)key,
                                 .key_len = 16,
                                 .nonce = (unsigned char *)input,
                                 .nonce_len = AES_GCM128_NONCE_LEN,
                                 .ad = NULL,
                                 .ad_len = 0,
                                 .data = (unsigned char *)(input + AES_GCM128_NONCE_LEN),
                                 .data_len = ilen - AES_GCM128_NONCE_LEN - AES_GCM128_TAG_LEN},
        output, olen, (unsigned char *)(input + (ilen - AES_GCM128_TAG_LEN)), AES_GCM128_TAG_LEN);
    if (rt != OPRT_OK) {
        PR_ERR("aes128_ecb_decode error:%d", rt);
        return rt;
    }

    return rt;
}

static int atop_response_data_decode(const char *key, const uint8_t *input, size_t ilen, uint8_t *output, size_t *olen)
{
    int rt = OPRT_OK;

    char *value;
    size_t value_length;

    cJSON *root = cJSON_Parse((char *)input);
    if (NULL == root) {
        return OPRT_CJSON_PARSE_ERR;
    }

    cJSON *item = cJSON_GetObjectItem(root, "result");
    if (NULL == item) {
        PR_ERR("no result");
        return OPRT_CJSON_GET_ERR;
    }

    value = item->valuestring;
    value_length = strlen(value);

    PR_TRACE("base64 encode result:\r\n%.*s", value_length, value);

    // base64 decode buffer
    size_t b64buffer_len = value_length * 3 / 4;
    uint8_t *b64buffer = tal_malloc(b64buffer_len);
    size_t b64buffer_olen = 0;

    // base64 decode
    rt = mbedtls_base64_decode(b64buffer, b64buffer_len, &b64buffer_olen, (const uint8_t *)value, value_length);
    if (rt != OPRT_OK) {
        PR_ERR("base64 decode error:%d", rt);
        tal_free(b64buffer);
        cJSON_Delete(root);
        return rt;
    }

    rt = atop_response_result_decrpyt(key, (const uint8_t *)b64buffer, b64buffer_olen, output, olen);
    cJSON_Delete(root);
    tal_free(b64buffer);
    if (rt != OPRT_OK) {
        PR_ERR("atop_data_decrpyt error: %d", rt);
        return rt;
    }
    PR_DEBUG("result:\r\n%.*s", *olen, output);

    return rt;
}

static int atop_response_result_parse_cjson(const uint8_t *input, size_t ilen, atop_base_response_t *response)
{
    int rt = OPRT_OK;

    if (NULL == input || NULL == response) {
        PR_ERR("param error");
        return OPRT_INVALID_PARM;
    }

    if (input[ilen] != '\0') {
        PR_ERR("string length error ilen:%d, stlen:%d", ilen, strlen((char *)input));
    }

    // json parse
    cJSON *root = cJSON_Parse((const char *)input);
    if (NULL == root) {
        PR_ERR("Json parse error");
        return OPRT_CJSON_PARSE_ERR;
    }

    // verify success key
    if (!cJSON_HasObjectItem(root, "success")) {
        PR_ERR("not found json success key");
        cJSON_Delete(root);
        return OPRT_CJSON_GET_ERR;
    }

    // sync timestamp
    if (cJSON_HasObjectItem(root, "t")) {
        response->t = cJSON_GetObjectItem(root, "t")->valueint;
    }

    // if 'success == True', copy the json object to result point
    if (cJSON_GetObjectItem(root, "success")->type == cJSON_True) {
        response->success = true;
        response->result = cJSON_DetachItemFromObject(root, "result");
        cJSON_Delete(root);
        return OPRT_OK;
    }

    // Exception parse
    char *errorCode = NULL;
    response->success = false;
    response->result = NULL;

    // error msg dump
    if (cJSON_GetObjectItem(root, "errorMsg")) {
        PR_ERR("errorMsg:%s", cJSON_GetObjectItem(root, "errorMsg")->valuestring);
    }

    if (cJSON_GetObjectItem(root, "errorCode") == NULL) {
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
    }

    errorCode = cJSON_GetObjectItem(root, "errorCode")->valuestring;

    if (strcasecmp(errorCode, "GATEWAY_NOT_EXISTS") == 0) {
        rt = OPRT_LINK_CORE_HTTP_GW_NOT_EXIST;
    }

    // free cJSON object
    cJSON_Delete(root);
    return rt;
}

/**
 * Sends a request to the Tuya cloud service.
 *
 * This function sends a request to the Tuya cloud service using the provided
 * request parameters.
 *
 * @param request The request parameters for the Tuya cloud service.
 * @param response The response structure to store the response from the Tuya
 * cloud service.
 * @return Returns an integer value indicating the status of the request:
 *         - OPRT_OK: The request was successful.
 *         - OPRT_INVALID_PARM: Invalid parameters were provided.
 *         - OPRT_MALLOC_FAILED: Memory allocation failed.
 *         - OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR: Error occurred while sending
 * the HTTP request.
 */
int atop_base_request(const atop_base_request_t *request, atop_base_response_t *response)
{
    if (NULL == request || NULL == response) {
        return OPRT_INVALID_PARM;
    }

    int rt = OPRT_OK;
    http_client_status_t http_status;

    /* user data */
    response->user_data = (void *)request->user_data;

    /* params fill */
    url_param_t params[6];
    int idx = 0;
    params[idx].key = "a";
    params[idx++].value = (char *)request->api;

    if (request->devid) {
        params[idx].key = "devId";
        params[idx++].value = (char *)request->devid;
    }

    params[idx].key = "et";
    params[idx++].value = "3";

    char ts_str[11];
    sprintf(ts_str, "%d", request->timestamp);
    params[idx].key = "t";
    params[idx++].value = ts_str;

    if (request->uuid) {
        params[idx].key = "uuid";
        params[idx++].value = (char *)request->uuid;
    }

    if (request->version) {
        params[idx].key = "v";
        params[idx++].value = (char *)request->version;
    }

    /* url param buffer make */
    char *path_buffer = tal_malloc(MAX_URL_LENGTH);
    if (NULL == path_buffer) {
        PR_ERR("path_buffer malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    /* attach path prefix */
    int path_buffer_len = sprintf(path_buffer, "%s?", (char *)request->path);
    PR_DEBUG("TUYA_HTTPS_ATOP_URL: %s", path_buffer);

    /* param encode */
    size_t encode_len = 0;
    rt = atop_url_params_encode((char *)request->key, params, idx, path_buffer + path_buffer_len, &encode_len);
    if (rt != OPRT_OK) {
        PR_ERR("url param encode error:%d", rt);
        tal_free(path_buffer);
        return rt;
    }
    path_buffer_len += encode_len;
    PR_DEBUG("request url len:%d: %s", path_buffer_len, path_buffer);

    /* POST data buffer */
    size_t body_length = 0;
    uint8_t *body_buffer =
        tal_malloc(POST_DATA_PREFIX + (request->datalen + AES_GCM128_NONCE_LEN + AES_GCM128_TAG_LEN) * 2 + 1);
    if (NULL == body_buffer) {
        PR_ERR("body_buffer malloc fail");
        tal_free(path_buffer);
        return OPRT_MALLOC_FAILED;
    }

    /* POST data encode */
    PR_DEBUG("atop_request_data_encode");
    rt = atop_request_data_encode((char *)request->key, request->data, request->datalen, body_buffer, &body_length);
    if (rt != OPRT_OK) {
        PR_ERR("atop_post_data_encrypt error:%d", rt);
        tal_free(path_buffer);
        tal_free(body_buffer);
        return rt;
    }
    PR_DEBUG("out post data len:%d, data:%s", body_length, body_buffer);

    /* HTTP headers */
    http_client_header_t headers[] = {
        {.key = "User-Agent", .value = "TUYA_IOT_SDK"},
        {.key = "Content-Type", .value = "application/x-www-form-urlencoded;charset=UTF-8"},
    };
    uint8_t headers_count = sizeof(headers) / sizeof(http_client_header_t);

    http_client_response_t http_response = {0};

    /* HTTP Request send */
    PR_DEBUG("http request send!");
    const tuya_endpoint_t *endpoint = tuya_endpoint_get();
    http_status = http_client_request(&(const http_client_request_t){.cacert = endpoint->cert,
                                                                     .cacert_len = endpoint->cert_len,
                                                                     .host = endpoint->atop.host,
                                                                     .port = endpoint->atop.port,
                                                                     .method = "POST",
                                                                     .path = path_buffer,
                                                                     .headers = headers,
                                                                     .headers_count = headers_count,
                                                                     .body = body_buffer,
                                                                     .body_length = body_length,
                                                                     .timeout_ms = HTTP_TIMEOUT_MS_DEFAULT},
                                      &http_response);

    /* Release http buffer */
    tal_free(path_buffer);
    tal_free(body_buffer);

    if (HTTP_CLIENT_SUCCESS != http_status) {
        PR_ERR("http_request_send error:%d", http_status);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    size_t result_buffer_length = 0;
    uint8_t *result_buffer = tal_calloc(1, http_response.body_length);
    if (NULL == result_buffer) {
        PR_ERR("result_buffer malloc fail");
        http_client_free(&http_response);
        return OPRT_MALLOC_FAILED;
    }

    /* Decoded response data */
    rt = atop_response_data_decode(request->key, http_response.body, http_response.body_length, result_buffer,
                                   &result_buffer_length);

    if (OPRT_OK == rt) {
        rt = atop_response_result_parse_cjson(result_buffer, result_buffer_length, response);
    } else {
        PR_NOTICE("atop_response_decode error:%d, try parse the plaintext data.", rt);
        rt = atop_response_result_parse_cjson(http_response.body, http_response.body_length, response);
    }

    http_client_free(&http_response);
    tal_free(result_buffer);

    return rt;
}

/**
 * @brief Frees the memory allocated for an atop_base_response_t structure.
 *
 * This function frees the memory allocated for an atop_base_response_t
 * structure. If the response indicates success and contains a valid result, the
 * cJSON object associated with the result is deleted.
 *
 * @param response Pointer to the atop_base_response_t structure to be freed.
 */
void atop_base_response_free(atop_base_response_t *response)
{
    if (response->success == true && response->result) {
        cJSON_Delete(response->result);
    }
}
