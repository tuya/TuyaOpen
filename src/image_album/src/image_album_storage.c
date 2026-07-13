/**
 * @file image_album_storage.c
 * @brief Storage backend dispatch — per-slot handles and init-time backend selection
 *
 * Registration fills global slots with (type, vtable). @ref album_storage_init allocates a
 * dispatch object and calls @c init only for backends whose @c type appears in @c storage_mask.
 * Save/remove fan out to slots that finished @c init successfully; @c priv[i] may stay NULL for
 * stateless backends (e.g. memory). Each slot keeps its own opaque @c path entry in a fixed-size
 * array parallel to @ref ALBUM_STORAGE_ENTRY_MAX_COUNT.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "image_album_priv.h"
#include <string.h>
#include "tal_api.h"

/***********************************************************
************************macro define***********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Per-album dispatch: private handle per registration slot (may be NULL) and init flags
 */
typedef struct {
    void    *priv[ALBUM_STORAGE_ENTRY_MAX_COUNT];
    bool_t   inited[ALBUM_STORAGE_ENTRY_MAX_COUNT];
} ALBUM_STORAGE_CTX_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static const IMAGE_ALBUM_STORAGE_INTFS_T *sg_album_storage_intfs[ALBUM_STORAGE_ENTRY_MAX_COUNT];

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Roll back inits for slots [0, @a up_to)
 * @param[in] up_to Exclusive upper slot index
 * @return none
 */
static void __album_storage_rollback_inits(ALBUM_STORAGE_CTX_T *ctx, uint32_t up_to)
{
    for (uint32_t i = 0; i < up_to; i++) {
        if (ctx->inited[i] == FALSE) {
            continue;
        }
        if (sg_album_storage_intfs[i] != NULL && sg_album_storage_intfs[i]->deinit != NULL) {
            sg_album_storage_intfs[i]->deinit(ctx->priv[i]);
        }
        ctx->priv[i] = NULL;
        ctx->inited[i] = FALSE;
    }
}

static void __album_storage_rollback_registers(void)
{
#if defined(ENABLE_IMAGE_ALBUM_STORAGE_MEM) && (ENABLE_IMAGE_ALBUM_STORAGE_MEM)
    image_album_storage_mem_register();
#endif

#if defined(ENABLE_IMAGE_ALBUM_STORAGE_SD) && (ENABLE_IMAGE_ALBUM_STORAGE_SD)
    image_album_storage_sd_register();
#endif
}



/**
 * @brief Initialize storage backends selected by @a storage_mask
 * @param[in] album_name Album name
 * @param[in] storage_mask OR of backend types; 0 means @ref IMAGE_ALBUM_STORAGE_TP_ALL
 * @param[out] handle Dispatch handle
 * @return OPRT_OK on success, OPRT_NOT_FOUND if mask matches no registered backend
 */
OPERATE_RET album_storage_init(char *album_name, IMAGE_ALBUM_STORAGE_TP_E storage_mask, void **handle)
{
    OPERATE_RET rt;
    ALBUM_STORAGE_CTX_T *ctx = NULL;
    uint32_t i;
    uint32_t activated;
    static bool_t is_registered = false;

    if (false == is_registered) {
        __album_storage_rollback_registers();
        is_registered = true;
    }

    if (handle == NULL || storage_mask == 0) {
        return OPRT_INVALID_PARM;
    }

    ctx = (ALBUM_STORAGE_CTX_T *)tal_malloc(sizeof(ALBUM_STORAGE_CTX_T));
    if (ctx == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(ctx, 0, sizeof(ALBUM_STORAGE_CTX_T));

    activated = 0;
    for (i = 0; i < ALBUM_STORAGE_ENTRY_MAX_COUNT; i++) {
        if (sg_album_storage_intfs[i] == NULL) {
            continue;
        }
        if ((storage_mask & sg_album_storage_intfs[i]->type) == 0) {
            continue;
        }

        rt = sg_album_storage_intfs[i]->init(album_name, &ctx->priv[i]);
        if (rt != OPRT_OK) {
            PR_ERR("album:%s storage tp:0x%x init failed, rt=%d", album_name,
                   (unsigned)sg_album_storage_intfs[i]->type, rt);
            __album_storage_rollback_inits(ctx, i);
            Free(ctx);
            return rt;
        }
        ctx->inited[i] = TRUE;
        activated++;
    }

    if (activated == 0) {
        PR_ERR("album:%s no storage backend for mask 0x%x", album_name, (unsigned)storage_mask);
        Free(ctx);
        return OPRT_NOT_FOUND;
    }

    *handle = ctx;

    return OPRT_OK;
}

/**
 * @brief Deinitialize storage layer and release all backends used by this dispatch handle
 * @param[in] handle Dispatch handle from @ref album_storage_init
 * @return none
 */
void album_storage_deinit(void *handle)
{
    ALBUM_STORAGE_CTX_T *ctx = (ALBUM_STORAGE_CTX_T *)handle;
    uint32_t i;

    if (handle == NULL) {
        return;
    }

    for (i = 0; i < ALBUM_STORAGE_ENTRY_MAX_COUNT; i++) {
        const IMAGE_ALBUM_STORAGE_INTFS_T *intfs = sg_album_storage_intfs[i];

        if (ctx->inited[i] == FALSE) {
            continue;
        }
        if (intfs != NULL && intfs->deinit != NULL) {
            intfs->deinit(ctx->priv[i]);
        }
        ctx->priv[i] = NULL;
        ctx->inited[i] = FALSE;
    }

    tal_free(ctx);
}

OPERATE_RET album_storage_save(void *handle, const char *filename,
                               const uint8_t *data, size_t size, void **path_slots)
{
    ALBUM_STORAGE_CTX_T *ctx = (ALBUM_STORAGE_CTX_T *)handle;
    OPERATE_RET rt;
    uint32_t i;
    bool_t any = FALSE;

    if (handle == NULL || path_slots == NULL) {
        return OPRT_INVALID_PARM;
    }

    // memset(path_slots, 0, sizeof(void *) * ALBUM_STORAGE_ENTRY_MAX_COUNT);

    for (i = 0; i < ALBUM_STORAGE_ENTRY_MAX_COUNT; i++) {
        const IMAGE_ALBUM_STORAGE_INTFS_T *intfs = sg_album_storage_intfs[i];

        if (ctx->inited[i] == FALSE || intfs == NULL || intfs->save == NULL) {
            continue;
        }

        any = TRUE;
        rt = intfs->save(ctx->priv[i], filename, data, size, &path_slots[i]);
        if (rt != OPRT_OK) {
            PR_ERR("file:%s storage tp:0x%x save failed, rt=%d", filename, (unsigned)intfs->type, rt);
            for (uint32_t j = 0; j < i; j++) {
                const IMAGE_ALBUM_STORAGE_INTFS_T *pj = sg_album_storage_intfs[j];
                if (ctx->inited[j] && path_slots[j] != NULL && pj != NULL && pj->remove != NULL) {
                    pj->remove(ctx->priv[j], path_slots[j]);
                    path_slots[j] = NULL;
                }
            }
            return rt;
        }
    }

    if (!any) {
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

/**
 * @brief Read picture data from backends selected by @a storage_tp
 * @param[in] handle Dispatch handle
 * @param[in] path_slots Per-slot opaque handles
 * @param[in] size Expected size
 * @param[out] buf Buffer to fill
 * @param[in] storage_tp 0 = all active slots in order; else require @c (intfs->type & storage_tp) != 0
 * @return OPRT_OK on success
 */
OPERATE_RET album_storage_read(void *handle, void **path_slots, size_t size, uint8_t *buf,
                               IMAGE_ALBUM_STORAGE_TP_E storage_tp)
{
    ALBUM_STORAGE_CTX_T *ctx = (ALBUM_STORAGE_CTX_T *)handle;
    OPERATE_RET last_rt = OPRT_NOT_FOUND;
    uint32_t i;

    if (handle == NULL || path_slots == NULL) {
        return OPRT_INVALID_PARM;
    }

    for (i = 0; i < ALBUM_STORAGE_ENTRY_MAX_COUNT; i++) {
        const IMAGE_ALBUM_STORAGE_INTFS_T *intfs = sg_album_storage_intfs[i];

        if (ctx->inited[i] == FALSE || intfs == NULL || intfs->read == NULL) {
            continue;
        }

        if (storage_tp != 0 && (intfs->type & storage_tp) == 0) {
            continue;
        }

        if (path_slots[i] == NULL) {
            continue;
        }

        last_rt = intfs->read(ctx->priv[i], path_slots[i], size, buf);
        if (last_rt == OPRT_OK) {
            return OPRT_OK;
        }
    }

    return last_rt;
}

/**
 * @brief Resolve opaque path for a selected backend (registration slot order)
 * @param[in] handle Dispatch handle
 * @param[in] path_slots Per-slot paths
 * @param[in] storage_tp Non-zero backend selector mask
 * @param[out] out_path Matching slot path
 * @return OPRT_OK on success, OPRT_INVALID_PARM or OPRT_NOT_FOUND
 */
OPERATE_RET album_storage_get_path_for_type(void *handle, void **path_slots,
                                            IMAGE_ALBUM_STORAGE_TP_E storage_tp, void **out_path)
{
    ALBUM_STORAGE_CTX_T *ctx = (ALBUM_STORAGE_CTX_T *)handle;
    uint32_t i;

    if (handle == NULL || path_slots == NULL || out_path == NULL) {
        return OPRT_INVALID_PARM;
    }
    *out_path = NULL;
    if (storage_tp == 0) {
        return OPRT_INVALID_PARM;
    }

    for (i = 0; i < ALBUM_STORAGE_ENTRY_MAX_COUNT; i++) {
        const IMAGE_ALBUM_STORAGE_INTFS_T *intfs = sg_album_storage_intfs[i];

        if (ctx->inited[i] == FALSE || intfs == NULL) {
            continue;
        }
        if ((intfs->type & storage_tp) == 0) {
            continue;
        }
        if (path_slots[i] == NULL) {
            continue;
        }
        *out_path = path_slots[i];
        return OPRT_OK;
    }

    return OPRT_NOT_FOUND;
}

/**
 * @brief Remove picture data from all initialized backends for this handle
 * @param[in] handle Dispatch handle
 * @param[in,out] path_slots Per-slot opaque handles (cleared after remove)
 * @return none
 */
void album_storage_remove(void *handle, void **path_slots)
{
    ALBUM_STORAGE_CTX_T *ctx = (ALBUM_STORAGE_CTX_T *)handle;
    uint32_t i;

    if (handle == NULL || path_slots == NULL) {
        return;
    }

    for (i = 0; i < ALBUM_STORAGE_ENTRY_MAX_COUNT; i++) {
        const IMAGE_ALBUM_STORAGE_INTFS_T *intfs = sg_album_storage_intfs[i];

        if (ctx->inited[i] == FALSE || intfs == NULL || intfs->remove == NULL) {
            continue;
        }

        if (path_slots[i] != NULL) {
            intfs->remove(ctx->priv[i], path_slots[i]);
            path_slots[i] = NULL;
        }
    }
}

/**
 * @brief Save to initialized backends whose path_slot is still NULL
 * @param[in] handle Storage dispatch handle
 * @param[in] filename Picture filename key
 * @param[in] data Picture file bytes
 * @param[in] size Data length
 * @param[in,out] path_slots Slots; non-NULL entries are skipped, new entries filled
 * @return OPRT_OK on success
 */
OPERATE_RET album_storage_sync(void *handle, const char *filename,
                               const uint8_t *data, size_t size, void **path_slots)
{
    ALBUM_STORAGE_CTX_T *ctx = (ALBUM_STORAGE_CTX_T *)handle;
    uint32_t i;

    if (handle == NULL || path_slots == NULL || data == NULL || size == 0) {
        return OPRT_INVALID_PARM;
    }

    for (i = 0; i < ALBUM_STORAGE_ENTRY_MAX_COUNT; i++) {
        const IMAGE_ALBUM_STORAGE_INTFS_T *intfs = sg_album_storage_intfs[i];

        if (ctx->inited[i] == FALSE || intfs == NULL || intfs->save == NULL) {
            continue;
        }

        if (path_slots[i] != NULL) {
            continue;
        }

        OPERATE_RET rt = intfs->save(ctx->priv[i], filename, data, size, &path_slots[i]);
        if (rt != OPRT_OK) {
            PR_WARN("sync file:%s storage tp:0x%x failed, rt=%d",
                    filename, (unsigned)intfs->type, rt);
        }
    }

    return OPRT_OK;
}

/**
 * @brief Wrapper context for recover dispatch (adds slot_index to each item)
 */
typedef struct {
    uint32_t              slot_index;
    ALBUM_RECOVER_ENTRY_CB real_cb;
    void                  *real_user_data;
} __ALBUM_RECOVER_WRAP_T;

/**
 * @brief Wrapper callback that augments each item with slot_index
 * @param[in] item Raw item from backend
 * @param[in] user_data __ALBUM_RECOVER_WRAP_T pointer
 * @return forwarded return from real callback
 */
static OPERATE_RET __album_recover_wrap_cb(const ALBUM_STORAGE_RECOVER_ITEM_T *item, void *user_data)
{
    __ALBUM_RECOVER_WRAP_T *wrap = (__ALBUM_RECOVER_WRAP_T *)user_data;

    ALBUM_RECOVER_ENTRY_T entry = {
        .filename   = item->filename,
        .file_size  = item->file_size,
        .path       = item->path,
        .slot_index = wrap->slot_index,
    };

    return wrap->real_cb(&entry, wrap->real_user_data);
}

/**
 * @brief Recover stored items from backends matching @a recover_tp
 * @param[in] handle Storage dispatch handle
 * @param[in] recover_tp Backend type mask to recover from (0 = skip recovery)
 * @param[in] cb Per-entry callback with slot_index filled
 * @param[in] user_data Opaque context
 * @return OPRT_OK on success
 */
OPERATE_RET album_storage_recover(void *handle, IMAGE_ALBUM_STORAGE_TP_E recover_tp,
                                  ALBUM_RECOVER_ENTRY_CB cb, void *user_data)
{
    ALBUM_STORAGE_CTX_T *ctx = (ALBUM_STORAGE_CTX_T *)handle;
    uint32_t i;

    if (handle == NULL || cb == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (recover_tp == 0) {
        return OPRT_OK;
    }

    for (i = 0; i < ALBUM_STORAGE_ENTRY_MAX_COUNT; i++) {
        const IMAGE_ALBUM_STORAGE_INTFS_T *intfs = sg_album_storage_intfs[i];

        if (ctx->inited[i] == FALSE || intfs == NULL || intfs->recover == NULL) {
            continue;
        }

        if ((intfs->type & recover_tp) == 0) {
            continue;
        }

        __ALBUM_RECOVER_WRAP_T wrap = {
            .slot_index     = i,
            .real_cb        = cb,
            .real_user_data = user_data,
        };

        OPERATE_RET rt = intfs->recover(ctx->priv[i], __album_recover_wrap_cb, &wrap);
        if (rt != OPRT_OK) {
            PR_ERR("storage tp:0x%x recover failed, rt=%d", (unsigned)intfs->type, rt);
            return rt;
        }
    }

    return OPRT_OK;
}

/**
 * @brief Register a storage backend to the global slot table
 * @param[in] intfs Backend interface ( @a type must be non-zero )
 * @return OPRT_OK on success
 */
OPERATE_RET image_album_register_storage(const IMAGE_ALBUM_STORAGE_INTFS_T *intfs)
{
    uint32_t i;

    if (intfs == NULL || intfs->type == 0) {
        return OPRT_INVALID_PARM;
    }

    for (i = 0; i < ALBUM_STORAGE_ENTRY_MAX_COUNT; i++) {
        if (sg_album_storage_intfs[i] == NULL) {
            sg_album_storage_intfs[i] = intfs;
            return OPRT_OK;
        }
    }

    return OPRT_EXCEED_UPPER_LIMIT;
}
