/**
 * @file tuya_file_storage_com.h
 * @brief file storage common include
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

#ifndef _TUYA_FILE_STORAGE_COM_H
#define _TUYA_FILE_STORAGE_COM_H

#include "tuya_cloud_types.h"
#include "http_session.h"

#define VENDOR_TENCENT_COS         "cos"   //cn
#define VENDOR_AMAZON_S3           "s3"    //europe,us-west,india
#define VENDOR_AZURE_BLOB          "azure" //us-east,europe-west
#define ST_URL_LEN     256
#define ST_DATE_LEN    64
#define COS_AUTH_EXPIRE_DEFAULT    300
#define SHA1_HEX_LEN               20
#define SHA1_STRING_LEN            (SHA1_HEX_LEN * 2 + 1)
#define SHA256_HEX_LEN             32
#define SHA256_STRING_LEN          (SHA256_HEX_LEN * 2 + 1)
#define MAX_STORAGE_CUSTOM_HEADERS 6

typedef struct {
    char *provider;
    char *ak;
    char *sk;
    char *bucket;
    char *endpoint;
    char *token;
    char *region;
    char *pathconfig;
    char *object;
} FILE_STORAGE_CONFIG_T;

typedef struct {
    FILE_STORAGE_CONFIG_T *config;
    enum http_method method;
    // Built header storage
    http_custom_header_t headers[MAX_STORAGE_CUSTOM_HEADERS];
    int headers_count;
    // Internal buffers for computed header values
    char auth_buf[512];
    char date_buf[ST_DATE_LEN];
    char sv_buf[32];
} FILE_STORAGE_HEADER_PARAM_T;

/**
 * @brief Build cloud storage authentication headers
 *
 * Populates param->headers[] and param->headers_count based on the
 * cloud storage provider (COS/S3/Azure). The header values remain valid
 * until the param struct goes out of scope.
 *
 * @param[in,out] param Header param with config and method set
 */
void tuya_file_storage_build_headers(FILE_STORAGE_HEADER_PARAM_T *param);

#endif
