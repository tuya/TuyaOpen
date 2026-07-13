/**
 * @file image_album_storage.h
 * @brief Image Album Storage Backend Interface
 *
 * Defines the abstract storage backend interface for the image album component.
 * Built-in backends include memory and filesystem storage. Third-party backends
 * can be implemented by filling in this interface and registering via
 * image_album_register_storage().
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __IMAGE_ALBUM_STORAGE_H__
#define __IMAGE_ALBUM_STORAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define ALBUM_STORAGE_ENTRY_MAX_COUNT  4

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef uint32_t IMAGE_ALBUM_STORAGE_TP_E;
#define IMAGE_ALBUM_STORAGE_TP_MEMORY       (BIT(0))
#define IMAGE_ALBUM_STORAGE_TP_SD           (BIT(1))
#define IMAGE_ALBUM_STORAGE_TP_CUSTOM       (BIT(8))

/**
 * @brief Bitmask of all built-in type bits (extend when adding backends)
 */
#define IMAGE_ALBUM_STORAGE_TP_ALL          0xFFFFFFFF

/**
 * @brief Recovered item descriptor returned by @ref recover callback
 */
typedef struct {
    const char *filename;
    size_t      file_size;
    void       *path;
} ALBUM_STORAGE_RECOVER_ITEM_T;

/**
 * @brief Per-item callback for storage recovery
 * @param[in] item Recovered item (path ownership transfers to caller on OPRT_OK)
 * @param[in] user_data Opaque context
 * @return OPRT_OK to continue iteration, error to stop
 */
typedef OPERATE_RET (*ALBUM_STORAGE_RECOVER_CB)(const ALBUM_STORAGE_RECOVER_ITEM_T *item, void *user_data);

/**
 * @brief Storage backend operations interface
 *
 * Each storage backend (memory, filesystem, etc.) implements this interface.
 * The album core dispatches storage operations through these function pointers.
 * Set @ref type to the matching @ref IMAGE_ALBUM_STORAGE_TP_* for @ref album_storage_init mask.
 */
typedef struct {
    /** Backend kind; used with @c storage_mask in @ref IMAGE_ALBUM_INIT_CFG_T */
    IMAGE_ALBUM_STORAGE_TP_E type;

    /**
     * @brief Initialize backend
     * @param[in] cfg Album configuration (backend extracts relevant fields)
     * @param[out] handle Output backend context handle
     * @return OPRT_OK on success
     */
    OPERATE_RET (*init)(char *album_name, void **handle);

    /**
     * @brief Deinitialize backend and free all resources
     * @param[in] handle Backend context handle
     * @return none
     */
    void (*deinit)(void *handle);

    /**
     * @brief Save picture data to this backend
     * @param[in] handle Backend context handle
     * @param[in] filename Picture filename as unique key
     * @param[in] data Picture data bytes
     * @param[in] size Data size in bytes
     * @param[out] path path of the picture
     * @return OPRT_OK on success
     */
    OPERATE_RET (*save)(void *handle, const char *filename,
                        const uint8_t *data, size_t size,\
                        void **path);

    /**
     * @brief Read picture data from this backend
     * @param[in] handle Backend context handle
     * @param[in] path path of the picture
     * @param[in] size expected size 
     * @param[out] buf Buffer to store picture data
     * @return OPRT_OK on success
     */
    OPERATE_RET (*read)(void *handle, void *path, size_t size, uint8_t *buf);

    /**
     * @brief Remove picture data from this backend
     * @param[in] handle Backend context handle
     * @return OPRT_OK on success
     */
    OPERATE_RET (*remove)(void *handle, void *path);

    /**
     * @brief Recover previously stored items from this backend
     * @param[in] handle Backend context handle
     * @param[in] cb Per-item callback; path ownership transfers on OPRT_OK return
     * @param[in] user_data Opaque context passed to @a cb
     * @return OPRT_OK on success (including no items to recover)
     * @note Optional; NULL means this backend does not support recovery.
     */
    OPERATE_RET (*recover)(void *handle, ALBUM_STORAGE_RECOVER_CB cb, void *user_data);
} IMAGE_ALBUM_STORAGE_INTFS_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Register a storage backend to the album
 *
 * @param[in] intfs Backend interface vtable 
 *
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET image_album_register_storage(const IMAGE_ALBUM_STORAGE_INTFS_T *intfs);

#ifdef __cplusplus
}
#endif

#endif /* __IMAGE_ALBUM_STORAGE_H__ */
