/**
 * @file http_inf.c
 * @brief http_inf module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "http_inf.h"

#include "ty_cJSON.h"
#include "cJSON.h"

#include "tal_api.h"
#include "tuya_iot.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define DEF_URL_LEN 1024

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

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Create url of HTTP request
 *
 * @param[in] buf_len max length of URL (include params), if 0 then use DEF_URL_LEN
 * @param[in] param_cnt max count of url params
 *
 * @return NULL on error. Pointer to HTTP_URL_H_S on success
 */
HTTP_URL_H_S *create_http_url_h(IN CONST uint16_t buf_len, IN CONST uint16_t param_cnt)
{
    uint16_t len;

    if (0 == buf_len) {
        len = DEF_URL_LEN;
    } else {
        len = buf_len;
    }

    uint16_t total_len = sizeof(HTTP_URL_H_S) + len + sizeof(HTTP_PARAM_H_S) + sizeof(char *) * param_cnt;

    HTTP_URL_H_S *hu_h = HTTP_MEMORY_MALLOC(total_len);
    if (NULL == hu_h) {
        PR_ERR("malloc http url handle failed, size:%d", total_len);
        return NULL;
    }

    memset(hu_h, 0, total_len);
    hu_h->head_size = 0;
    hu_h->param_in = hu_h->buf;
    hu_h->buf_len = len;

    HTTP_PARAM_H_S *param_h = (HTTP_PARAM_H_S *)((uint8_t *)hu_h + (sizeof(HTTP_URL_H_S) + len));
    param_h->cnt = 0;
    param_h->total = param_cnt;

    hu_h->param_h = param_h;

    return hu_h;
}

/**
 * @brief Free url of HTTP request
 *
 * @param[in] hu_h A pointer that points to the structure returned from the call to create_http_url_h
 */
VOID del_http_url_h(IN HTTP_URL_H_S *hu_h)
{
    if (hu_h) {
        HTTP_MEMORY_FREE(hu_h);
    }
}

/**
 * @brief Initialize url of HTTP request
 *
 * @param[in,out] hu_h A pointer that points to the structure returned from the call to create_http_url_h
 * @param[in] url_h HTTP url head
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET fill_url_head(INOUT HTTP_URL_H_S *hu_h, IN CONST char *url_h)
{
    if (NULL == hu_h || NULL == url_h) {
        PR_ERR("invalid param");
        return OPRT_INVALID_PARM;
    }

    int url_len = strlen(url_h);
    int buf_remain = hu_h->buf_len - (hu_h->param_in - hu_h->buf);
    hu_h->head_size = url_len < buf_remain ? url_len : buf_remain - 1;

    strncpy(hu_h->param_in, url_h, hu_h->head_size);
    hu_h->param_in += hu_h->head_size;
    *hu_h->param_in = '\0';

    return OPRT_OK;
}

// tuya iot http api
OPERATE_RET iot_httpc_common_post(IN CONST char *api_name, IN CONST char *api_ver, IN CONST char *uuid,
                                  IN CONST char *devid, IN char *post_data, IN CONST char *p_head_other,
                                  OUT ty_cJSON **pp_result)
{
    OPERATE_RET rt = OPRT_OK;

    if (!api_name || !api_ver || !post_data || !pp_result) {
        return OPRT_INVALID_PARM;
    }

    TIME_T timestamp = tal_time_get_posix();
    tuya_iot_client_t *iot_hdl = tuya_iot_client_get();
    atop_base_request_t atop_request = {
        .devid = devid,
        .uuid = uuid,
        .key = iot_hdl->activate.seckey,
        .path = "/d.json",
        .timestamp = timestamp,
        .api = api_name,
        .version = api_ver,
        .data = post_data,
        .datalen = strlen(post_data),
        .user_data = NULL,
    };
    atop_base_response_t response = {0};
    rt = atop_base_request(&atop_request, &response);
    if (OPRT_OK != rt) {
        PR_ERR("http post err, rt:%d", rt);
        return rt;
    }

    *pp_result = response.result;

    return OPRT_OK;
}
