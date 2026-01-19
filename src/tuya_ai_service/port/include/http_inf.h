/**
 * @file http_inf.h
 * @brief http_inf module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __HTTP_INF_H__
#define __HTTP_INF_H__

#include "tuya_cloud_types.h"

#include "tuya_ai_types.h"

#include "ty_cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/**
 * @brief Definition of HTTP URL param structure
 */
typedef struct {
    /** max count of URL params */
    unsigned char total;
    /** used count of URL params */
    unsigned char cnt;
    /** pointer to URL param */
    char *pos[0];
} HTTP_PARAM_H_S;

typedef struct {
    /** param handle */
    HTTP_PARAM_H_S *param_h;
    /** param insert pos */
    char *param_in;
    /** head_size == "url?" or "url" */
    unsigned short head_size;
    /** bufer len */
    unsigned short buf_len;
    /** "url?key=value" + "kev=value statistics" */
    char buf[0];
} HTTP_URL_H_S;

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Create url of HTTP request
 *
 * @param[in] buf_len max lenth of URL(include params)
 * @param[in] param_cnt max count of url params
 *
 * @return NULL on error. Pointer to HTTP_URL_H_S on success
 */
HTTP_URL_H_S *create_http_url_h(IN CONST uint16_t buf_len, IN CONST uint16_t param_cnt);

/**
 * @brief Free url of HTTP request
 *
 * @param[in] hu_h A pointer that points to the structure returned from the call to create_http_url_h
 *
 */
VOID del_http_url_h(IN HTTP_URL_H_S *hu_h);

/**
 * @brief Initiaze url of HTTP request
 *
 * @param[in,out] hu_h A pointer that points to the structure returned from the call to create_http_url_h
 * @param[in] url_h HTTP url
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET fill_url_head(INOUT HTTP_URL_H_S *hu_h, IN CONST char *url_h);

// iot api
OPERATE_RET iot_httpc_common_post(IN CONST char *api_name, IN CONST char *api_ver,
                                  IN CONST char *uuid, IN CONST char *devid,
                                  IN char *post_data,
                                  IN CONST char *p_head_other,
                                  OUT ty_cJSON **pp_result);

#ifdef __cplusplus
}
#endif

#endif /* __HTTP_INF_H__ */
