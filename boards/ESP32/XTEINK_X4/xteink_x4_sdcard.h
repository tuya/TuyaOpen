/**
 * @file xteink_x4_sdcard.h
 * @brief microSD helpers for Xteink X4.
 * @version 0.1
 * @date 2026-04-30
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __XTEINK_X4_SDCARD_H__
#define __XTEINK_X4_SDCARD_H__

#include "tuya_cloud_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define X4_SDCARD_MOUNT_PATH "/sdcard"

typedef OPERATE_RET (*X4_SDCARD_LIST_CB)(const char *path, bool is_dir, void *user_data);

/**
 * @brief Mount the X4 microSD card with ESP-IDF SDSPI + FATFS APIs.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_mount(void);

/**
 * @brief Unmount the X4 microSD card.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_unmount(void);

/**
 * @brief Check whether the X4 microSD card is mounted.
 * @return true if mounted, otherwise false.
 */
bool xteink_x4_sdcard_ready(void);

/**
 * @brief Read total and free capacity for the mounted card.
 * @param[out] total_bytes total filesystem bytes, may be NULL.
 * @param[out] free_bytes free filesystem bytes, may be NULL.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_get_usage(uint64_t *total_bytes, uint64_t *free_bytes);

/**
 * @brief List files under a directory.
 * @param[in] path absolute path relative to SD root, for example "/" or "/dir".
 * @param[in] max_files maximum number of entries to report.
 * @param[in] cb callback invoked for each entry.
 * @param[in] user_data caller context passed to callback.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_list(const char *path, uint32_t max_files, X4_SDCARD_LIST_CB cb, void *user_data);

/**
 * @brief Read a file into a caller-provided buffer.
 * @param[in] path absolute path relative to SD root.
 * @param[out] buffer destination buffer.
 * @param[in] buffer_size destination buffer size.
 * @param[in] max_bytes optional maximum bytes to read, 0 means buffer_size - 1.
 * @param[out] bytes_read bytes read before null termination, may be NULL.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_read_file_to_buffer(const char *path, char *buffer, size_t buffer_size, size_t max_bytes,
                                                 size_t *bytes_read);

/**
 * @brief Write a buffer to a file, replacing existing contents.
 * @param[in] path absolute path relative to SD root.
 * @param[in] content data to write, may be NULL only when content_len is 0.
 * @param[in] content_len data length.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_write_file(const char *path, const char *content, size_t content_len);

/**
 * @brief Ensure a directory exists.
 * @param[in] path absolute directory path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_ensure_dir(const char *path);

/**
 * @brief Check whether a file or directory exists.
 * @param[in] path absolute path relative to SD root.
 * @return true if the path exists, otherwise false.
 */
bool xteink_x4_sdcard_exists(const char *path);

/**
 * @brief Remove one file.
 * @param[in] path absolute path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_remove(const char *path);

/**
 * @brief Remove one empty directory.
 * @param[in] path absolute directory path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_rmdir(const char *path);

/**
 * @brief Rename a file or directory.
 * @param[in] old_path current absolute path relative to SD root.
 * @param[in] new_path new absolute path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_rename(const char *old_path, const char *new_path);

#ifdef __cplusplus
}
#endif
#endif /* __XTEINK_X4_SDCARD_H__ */
