/**
 * @file httpc.c
 * @brief httpc module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "httpc.h"

#include <string.h>

#include "tuya_error_code.h"
#include "core_http_client.h"

#include "http_port_internal.h"

/***********************************************************
************************macro define************************
***********************************************************/

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
int http_read_content(http_session_t handle, void *buf, unsigned int max_len)
{
    if (!handle || !buf || 0 == max_len) {
        return OPRT_INVALID_PARM;
    }

    http_session_ctx_t *ctx = (http_session_ctx_t *)handle;
    if (!ctx || !ctx->response_ready) {
        return OPRT_COM_ERROR;
    }

    /* Check if in streaming mode */
    if (ctx->streaming_mode) {
        /* Check if we've already read all data */
        if (ctx->bytes_read >= ctx->total_body_length) {
            return 0;
        }

        /* Use HTTPClient_Recv to read data directly into caller's buffer */
        int32_t bytes_read = HTTPClient_Recv(&ctx->transport, &ctx->http_response, (uint8_t *)buf, (size_t)max_len);

        if (bytes_read < 0) {
            /* Network error or read failure */
            return OPRT_COM_ERROR;
        }

        /* Update bytes read counter */
        ctx->bytes_read += bytes_read;

        return (int)bytes_read;
    } else {
        /* Legacy mode: copy from pre-allocated buffer */
        if (!ctx->response.body) {
            return OPRT_COM_ERROR;
        }

        size_t remaining = 0;
        if (ctx->response.body_length > ctx->read_offset) {
            remaining = ctx->response.body_length - ctx->read_offset;
        } else {
            return 0;
        }

        if (remaining == 0) {
            return 0;
        }

        size_t to_copy = remaining < max_len ? remaining : max_len;
        memcpy(buf, ctx->response.body + ctx->read_offset, to_copy);
        ctx->read_offset += to_copy;

        return (int)to_copy;
    }
}