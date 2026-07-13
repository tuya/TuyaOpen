/**
 * @file ai_picture.h
 * @brief Picture album management interface for Ai AI device.
 *        Provides album open/close, photo navigation, thumbnail generation,
 *        and batch operations built on top of the image_album component.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_PICTURE_H__
#define __AI_PICTURE_H__

#include "tuya_cloud_types.h"
#include "image_album.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_PICTURE_ALBUM_STORAGE_MASK IMAGE_ALBUM_STORAGE_TP_ALL

/**
 * @brief Backend selector for @ref image_album_get_next_item / @ref image_album_item_retain_locked paths.
 *        Ai picture uses in-memory payload pointer from the memory backend.
 */
#define AI_PICTURE_GET_STORAGE_TP IMAGE_ALBUM_STORAGE_TP_MEMORY

/**
 * @brief Storage backend used as source of truth for image list recovery at init.
 *        Set to @ref IMAGE_ALBUM_STORAGE_TP_SD when persistent SD storage is enabled,
 *        0 when no persistent storage (image list starts empty on each boot).
 */
#if defined(ENABLE_IMAGE_ALBUM_STORAGE_SD) && (ENABLE_IMAGE_ALBUM_STORAGE_SD)
#define AI_PICTURE_ALBUM_RECOVER_TP IMAGE_ALBUM_STORAGE_TP_SD
#else
#define AI_PICTURE_ALBUM_RECOVER_TP 0
#endif

#ifndef ALBUM_FILENAME_MAX_LEN
#define AI_PICTURE_NAME_MAX_LEN 64
#else
#define AI_PICTURE_NAME_MAX_LEN ALBUM_FILENAME_MAX_LEN
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize the picture album module
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_init(void);

/**
 * @brief Save a JPEG picture to the album
 * @param[in] picture JPEG data buffer
 * @param[in] len JPEG data length in bytes
 * @param[in] in_name desired filename; NULL or empty string to auto-generate a timestamp-based name
 * @param[out] name filled with the actual filename used (may be NULL)
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_save_to_album(uint8_t *picture, uint32_t len, const char *in_name, char name[AI_PICTURE_NAME_MAX_LEN + 1]);


char *ai_picture_get_album_name(void);

#ifdef __cplusplus
}
#endif

#endif /* __WUKONG_PICTURE_H__ */
