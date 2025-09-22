/**
 * @file tal_fs.h
 * @brief Defines the file system management system for Tuya IoT applications.
 *
 * This header file provides the definitions and API declarations for the file
 * system management system used in Tuya IoT applications. It includes functionalities
 * for creating, removing, renaming, reading, writing, and managing files and directories,
 * allowing components within the application to effectively manage and interact with
 * the file system. The file system management system is built on top of Tuya's IoT platform,
 * leveraging the platform's robust infrastructure for efficient and reliable file operations.
 *
 * The file system management system is crucial for developing responsive and modular
 * IoT applications that can store, retrieve, and manipulate data in a structured manner.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_FS_H__
#define __TAL_FS_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************************
 ********************************************************************************
 ********************************************************************************/

/**
 * @brief Make directory
 *
 * @param[in] path: path of directory
 *
 * @note This API is used for making a directory
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fs_mkdir(const char *path);

/**
 * @brief Remove directory
 *
 * @param[in] path: path of directory
 *
 * @note This API is used for removing a directory
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fs_remove(const char *path);

/**
 * @brief Get file mode
 *
 * @param[in] path: path of directory
 * @param[out] mode: bit attibute of directory
 *
 * @note This API is used for getting file mode.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fs_mode(const char *path, unsigned int *mode);

/**
 * @brief Check whether the file or directory exists
 *
 * @param[in] path: path of directory
 * @param[out] is_exist: the file or directory exists or not
 *
 * @note This API is used to check whether the file or directory exists.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fs_is_exist(const char *path, BOOL_T *is_exist);

/**
 * @brief File rename
 *
 * @param[in] path_old: old path of directory
 * @param[in] path_new: new path of directory
 *
 * @note This API is used to rename the file.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fs_rename(const char *path_old, const char *path_new);

/**
 * @brief Open directory
 *
 * @param[in] path: path of directory
 * @param[out] dir: handle of directory
 *
 * @note This API is used to open a directory
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_dir_open(const char *path, TUYA_DIR *dir);

/**
 * @brief Close directory
 *
 * @param[in] dir: handle of directory
 *
 * @note This API is used to close a directory
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_dir_close(TUYA_DIR dir);

/**
 * @brief Read directory
 *
 * @param[in] dir: handle of directory
 * @param[out] info: file information
 *
 * @note This API is used to read a directory.
 * Read the file information of the current node, and the internal pointer points to the next node.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_dir_read(TUYA_DIR dir, TUYA_FILEINFO *info);

/**
 * @brief Get the name of the file node
 *
 * @param[in] info: file information
 * @param[out] name: file name
 *
 * @note This API is used to get the name of the file node.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_dir_name(TUYA_FILEINFO info, const char **name);

/**
 * @brief Check whether the node is a directory
 *
 * @param[in] info: file information
 * @param[out] is_dir: is directory or not
 *
 * @note This API is used to check whether the node is a directory.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_dir_is_directory(TUYA_FILEINFO info, BOOL_T *is_dir);

/**
 * @brief Check whether the node is a normal file
 *
 * @param[in] info: file information
 * @param[out] is_regular: is normal file or not
 *
 * @note This API is used to check whether the node is a normal file.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */

int tal_dir_is_regular(TUYA_FILEINFO info, BOOL_T *is_regular);

/**
 * @brief Open file
 *
 * @param[in] path: path of file
 * @param[in] mode: file open mode: "r","w"...
 *
 * @note This API is used to open a file
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_FILE tal_fopen(const char *path, const char *mode);

/**
 * @brief Close file
 *
 * @param[in] file: file handle
 *
 * @note This API is used to close a file
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fclose(TUYA_FILE file);

/**
 * @brief Read file
 *
 * @param[in] buf: buffer for reading file
 * @param[in] bytes: buffer size
 * @param[in] file: file handle
 *
 * @note This API is used to read a file
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fread(void *buf, int bytes, TUYA_FILE file);

/**
 * @brief write file
 *
 * @param[in] buf: buffer for writing file
 * @param[in] bytes: buffer size
 * @param[in] file: file handle
 *
 * @note This API is used to write a file
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fwrite(void *buf, int bytes, TUYA_FILE file);

/**
 * @brief write buffer to flash
 *
 * @param[in] fd: file fd
 *
 * @note This API is used to write buffer to flash
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fsync(TUYA_FILE file);

/**
 * @brief Read string from file
 *
 * @param[in] buf: buffer for reading file
 * @param[in] len: buffer size
 * @param[in] file: file handle
 *
 * @note This API is used to read string from file
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
char *tal_fgets(char *buf, int len, TUYA_FILE file);

/**
 * @brief Check wheather to reach the end fo the file
 *
 * @param[in] file: file handle
 *
 * @note This API is used to check wheather to reach the end fo the file
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_feof(TUYA_FILE file);

/**
 * @brief Seek to the offset position of the file
 *
 * @param[in] file: file handle
 * @param[in] offs: offset
 * @param[in] whence: seek start point mode
 *
 * @note This API is used to seek to the offset position of the file.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fseek(TUYA_FILE file, int64_t offs, int whence);

/**
 * @brief Get current position of file
 *
 * @param[in] file: file handle
 *
 * @note This API is used to get current position of file.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int64_t tal_ftell(TUYA_FILE file);

/**
 * @brief Get file size
 *
 * @param[in] filepath file path + file name
 *
 * @note This API is used to get the size of file.
 *
 * @return the sizeof of file
 */
int tal_fgetsize(const char *filepath);

/**
 * @brief Judge if the file can be access
 *
 * @param[in] filepath file path + file name
 *
 * @param[in] mode access mode
 *
 * @note This API is used to access one file.
 *
 * @return 0 success,-1 failed
 */
int tal_faccess(const char *filepath, int mode);

/**
 * @brief read the next character from stream
 *
 * @param[in] file char stream
 *
 * @note This API is used to get one char from stream.
 *
 * @return as an unsigned char cast to a int ,or EOF on end of file or error
 */
int tal_fgetc(TUYA_FILE file);

/**
 * @brief flush the IO read/write stream
 *
 * @param[in] file char stream
 *
 * @note This API is used to flush the IO read/write stream.
 *
 * @return 0 success,-1 failed
 */
int tal_fflush(TUYA_FILE file);

/**
 * @brief get the file fd
 *
 * @param[in] file char stream
 *
 * @note This API is used to get the file fd.
 *
 * @return the file fd
 */
int tal_fileno(TUYA_FILE file);

/**
 * @brief truncate one file according to the length
 *
 * @param[in] fd file description
 *
 * @param[in] length the length want to truncate
 *
 * @note This API is used to truncate one file.
 *
 * @return 0 success,-1 failed
 */
int tal_ftruncate(int fd, uint64_t length);

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif
