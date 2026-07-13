/**
 * @file tuya_file_storage_com.c
 * @brief file storage common
 * @version 0.1
 * @date 2024-07-04
 *
 * @copyright Copyright (c) 2023 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tal_log.h"
#include "tal_hash.h"
#include "tal_time_service.h"
#include "mix_method.h"
#include "tuya_file_storage_com.h"

#define S3_SIGNED_HEADERS "host;x-amz-content-sha256;x-amz-date;x-amz-security-token"

static void __file_storage_calc_s3_sign(char *date, char signature[64],
                                        FILE_STORAGE_HEADER_PARAM_T *header_para)
{
    char *method_string = NULL;
    if (header_para->method == HTTP_PUT) {
        method_string = "PUT";
    } else if (header_para->method == HTTP_GET) {
        method_string = "GET";
    } else if (header_para->method == HTTP_POST) {
        method_string = "POST";
    } else {
        PR_ERR("method err");
        return;
    }
    char                  *unsigned_payload  = "UNSIGNED-PAYLOAD";
    FILE_STORAGE_CONFIG_T *config            = header_para->config;
    char                  *canonical_request = NULL;
    char                  *canonical_headers = NULL;
    int                    idx               = 0;
    char canonical_query_string[] = "";
    int canonical_headers_len = strlen(config->bucket) + strlen(config->endpoint) + strlen(date) +
                                strlen(config->token) + strlen(unsigned_payload) + 128;
    canonical_headers           = tal_malloc(canonical_headers_len);
    TUYA_CHECK_NULL_GOTO(canonical_headers, EXIT);
    memset(canonical_headers, 0, canonical_headers_len);
    snprintf(canonical_headers, canonical_headers_len,
             "host:%s.%s\nx-amz-content-sha256:%s\nx-amz-date:%s\nx-amz-security-token:%s\n", config->bucket,
             config->endpoint, unsigned_payload, date, config->token);
    PR_DEBUG("canonical_headers:%s", canonical_headers);

    int canonical_request_len = strlen(config->object) + strlen(canonical_query_string) + canonical_headers_len +
                                strlen(S3_SIGNED_HEADERS) + strlen(unsigned_payload) + 64;
    canonical_request           = tal_malloc(canonical_request_len);
    TUYA_CHECK_NULL_GOTO(canonical_request, EXIT);
    memset(canonical_request, 0, canonical_request_len);
    snprintf(canonical_request, canonical_request_len, "%s\n%s\n%s\n%s\n%s\n%s", method_string, config->object,
             canonical_query_string, canonical_headers, S3_SIGNED_HEADERS, unsigned_payload);
    PR_DEBUG("canonical_request:%s", canonical_request);

    uint8_t sha_canonical_request[SHA256_HEX_LEN];
    tal_sha256_ret((uint8_t *)canonical_request, strlen(canonical_request), sha_canonical_request, 0);
    uint8_t string_canonical_request[SHA256_STRING_LEN];
    hex2str(string_canonical_request, sha_canonical_request, SHA256_HEX_LEN);
    for (idx = 0; idx < (int)strlen((char *)string_canonical_request); idx++) {
        string_canonical_request[idx] = tuya_tolower(string_canonical_request[idx]);
    }

    char year_mon_day[9] = {0};
    memcpy(year_mon_day, date, 8);
    char scope[64] = {0};
    snprintf(scope, 64, "%s/%s/s3/aws4_request", year_mon_day, config->region);
    PR_DEBUG("scope:%s", scope);
    char string_to_sign[256] = {0};
    snprintf(string_to_sign, 256, "AWS4-HMAC-SHA256\n%s\n%s\n%s", date, scope, string_canonical_request);
    PR_DEBUG("string_to_sign:%s", string_to_sign);

    char key[64] = {0};
    snprintf(key, 64, "AWS4%s", config->sk);
    PR_DEBUG("key:%s", key);
    uint8_t date_key[SHA256_HEX_LEN] = {0};
    tal_sha256_mac((uint8_t *)key, strlen(key), (uint8_t *)year_mon_day, strlen(year_mon_day), date_key);
    uint8_t date_region_key[SHA256_HEX_LEN] = {0};
    tal_sha256_mac(date_key, SHA256_HEX_LEN, (uint8_t *)(config->region), strlen(config->region), date_region_key);
    uint8_t date_region_service_key[SHA256_HEX_LEN] = {0};
    tal_sha256_mac(date_region_key, SHA256_HEX_LEN, (uint8_t *)("s3"), strlen("s3"), date_region_service_key);
    uint8_t signing_key[SHA256_HEX_LEN] = {0};
    tal_sha256_mac(date_region_service_key, SHA256_HEX_LEN, (uint8_t *)("aws4_request"), strlen("aws4_request"),
                   signing_key);

    uint8_t final[SHA256_HEX_LEN] = {0};
    tal_sha256_mac(signing_key, SHA256_HEX_LEN, (uint8_t *)string_to_sign, strlen(string_to_sign), final);
    hex2str((uint8_t *)signature, final, SHA256_HEX_LEN);
    for (idx = 0; idx < (int)strlen(signature); idx++) {
        signature[idx] = tuya_tolower(signature[idx]);
    }

EXIT:
    if (canonical_headers) {
        tal_free(canonical_headers);
    }
    if (canonical_request) {
        tal_free(canonical_request);
    }
    return;
}

static void __file_storage_build_cos_headers(FILE_STORAGE_HEADER_PARAM_T *header_para)
{
    char *method_string = NULL;
    if (header_para->method == HTTP_PUT) {
        method_string = "put";
    } else if (header_para->method == HTTP_GET) {
        method_string = "get";
    } else if (header_para->method == HTTP_POST) {
        method_string = "post";
    } else {
        PR_ERR("method err");
        return;
    }
    FILE_STORAGE_CONFIG_T *config                = header_para->config;
    int                    idx                   = 0;
    char                   key_time[ST_DATE_LEN] = {0};
    TIME_T                 now                   = tal_time_get_posix();
    int                    key_time_len = snprintf(key_time, ST_DATE_LEN, "%u;%u", (unsigned)now, (unsigned)(now + COS_AUTH_EXPIRE_DEFAULT));
    uint8_t                hex_sign_key[SHA1_HEX_LEN]       = {0};
    uint8_t                string_sign_key[SHA1_STRING_LEN] = {0};
    tal_sha1_mac((uint8_t *)config->sk, strlen(config->sk), (uint8_t *)key_time, key_time_len, hex_sign_key);
    hex2str(string_sign_key, hex_sign_key, SHA1_HEX_LEN);
    for (idx = 0; idx < (int)strlen((char *)string_sign_key); idx++) {
        string_sign_key[idx] = tuya_tolower(string_sign_key[idx]);
    }

    PR_DEBUG("key_time:%s,key_time_len:%d,strlen:%d", key_time, key_time_len, (int)strlen(key_time));
    PR_DEBUG("string_sign_key:%s", string_sign_key);

    char *http_string            = NULL;
    int   malloc_http_string_len = strlen(config->object) + 64;
    http_string                    = tal_malloc(malloc_http_string_len);
    if (NULL == http_string) {
        PR_ERR("malloc HttpString err");
        return;
    }
    memset(http_string, 0, malloc_http_string_len);
    snprintf(http_string, malloc_http_string_len, "%s\n%s\n\n\n", method_string, config->object);
    PR_DEBUG("http_string:%s,strlen:%d", http_string, (int)strlen(http_string));

    uint8_t hex_sum[SHA1_HEX_LEN];
    uint8_t http_string_sha[SHA1_STRING_LEN] = {0};
    tal_sha1_ret((uint8_t *)http_string, strlen(http_string), hex_sum);
    hex2str(http_string_sha, hex_sum, SHA1_HEX_LEN);
    for (idx = 0; idx < (int)strlen((char *)http_string_sha); idx++) {
        http_string_sha[idx] = tuya_tolower(http_string_sha[idx]);
    }
    tal_free(http_string);
    PR_DEBUG("http_string_sha:%s", http_string_sha);

    char    StringToSign[ST_DATE_LEN + SHA1_STRING_LEN + 16] = {0};
    uint8_t hex_Signature[SHA1_HEX_LEN]                      = {0};
    uint8_t string_Signature[SHA1_STRING_LEN]                = {0};
    int     StringToSign_len =
        snprintf(StringToSign, ST_DATE_LEN + SHA1_STRING_LEN + 16, "sha1\n%s\n%s\n", key_time, http_string_sha);
    tal_sha1_mac(string_sign_key, strlen((char *)string_sign_key), (uint8_t *)StringToSign, StringToSign_len,
                 hex_Signature);
    hex2str(string_Signature, hex_Signature, SHA1_HEX_LEN);
    for (idx = 0; idx < (int)strlen((char *)http_string_sha); idx++) {
        string_Signature[idx] = tuya_tolower(string_Signature[idx]);
    }
    PR_DEBUG("string_Signature:%s", string_Signature);

    snprintf(header_para->auth_buf, sizeof(header_para->auth_buf),
             "q-sign-algorithm=sha1&q-ak=%s&q-sign-time=%s&q-key-time=%s&q-header-list=&q-url-param-list=&q-"
             "signature=%s",
             config->ak, key_time, key_time, string_Signature);

    int n = 0;
    header_para->headers[n].key = "Authorization";
    header_para->headers[n].value = header_para->auth_buf;
    n++;
    header_para->headers[n].key = "x-cos-security-token";
    header_para->headers[n].value = config->token;
    n++;
    header_para->headers_count = n;
}

static void __file_storage_build_s3_headers(FILE_STORAGE_HEADER_PARAM_T *header_para)
{
    char     signature[SHA256_STRING_LEN] = {0};
    char     year_mon_day[9]              = {0};
    POSIX_TM_S tm;

    FILE_STORAGE_CONFIG_T *config = header_para->config;
    memset(&tm, 0, sizeof(POSIX_TM_S));
    tal_time_get(&tm);
    snprintf(header_para->date_buf, ST_DATE_LEN, "%04d%02d%02dT%02d%02d%02dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
             tm.tm_min, tm.tm_sec);

    __file_storage_calc_s3_sign(header_para->date_buf, signature, header_para);

    memcpy(year_mon_day, header_para->date_buf, 8);

    snprintf(header_para->auth_buf, sizeof(header_para->auth_buf),
             "AWS4-HMAC-SHA256 Credential=%s/%s/%s/s3/aws4_request,SignedHeaders=%s,Signature=%s",
             config->ak, year_mon_day, config->region, S3_SIGNED_HEADERS, signature);

    int n = 0;
    header_para->headers[n].key = "x-amz-content-sha256";
    header_para->headers[n].value = "UNSIGNED-PAYLOAD";
    n++;
    header_para->headers[n].key = "Authorization";
    header_para->headers[n].value = header_para->auth_buf;
    n++;
    header_para->headers[n].key = "x-amz-date";
    header_para->headers[n].value = header_para->date_buf;
    n++;
    header_para->headers[n].key = "x-amz-security-token";
    header_para->headers[n].value = config->token;
    n++;
    header_para->headers_count = n;
}

static void __file_storage_build_azure_headers(FILE_STORAGE_HEADER_PARAM_T *header_para)
{
    FILE_STORAGE_CONFIG_T *config = header_para->config;

    char *tmp_sv = strstr(config->token, "sv=");
    if (tmp_sv == NULL) {
        PR_ERR("TOKEN has no sv");
        return;
    }
    int idx = 0;

    while (idx < 31) {
        if (tmp_sv[3 + idx] == '&') {
            break;
        }
        header_para->sv_buf[idx] = tmp_sv[3 + idx];
        idx++;
    }
    header_para->sv_buf[idx] = '\0';

    PR_DEBUG("SV = %s", header_para->sv_buf);

    int n = 0;
    header_para->headers[n].key = "x-ms-blob-type";
    header_para->headers[n].value = "BlockBlob";
    n++;
    header_para->headers[n].key = "x-ms-version";
    header_para->headers[n].value = header_para->sv_buf;
    n++;
    header_para->headers_count = n;
}

void tuya_file_storage_build_headers(FILE_STORAGE_HEADER_PARAM_T *param)
{
    if (NULL == param || NULL == param->config) {
        PR_ERR("param was null");
        return;
    }

    param->headers_count = 0;
    memset(param->auth_buf, 0, sizeof(param->auth_buf));
    memset(param->date_buf, 0, sizeof(param->date_buf));
    memset(param->sv_buf, 0, sizeof(param->sv_buf));

    if (0 == tuya_strncasecmp(param->config->provider, VENDOR_TENCENT_COS, strlen(VENDOR_TENCENT_COS))) {
        __file_storage_build_cos_headers(param);
    } else if (0 == tuya_strncasecmp(param->config->provider, VENDOR_AMAZON_S3, strlen(VENDOR_AMAZON_S3))) {
        __file_storage_build_s3_headers(param);
    } else if (0 == tuya_strncasecmp(param->config->provider, VENDOR_AZURE_BLOB, strlen(VENDOR_AZURE_BLOB))) {
        __file_storage_build_azure_headers(param);
    }
}
