/**
 * @file ai_player_datasink.c
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
#include "datasink_cfg.h"
#include "ai_player_datasink.h"

#if defined(AI_PLAYER_DATASINK) && (AI_PLAYER_DATASINK & AI_PLAYER_DATASINK_URL)
extern DATASINK_T g_datasink_url;
#endif
#if defined(AI_PLAYER_DATASINK) && (AI_PLAYER_DATASINK & AI_PLAYER_DATASINK_FILE)
extern DATASINK_T g_datasink_file;
#endif
#if defined(AI_PLAYER_DATASINK) && (AI_PLAYER_DATASINK & AI_PLAYER_DATASINK_MEM)
extern DATASINK_T g_datasink_mem;
#endif

typedef struct {
    DATASINK_T *sink;
    void* priv;
} DATASINK_CTX_T;

OPERATE_RET ai_player_datasink_init(PLAYER_DATASINK *handle)
{
    if (handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    DATASINK_CTX_T *ctx = (DATASINK_CTX_T *)Malloc(sizeof(DATASINK_CTX_T));
    if (ctx == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    memset(ctx, 0, sizeof(DATASINK_CTX_T));
    *handle = (PLAYER_DATASINK)ctx;
    return OPRT_OK;
}

OPERATE_RET ai_player_datasink_deinit(PLAYER_DATASINK handle)
{
    DATASINK_CTX_T *ctx = (DATASINK_CTX_T *)handle;

    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }

    DATASINK_T *datasink = ctx->sink;
    if(datasink && datasink->exit) {
        datasink->exit(ctx->priv);
    }

    Free(ctx);
    return OPRT_OK;
}

OPERATE_RET ai_player_datasink_start(PLAYER_DATASINK handle, AI_PLAYER_SRC_E src, char *value)
{
    OPERATE_RET rt = OPRT_OK;
    DATASINK_T *datasink = NULL;
    DATASINK_CTX_T *ctx = (DATASINK_CTX_T *)handle;

    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }

    switch (src) {
#if defined(AI_PLAYER_DATASINK) && (AI_PLAYER_DATASINK & AI_PLAYER_DATASINK_MEM)
        case AI_PLAYER_SRC_MEM:
            datasink = &g_datasink_mem;
            break;
#endif
#if defined(AI_PLAYER_DATASINK) && (AI_PLAYER_DATASINK & AI_PLAYER_DATASINK_FILE)
        case AI_PLAYER_SRC_FILE:
            datasink = &g_datasink_file;
            break;
#endif
#if defined(AI_PLAYER_DATASINK) && (AI_PLAYER_DATASINK & AI_PLAYER_DATASINK_URL)
        case AI_PLAYER_SRC_URL:
            datasink = &g_datasink_url;
            break;
#endif
        default:
            return OPRT_INVALID_PARM;
    }

    if (datasink == NULL || datasink->start == NULL) {
        return OPRT_INVALID_PARM;
    }

    if(ctx->sink) {
        if(ctx->sink == datasink) {
            if(ctx->sink->stop) {
                ctx->sink->stop(ctx->priv);
            }
        } else {
            if(ctx->sink->exit) {
                ctx->sink->exit(ctx->priv);
            }
            ctx->priv = NULL;
        }
    }

    ctx->sink = datasink;
    rt = datasink->start(value, &ctx->priv);
    return rt;
}

OPERATE_RET ai_player_datasink_stop(PLAYER_DATASINK handle)
{
    DATASINK_CTX_T *ctx = (DATASINK_CTX_T *)handle;

    if (ctx == NULL || ctx->sink == NULL) {
        return OPRT_INVALID_PARM;
    }

    if(ctx->sink->stop) {
        return ctx->sink->stop(ctx->priv);
    }

    return OPRT_OK;
}

OPERATE_RET ai_player_datasink_feed(PLAYER_DATASINK handle, uint8_t *data, uint32_t len)
{
    DATASINK_CTX_T *ctx = (DATASINK_CTX_T *)handle;

    if (ctx == NULL || ctx->sink == NULL) {
        return OPRT_INVALID_PARM;
    }

    if(ctx->sink->feed) {
        return ctx->sink->feed(ctx->priv, data, len);
    }

    return OPRT_OK;
}

OPERATE_RET ai_player_datasink_read(PLAYER_DATASINK handle, uint8_t *buf, uint32_t len, uint32_t *out_len)
{
    DATASINK_CTX_T *ctx = (DATASINK_CTX_T *)handle;

    if (ctx == NULL || ctx->sink == NULL || ctx->sink->read == NULL) {
        return OPRT_INVALID_PARM;
    }

    return ctx->sink->read(ctx->priv, buf, len, out_len);
}