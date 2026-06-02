/**
 * @file image_album_priv.h
 * @brief Internal data structures for the image album component
 *
 * This file defines the private context structures, picture node layout,
 * and internal macros shared between image_album.c, image_album_scan.c,
 * and storage backend implementations. It is not intended for external inclusion.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __IMAGE_ALBUM_PRIV_H__
#define __IMAGE_ALBUM_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "image_album.h"
#include "image_album_scan.h"
#include "image_album_thumb.h"
#include "tal_memory.h"
#include "tal_mutex.h"

#include "tuya_list.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define ALBUM_PNG_SIGNATURE_LEN  8
#define ALBUM_PNG_IHDR_MIN_LEN   24

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Register built-in RAM storage backend
 * @return OPRT_OK on success
 */
 OPERATE_RET image_album_storage_mem_register(void);

 /**
  * @brief Register built-in SD card storage backend
  *
  * Mounts SD (non-Linux targets), creates @c ALBUM_SD_MOUNT_PATH/<album_name>,
  * then registers this backend with the album. Call before image_album_init()
  * or follow project init order as required.
  *
  * @return OPRT_OK on success
  */
 OPERATE_RET image_album_storage_sd_register(void);

/**
 * @brief Initialize storage backends selected by @a storage_mask
 * @param[in] album_name Album name
 * @param[in] storage_mask OR of backend types; 0 means @ref IMAGE_ALBUM_STORAGE_TP_ALL
 * @param[out] handle Dispatch handle
 * @return OPRT_OK on success, OPRT_NOT_FOUND if mask matches no registered backend
 */
OPERATE_RET album_storage_init(char *album_name, IMAGE_ALBUM_STORAGE_TP_E storage_mask, void **handle);

/**
 * @brief Deinitialize storage layer and release all backends
 * @param[in] handle Storage handle
 * @return none
 */
void album_storage_deinit(void *handle);

/**
 * @brief Save picture data to all active backends (one opaque path per registration slot)
 * @param[in] handle Storage dispatch handle
 * @param[in] filename Picture filename
 * @param[in] data Picture data bytes
 * @param[in] size Data size in bytes
 * @param[in,out] path_slots Array of @ref ALBUM_STORAGE_ENTRY_MAX_COUNT pointers; cleared then filled
 * @return OPRT_OK on success
 */
OPERATE_RET album_storage_save(void *handle, const char *filename,
                               const uint8_t *data, size_t size, void **path_slots);

/**
 * @brief Read picture data from backend(s) per @a storage_tp
 * @param[in] handle Storage dispatch handle
 * @param[in] path_slots Per-slot opaque paths from save
 * @param[in] size Expected size
 * @param[out] buf Destination buffer
 * @param[in] storage_tp @c 0 = any initialized slot; else filter by @c (intfs->type & storage_tp)
 * @return OPRT_OK on success, OPRT_NOT_FOUND if no backend has the data
 */
OPERATE_RET album_storage_read(void *handle, void **path_slots, size_t size, uint8_t *buf,
                               IMAGE_ALBUM_STORAGE_TP_E storage_tp);

/**
 * @brief Resolve opaque path for a committed picture on a selected backend (no auto mode)
 * @param[in] handle Storage dispatch handle
 * @param[in] path_slots Per-slot opaque paths from save
 * @param[in] storage_tp Non-zero mask; first registration-order slot with @c (type & storage_tp) and
 *            non-NULL path wins (same ordering as @ref album_storage_read with non-zero @a storage_tp)
 * @param[out] out_path Opaque path for that slot
 * @return OPRT_OK on success
 * @return OPRT_INVALID_PARM if @a storage_tp is zero or bad parameters
 * @return OPRT_NOT_FOUND if no matching initialized slot carries data for this picture
 */
OPERATE_RET album_storage_get_path_for_type(void *handle, void **path_slots,
                                            IMAGE_ALBUM_STORAGE_TP_E storage_tp, void **out_path);

/**
 * @brief Remove picture data from every slot that has a non-NULL path
 * @param[in] handle Storage dispatch handle
 * @param[in,out] path_slots Per-slot paths (nulled after remove)
 * @return none
 */
void album_storage_remove(void *handle, void **path_slots);

/**
 * @brief Recovered entry with storage slot index (dispatch → album core)
 */
typedef struct {
    const char *filename;
    size_t      file_size;
    void       *path;
    uint32_t    slot_index;
} ALBUM_RECOVER_ENTRY_T;

/**
 * @brief Per-entry callback from dispatch to album core during recovery
 * @param[in] entry Recovered entry (path ownership transfers on OPRT_OK)
 * @param[in] user_data Opaque context
 * @return OPRT_OK to continue, error to stop
 */
typedef OPERATE_RET (*ALBUM_RECOVER_ENTRY_CB)(const ALBUM_RECOVER_ENTRY_T *entry, void *user_data);

/**
 * @brief Recover stored items from backends matching @a recover_tp
 * @param[in] handle Storage dispatch handle
 * @param[in] recover_tp Backend type mask to recover from (0 = skip recovery)
 * @param[in] cb Per-entry callback with slot_index filled
 * @param[in] user_data Opaque context
 * @return OPRT_OK on success
 */
OPERATE_RET album_storage_recover(void *handle, IMAGE_ALBUM_STORAGE_TP_E recover_tp,
                                  ALBUM_RECOVER_ENTRY_CB cb, void *user_data);

/**
 * @brief Save to initialized backends whose path_slot is still NULL
 * @param[in] handle Storage dispatch handle
 * @param[in] filename Picture filename key
 * @param[in] data Picture file bytes
 * @param[in] size Data length
 * @param[in,out] path_slots Slots; non-NULL entries are skipped, new entries filled
 * @return OPRT_OK on success
 * @note Partial failures leave successfully-written slots intact (no rollback).
 */
OPERATE_RET album_storage_sync(void *handle, const char *filename,
                               const uint8_t *data, size_t size, void **path_slots);



#ifdef __cplusplus
}
#endif

#endif /* __IMAGE_ALBUM_PRIV_H__ */
