/**
 * @file storage_wrapper.c
 * @brief Implements storage operations using POSIX file system for Tuya SDK.
 *
 * This file provides the implementation of the storage interface defined in
 * storage_interface.h, specifically tailored for platforms with a
 * POSIX-compliant file system. It includes functions for setting (writing) and
 * getting (reading) data associated with a unique key into/from the persistent
 * storage. The implementation utilizes standard C file I/O operations to manage
 * data storage in a local directory, designated as the Tuya database path.
 * Error handling and logging are incorporated to ensure robustness and ease of
 * debugging.
 *
 * The storage operations are essential for applications that require persistent
 * storage of configuration parameters, operational states, or other data across
 * reboots or power cycles.
 *
 * @note This implementation assumes the availability of a POSIX-compliant file
 * system and may require modifications to work on platforms without standard C
 * file I/O support.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include "tuya_cloud_types.h"
#include "storage_interface.h"
#include "tal_log.h"

#define tuya_db_path "./tuyadb"

/**
 * @brief Sets the value of a key in the storage.
 *
 * This function sets the value of a key in the storage by writing the provided
 * buffer to a file.
 *
 * @param key The key to set.
 * @param buffer The buffer containing the value to set.
 * @param length The length of the buffer.
 * @return Returns OPRT_OK if the operation is successful, OPRT_INVALID_PARM if
 * the key or buffer is NULL, or OPRT_COM_ERROR if there is an error opening or
 * writing to the file.
 */
int posix_storage_set(const char *key, const uint8_t *buffer, size_t length)
{
    if (NULL == key || NULL == buffer) {
        return OPRT_INVALID_PARM;
    }

    FILE *fptr = NULL;

    char name[128];
    int written = snprintf(name, sizeof(name), "%s/%s", tuya_db_path, key);
    if (written < 0 || written >= (int)sizeof(name)) {
        PR_ERR("key path too long");
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    PR_DEBUG("key:%s", name);

    fptr = fopen(name, "wb+");
    if (NULL == fptr) {
        PR_ERR("open file error");
        return OPRT_COM_ERROR;
    } else {
        PR_DEBUG("open file OK");
    }

    int file_len = fwrite(buffer, 1, length, fptr);
    fclose(fptr);
    if (file_len != length) {
        PR_ERR("uf_kv_write fail %d", file_len);
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

/**
 * @brief Retrieves the value associated with a given key from the storage.
 *
 * This function reads the value associated with the specified key from the
 * storage and stores it in the provided buffer.
 *
 * @param key The key to retrieve the value for.
 * @param buffer The buffer to store the retrieved value.
 * @param length A pointer to the length of the buffer. On input, it specifies
 * the maximum number of bytes to read. On output, it contains the actual number
 * of bytes read.
 *
 * @return Returns OPRT_OK if the value was successfully retrieved, or an error
 * code if an error occurred. Possible error codes:
 *         - OPRT_INVALID_PARM: One or more of the input parameters is invalid
 * (key, buffer, or length is NULL).
 *         - OPRT_COM_ERROR: An error occurred while accessing the storage
 * (e.g., file not found, read error).
 */
int posix_storage_get(const char *key, uint8_t *buffer, size_t *length)
{
    if (NULL == key || NULL == buffer || NULL == length) {
        return OPRT_INVALID_PARM;
    }

    char name[128];
    int written = snprintf(name, sizeof(name), "%s/%s", tuya_db_path, key);
    if (written < 0 || written >= (int)sizeof(name)) {
        PR_ERR("key path too long");
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    PR_DEBUG("key:%s, len:%d", name, (int)*length);

    FILE *fptr = fopen(name, "rb");
    if (NULL == fptr) {
        *length = 0;
        PR_WARN("cannot open file");
        return OPRT_COM_ERROR;
    }

    int read_len = *length; // ?
    read_len = fread(buffer, 1, (size_t)read_len, fptr);
    fclose(fptr);
    if (read_len <= 0) {
        *length = 0;
        PR_ERR("read error %d", read_len);
        return OPRT_COM_ERROR;
    }

    *length = read_len;
    return OPRT_OK;
}

/**
 * @brief Deletes a file from the storage.
 *
 * This function deletes a file from the storage using the specified key.
 *
 * @param key The key of the file to be deleted.
 * @return Returns OPRT_OK if the file is deleted successfully, or
 * OPRT_COM_ERROR if there is an error.
 */
int posix_storage_del(const char *key)
{
    char name[128];
    int written = snprintf(name, sizeof(name), "%s/%s", tuya_db_path, key);
    if (written < 0 || written >= (int)sizeof(name)) {
        PR_ERR("key path too long");
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    PR_DEBUG("key:%s", name);
    if (remove(name) == 0) {
        PR_DEBUG("Deleted successfully");
        return OPRT_OK;
    } else {
        PR_ERR("Unable to delete the file");
        return OPRT_COM_ERROR;
    }
}

#ifdef __cplusplus
}
#endif
