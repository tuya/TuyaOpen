/**
 * @file datasink_file.c
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
#include <string.h>
#include "tal_api.h"
#include "datasink_cfg.h"

typedef struct {
    lfs_file_t file;
    bool_t file_opened;
    int file_size;
} FILE_DATASINK_CTX_T;

OPERATE_RET datasink_file_start(char *value, void* *handle)
{
    FILE_DATASINK_CTX_T *ctx = (FILE_DATASINK_CTX_T *)(*handle);
    lfs_t *lfs = NULL;
    int ret = 0;

    if(ctx) {
        if(ctx->file_opened) {
            lfs_file_close(tal_lfs_get(), &ctx->file);
            ctx->file_opened = FALSE;
            ctx->file_size = 0;
        }
    } else {
        ctx = (FILE_DATASINK_CTX_T *)Malloc(sizeof(FILE_DATASINK_CTX_T));
        if (ctx == NULL) {
            return OPRT_MALLOC_FAILED;
        }
    }

    memset(ctx, 0, sizeof(FILE_DATASINK_CTX_T));
    
    lfs = tal_lfs_get();
    if (lfs == NULL) {
        if (*handle == NULL) {
            Free(ctx);
        }
        return OPRT_FILE_OPEN_FAILED;
    }

    ret = lfs_file_open(lfs, &ctx->file, value, LFS_O_RDONLY);
    if (ret != LFS_ERR_OK) {
        if (*handle == NULL) {
            Free(ctx);
        }
        return OPRT_FILE_OPEN_FAILED;
    }
    
    ctx->file_opened = TRUE;
    *handle = ctx;
    return OPRT_OK;
}

OPERATE_RET datasink_file_stop(void* handle)
{
    FILE_DATASINK_CTX_T *ctx = (FILE_DATASINK_CTX_T *)handle;
    if(!ctx) {
        return OPRT_INVALID_PARM;
    }

    if(ctx->file_opened) {
        lfs_file_close(tal_lfs_get(), &ctx->file);
        ctx->file_opened = FALSE;
        ctx->file_size = 0;
    }

    return OPRT_OK;
}

OPERATE_RET datasink_file_exit(void* handle)
{
    datasink_file_stop(handle);
    Free(handle);

    return OPRT_OK;
}

OPERATE_RET datasink_file_read(void* handle, uint8_t *data, uint32_t len, uint32_t *out_len)
{
    FILE_DATASINK_CTX_T *ctx = (FILE_DATASINK_CTX_T *)handle;
    lfs_t *lfs = NULL;
    lfs_ssize_t rt = 0;

    if(!ctx || !ctx->file_opened) {
        return OPRT_INVALID_PARM;
    }

    lfs = tal_lfs_get();
    if (lfs == NULL) {
        return OPRT_FILE_READ_FAILED;
    }

    rt = lfs_file_read(lfs, &ctx->file, data, len);
    if(rt == 0) {
        *out_len = 0;
        PR_DEBUG("eof file size %d", ctx->file_size);
        return OPRT_NOT_FOUND; // eof
    } else if(rt > 0) {
        *out_len = (uint32_t)rt;
        ctx->file_size += rt;
        return OPRT_OK;
    } else {
        PR_ERR("file read err %ld", (long)rt);
        return OPRT_FILE_READ_FAILED;
    }
}

DATASINK_T g_datasink_file = {
    .start = datasink_file_start,
    .stop  = datasink_file_stop,
    .exit  = datasink_file_exit,
    .feed  = NULL,
    .read  = datasink_file_read,
};
