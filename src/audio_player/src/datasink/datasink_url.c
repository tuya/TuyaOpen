/**
 * @file datasink_url.c
 * @brief 
 * @version 0.1
 * @date 2025-09-23
 * 
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
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

#include "tal_api.h"
#include "http_session.h"
#include "mix_method.h"
#include "datasink_cfg.h"

typedef enum {
    URLSINK_STATUS_INIT,
    URLSINK_STATUS_CONNECT,
    URLSINK_STATUS_RECV,
    URLSINK_STATUS_EXIT
} URLSINK_STATUS_T;

typedef struct {
    http_session_t session;
    URLSINK_STATUS_T status;
    uint32_t length;
    uint32_t offset;
    bool chunked;
    bool eof;
    char *url;
    TIME_T download_start_s;
} URL_DATASINK_CTX_T;

static void __http_connect(void *data)
{
    URL_DATASINK_CTX_T *ctx = (URL_DATASINK_CTX_T *)data;
    if(!ctx || (ctx->status == URLSINK_STATUS_INIT)) {
        return;
    }

    OPERATE_RET rt = OPRT_OK;
    http_resp_t *response = NULL;
    http_req_t request = {
        .type        = HTTP_GET,
        .version     = HTTP_VER_1_1
    };
    uint32_t field_flags = (HTTP_REQUEST_KEEP_ALIVE_FLAG);
    if(ctx->offset > 0) {
        request.download_offset = ctx->offset;
        request.download_size = ctx->length;
        PR_DEBUG("url sink resume from %d length %d", ctx->offset, ctx->length);
    }


    TUYA_CALL_ERR_GOTO(http_open_session(&ctx->session, ctx->url, 0), ERR_EXIT);
    TUYA_CALL_ERR_GOTO(http_send_request(ctx->session, &request, field_flags), ERR_EXIT);
    TUYA_CALL_ERR_GOTO(http_get_response_hdr(ctx->session, &response), ERR_EXIT);

    if ((response->status_code != 200) && (response->status_code != 206)) {
        if(HTTP_RESP_REDIR(response->status_code) && response->location != NULL) { //redirect
            PR_DEBUG("url sink redirect to %s", response->location);

            if(ctx->url) {
                Free(ctx->url);
                ctx->url = NULL;
            }
            ctx->url = mm_strdup(response->location);
            TUYA_CHECK_NULL_GOTO(ctx->url, ERR_EXIT);

            if(ctx->session) {
                http_close_session(&ctx->session);
                ctx->session = NULL;
            }

            TUYA_CALL_ERR_GOTO(tal_workq_schedule(WORKQ_SYSTEM, __http_connect, (void *)ctx), ERR_EXIT);
        } else {
            PR_ERR("url sink err status_code: %d", response->status_code);
            ctx->status = URLSINK_STATUS_EXIT;
        }
    } else {
        if(response->status_code == 200) {
            ctx->length = response->content_length;
            ctx->chunked = response->chunked;
            PR_DEBUG("url sink file size %d chunked %d", response->content_length, response->chunked);
        }else if(response->status_code == 206) {
            // in resume mode, server may not return content-length
            if(response->content_length > 0) {
                ctx->length = ctx->offset + response->content_length;
            }
            ctx->chunked = response->chunked;
            PR_DEBUG("url sink resume file size %d chunked %d", ctx->length, response->chunked);
        }

        http_set_timeout(ctx->session, AI_PLAYER_HTTP_YIELD_MS);
        ctx->status = URLSINK_STATUS_RECV;
        ctx->download_start_s = tal_time_get_posix();
    }

    http_free_response_hdr(&response);

    return;

ERR_EXIT:
    if(ctx->session) {
        http_close_session(&ctx->session);
        ctx->session = NULL;
    }
    ctx->status = URLSINK_STATUS_EXIT;
}

OPERATE_RET datasink_url_start(char *value, void**handle)
{
    OPERATE_RET rt = OPRT_OK;
    URL_DATASINK_CTX_T *ctx = (URL_DATASINK_CTX_T *)(*handle);

    if(ctx) {
        if(ctx->session) {
            http_close_session(&ctx->session);
            ctx->session = NULL;
        }
        if(ctx->url) {
            Free(ctx->url);
            ctx->url = NULL;
        }
    } else {
        ctx = (URL_DATASINK_CTX_T *)Malloc(sizeof(URL_DATASINK_CTX_T));
        if (ctx == NULL) {
            return OPRT_MALLOC_FAILED;
        }
        *handle = ctx;
    }

    memset(ctx, 0, sizeof(URL_DATASINK_CTX_T));

    ctx->url = mm_strdup(value);
    if(!ctx->url) {
        return OPRT_MALLOC_FAILED;
    }

    ctx->download_start_s = tal_time_get_posix();
    ctx->status = URLSINK_STATUS_CONNECT;
    ctx->eof = false;
    rt = tal_workq_schedule(WORKQ_SYSTEM, __http_connect, (void *)ctx);
    if (rt != OPRT_OK) {
        ctx->status = URLSINK_STATUS_EXIT;
    }

    return rt;
}

OPERATE_RET datasink_url_stop(void* handle)
{
    URL_DATASINK_CTX_T *ctx = (URL_DATASINK_CTX_T *)handle;
    if(!ctx) {
        return OPRT_INVALID_PARM;
    }

    ctx->status = URLSINK_STATUS_INIT;
    ctx->eof = false;
    if(ctx->session) {
        http_close_session(&ctx->session);
        ctx->session = NULL;
    }
    if(ctx->url) {
        Free(ctx->url);
        ctx->url = NULL;
    }

    return OPRT_OK;
}

OPERATE_RET datasink_url_exit(void* handle)
{
    datasink_url_stop(handle);
    Free(handle);

    return OPRT_OK;
}

OPERATE_RET datasink_url_read(void* handle, uint8_t *data, uint32_t len, uint32_t *out_len)
{
    URL_DATASINK_CTX_T *ctx = (URL_DATASINK_CTX_T *)handle;
    if(!ctx) {
        return OPRT_INVALID_PARM;
    }

    *out_len = 0;

    if(ctx->status <= URLSINK_STATUS_CONNECT) {
        tal_system_sleep(AI_PLAYER_HTTP_YIELD_MS);
        return OPRT_OK;
    } else if(ctx->status == URLSINK_STATUS_EXIT) {
        if(ctx->chunked) { // in chunked mode, do not support retry
            return OPRT_NETWORK_ERROR;
        } else if((ctx->download_start_s + AI_PLAYER_HTTP_TIMEOUT_S) < tal_time_get_posix()) { // timeout
            return OPRT_NETWORK_ERROR;
        } else {
            ctx->status = URLSINK_STATUS_CONNECT;
            return tal_workq_schedule(WORKQ_SYSTEM, __http_connect, (void *)ctx);
        }
    }

    if(ctx->eof || ((ctx->offset >= ctx->length) && !ctx->chunked)) {
        if(ctx->session) {
            http_close_session(&ctx->session);
            ctx->session = NULL;
        }
        ctx->eof = true;
        return OPRT_NOT_FOUND; // eof
    }

    int ret = http_read_content(ctx->session, data, len);
    if(ret > 0) {
        *out_len = ret;
        ctx->offset += ret;
        ctx->download_start_s = tal_time_get_posix();
        return OPRT_OK;
    } else if(ret == 0) {
        ctx->eof = true;
        return OPRT_NOT_FOUND; // eof
    } else {
        PR_ERR("url sink err offset %d length %d start_s %d", ctx->offset, ctx->length, ctx->download_start_s);
        ctx->status = URLSINK_STATUS_EXIT; // error, let connect again
        return OPRT_OK;
    }
}

DATASINK_T g_datasink_url = {
    .start = datasink_url_start,
    .stop  = datasink_url_stop,
    .exit  = datasink_url_exit,
    .feed  = NULL,
    .read  = datasink_url_read,
};
