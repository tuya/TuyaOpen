/**
 * @file datasink_mem.c
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
#include "tuya_ringbuf.h"

typedef struct {
    TUYA_RINGBUFF_T ringbuf;
    MUTEX_HANDLE mutex;
    bool eof;
} MEM_DATASINK_CTX_T;

OPERATE_RET datasink_mem_start(char *value, void* *handle)
{
    OPERATE_RET rt = OPRT_OK;
    MEM_DATASINK_CTX_T *ctx = (MEM_DATASINK_CTX_T *)(*handle);

    if(ctx != NULL) {
        tal_mutex_lock(ctx->mutex);
        tuya_ring_buff_reset(ctx->ringbuf);
        ctx->eof = false;
        tal_mutex_unlock(ctx->mutex);

        return OPRT_OK;
    }

    ctx = (MEM_DATASINK_CTX_T *)Malloc(sizeof(MEM_DATASINK_CTX_T));
    if (ctx == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    memset(ctx, 0, sizeof(MEM_DATASINK_CTX_T));
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&ctx->mutex));
    TUYA_CALL_ERR_RETURN(tuya_ring_buff_create(AI_PLAYER_RINGBUF_SIZE, OVERFLOW_STOP_TYPE, &ctx->ringbuf));

    *handle = (void*)ctx;
    return OPRT_OK;
}

OPERATE_RET datasink_mem_stop(void* handle)
{
    MEM_DATASINK_CTX_T *ctx = (MEM_DATASINK_CTX_T *)handle;
    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ctx->mutex);
    tuya_ring_buff_reset(ctx->ringbuf);
    tal_mutex_unlock(ctx->mutex);

    return OPRT_OK;
}

OPERATE_RET datasink_mem_exit(void* handle)
{
    MEM_DATASINK_CTX_T *ctx = (MEM_DATASINK_CTX_T *)handle;
    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ctx->mutex);
    tuya_ring_buff_free(ctx->ringbuf);
    ctx->ringbuf = NULL;
    tal_mutex_unlock(ctx->mutex);

    tal_mutex_release(ctx->mutex);
    Free(ctx);

    return OPRT_OK;
}

OPERATE_RET datasink_mem_feed(void* handle, uint8_t *data, uint32_t len)
{
    MEM_DATASINK_CTX_T *ctx = (MEM_DATASINK_CTX_T *)handle;
    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }

    if((data == NULL) && (len == 0)) { // eof
        tal_mutex_lock(ctx->mutex);
        ctx->eof = true;
        tal_mutex_unlock(ctx->mutex);
        return OPRT_OK;
    }

    if(data == NULL || len == 0) {
        return OPRT_INVALID_PARM;
    }

    int rt = 0;
    int cnt = 0;

    tal_mutex_lock(ctx->mutex);
    rt = tuya_ring_buff_write(ctx->ringbuf, (char *)data, len);
    tal_mutex_unlock(ctx->mutex);

    while(rt != len) {
        tal_system_sleep(10);

        data += rt;
        len -= rt;
        rt = 0;

        tal_mutex_lock(ctx->mutex);
        rt = tuya_ring_buff_write(ctx->ringbuf, data, len);
        tal_mutex_unlock(ctx->mutex);

        if(cnt ++ > 500) {
            PR_ERR("player ring buf write failed %d", rt);
            break;
        }
    }

    return OPRT_OK;
}

OPERATE_RET datasink_mem_read(void* handle, uint8_t *data, uint32_t len, uint32_t *out_len)
{
    MEM_DATASINK_CTX_T *ctx = (MEM_DATASINK_CTX_T *)handle;
    if (ctx == NULL || data == NULL || len == 0 || out_len == NULL) {
        return OPRT_INVALID_PARM;
    }

    int rt = 0;

    tal_mutex_lock(ctx->mutex);
    rt = tuya_ring_buff_read(ctx->ringbuf, (char *)data, len);
    tal_mutex_unlock(ctx->mutex);

    *out_len = rt;

    if((rt == 0) && (ctx->eof == true)) {
        return OPRT_NOT_FOUND; // eof
    } else {
        return OPRT_OK;
    }
}

DATASINK_T g_datasink_mem = {
    .start = datasink_mem_start,
    .stop  = datasink_mem_stop,
    .exit  = datasink_mem_exit,
    .feed  = datasink_mem_feed,
    .read  = datasink_mem_read,
};
