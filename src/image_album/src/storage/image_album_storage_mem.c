/**
 * @file image_album_storage_mem.c
 * @brief Memory storage backend for image album
 *
 * Stores picture data in RAM. The opaque @c path handle from save() is the
 * heap-allocated data buffer (same layout as bytes written).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include <string.h>
#include "image_album_storage.h"
#include "tal_log.h"
#include "tal_memory.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static IMAGE_ALBUM_STORAGE_INTFS_T sg_album_storage_mem;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __mem_init(char *album_name, void **handle)
{
    (void)album_name;
    (void)handle;
    return OPRT_OK;
}

/**
 * @brief Deinitialize memory storage backend
 * @param[in] handle Backend context
 * @return none
 */
static void __mem_deinit(void *handle)
{
    (void)handle;
}

/**
 * @brief Save picture data to memory
 * @param[in] handle Backend context
 * @param[in] filename Picture filename (unused)
 * @param[in] data Picture data bytes
 * @param[in] size Data size in bytes
 * @param[out] path Receives allocated buffer holding a copy of @a data
 * @return OPRT_OK on success
 */
static OPERATE_RET __mem_save(void *handle, const char *filename,
                              const uint8_t *data, size_t size, void **path)
{
    (void)filename;
    (void)handle;

    if (path == NULL) {
        return OPRT_INVALID_PARM;
    }
    *path = NULL;

    uint8_t *buf = (uint8_t *)Malloc(size);
    if (buf == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(buf, data, size);
    *path = buf;

    PR_NOTICE("save image, path:%p", buf);

    return OPRT_OK;
}

/**
 * @brief Read picture data from memory buffer
 * @param[in] handle Backend context
 * @param[in] path Buffer returned by save()
 * @param[in] size Number of bytes to copy
 * @param[out] buf Destination buffer
 * @return OPRT_OK on success
 */
static OPERATE_RET __mem_read(void *handle, void *path, size_t size, uint8_t *buf)
{
    (void)handle;

    if (path == NULL || buf == NULL) {
        return OPRT_INVALID_PARM;
    }

    memcpy(buf, (uint8_t *)path, size);
    return OPRT_OK;
}

/**
 * @brief Free in-memory picture buffer
 * @param[in] handle Backend context
 * @param[in] path Buffer from save()
 * @return OPRT_OK on success
 */
static OPERATE_RET __mem_remove(void *handle, void *path)
{
    (void)handle;

    if (path == NULL) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("remove image, path:%p", path);
    Free(path);
    return OPRT_OK;
}

/**
 * @brief Register memory storage backend
 * @return OPRT_OK on success
 */
OPERATE_RET image_album_storage_mem_register(void)
{
    memset(&sg_album_storage_mem, 0, sizeof(sg_album_storage_mem));
    sg_album_storage_mem.type   = IMAGE_ALBUM_STORAGE_TP_MEMORY;
    sg_album_storage_mem.init   = __mem_init;
    sg_album_storage_mem.deinit = __mem_deinit;
    sg_album_storage_mem.save   = __mem_save;
    sg_album_storage_mem.read   = __mem_read;
    sg_album_storage_mem.remove = __mem_remove;

    return image_album_register_storage(&sg_album_storage_mem);
}
