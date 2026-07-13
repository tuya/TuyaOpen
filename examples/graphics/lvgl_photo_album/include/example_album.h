/**
 * @file example_album.h
 * @brief Album management and browsing module for photo album example
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#ifndef __EXAMPLE_ALBUM_H__
#define __EXAMPLE_ALBUM_H__

#include "tuya_cloud_types.h"
#include "image_album.h"
#include "image_album_thumb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define EXAMPLE_ALBUM_NAME          "photo_album"
#define EXAMPLE_ALBUM_MAX_PHOTOS    20

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    char      name[ALBUM_FILENAME_MAX_LEN + 1];
    uint16_t  width;
    uint16_t  height;
    uint32_t  file_size;
    uint32_t  index;
    uint32_t  total;
} EXAMPLE_ALBUM_PHOTO_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */

/**
 * @brief Initialize image album
 * @return OPRT_OK on success
 */
OPERATE_RET example_album_init(void);

/**
 * @brief Save a JPEG photo to the album, auto-trim oldest if full
 * @param[in] jpeg_data JPEG file bytes
 * @param[in] jpeg_len JPEG data length
 * @return OPRT_OK on success
 */
OPERATE_RET example_album_save_photo(uint8_t *jpeg_data, uint32_t jpeg_len);

/**
 * @brief Get current photo count in album
 * @return count Photo count
 */
uint32_t example_album_get_count(void);

/**
 * @brief Start album browse session (retains album list)
 * @return OPRT_OK on success, OPRT_NOT_FOUND if album is empty
 */
OPERATE_RET example_album_browse_start(void);

/**
 * @brief Seek the browse session to a specific 0-based photo index.
 *
 * After this call, example_album_browse_next() will return the photo at @a idx.
 * The browse session must already be started.
 *
 * @param[in] idx  0-based target index
 * @return OPRT_OK on success, OPRT_INVALID_PARM if browse session not started
 */
OPERATE_RET example_album_browse_seek(uint32_t idx);

/**
 * @brief Get the next photo in browse session (wraps to first)
 * @param[out] photo Photo metadata (name, dimensions, index, total)
 * @param[out] jpeg_data Receives Malloc'd JPEG buffer (caller must call image_album_free_file_data)
 * @param[out] jpeg_len JPEG data length
 * @return OPRT_OK on success, OPRT_NOT_FOUND when no more photos
 */
OPERATE_RET example_album_browse_next(EXAMPLE_ALBUM_PHOTO_T *photo,
                                      uint8_t **jpeg_data, size_t *jpeg_len);

/**
 * @brief Get the previous photo in browse session (wraps to last)
 * @param[out] photo Photo metadata (name, dimensions, index, total)
 * @param[out] jpeg_data Receives Malloc'd JPEG buffer (caller must call image_album_free_file_data)
 * @param[out] jpeg_len JPEG data length
 * @return OPRT_OK on success, OPRT_NOT_FOUND when album is empty
 */
OPERATE_RET example_album_browse_prev(EXAMPLE_ALBUM_PHOTO_T *photo,
                                      uint8_t **jpeg_data, size_t *jpeg_len);


void example_album_release_jpeg_data(uint8_t *jpeg_data);

/**
 * @brief Delete a photo by filename and reposition the browse session
 * @param[in] filename Photo filename to delete
 * @return OPRT_OK on success
 */
OPERATE_RET example_album_delete_current(const char *filename);

/**
 * @brief End album browse session (releases album list)
 * @return none
 */
void example_album_browse_end(void);

/**
 * @brief Initialize a thumbnail iterator for the album.
 *
 * Uses the same storage backend selection as the browse APIs.
 * Must be paired with example_album_thumb_end().
 *
 * @param[out] iter  Receives the iterator handle
 * @return OPRT_OK on success
 */
OPERATE_RET example_album_thumb_start(ALBUM_THUMB_ITER_HANDLE *iter);

/**
 * @brief Deinitialize a thumbnail iterator created by example_album_thumb_start().
 * @param[in] iter  Iterator handle (may be NULL)
 */
void example_album_thumb_end(ALBUM_THUMB_ITER_HANDLE iter);

#ifdef __cplusplus
}
#endif

#endif /* __EXAMPLE_ALBUM_H__ */
