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
#include "tal_fs.h"
#include "tal_api.h"

#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
#include "tkl_fs.h"
#else
#include "lfs.h"
#endif

#if !(defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1))
int __lfs_get_cfg(const char *mode)
{
    int flag = 0;

    // Iterate through the mode string and set the corresponding bit mask
    for (const char *p = mode; *p != '\0'; ++p) {
        switch (*p) {
        case 'r':
            if (*(p + 1) == '+') {
                flag |= LFS_O_RDWR;
                ++p; // Skip '+'
            } else {
                flag |= LFS_O_RDONLY;
            }
            break;
#ifndef LFS_READONLY
        case 'w':
            if (*(p + 1) == '+') {
                flag |= LFS_O_RDWR | LFS_O_CREAT | LFS_O_TRUNC;
                ++p; // Skip '+'
            } else {
                flag |= LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC;
            }
            break;
        case 'a':
            if (*(p + 1) == '+') {
                flag |= LFS_O_RDWR | LFS_O_CREAT | LFS_O_APPEND;
                ++p; // Skip '+'
            } else {
                flag |= LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND;
            }
            break;
        case 'x':
            flag |= LFS_O_EXCL;
            break;
        case 't':
            flag |= LFS_O_TRUNC;
            break;
#endif
        default:
            // Ignore other characters
            break;
        }
    }

    return flag;
}
#endif

/**
 * @brief Make directory
 *
 * @param[in] path: path of directory
 *
 * @note This API is used for making a directory
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fs_mkdir(const char *path)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fs_mkdir(path);
#else
    return lfs_mkdir(tal_lfs_get(), path);
#endif
}

/**
 * @brief Remove directory
 *
 * @param[in] path: path of directory
 *
 * @note This API is used for removing a directory
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fs_remove(const char *path)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fs_remove(path);
#else
    return lfs_remove(tal_lfs_get(), path);
#endif
}

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
int tal_fs_mode(const char *path, unsigned int *mode)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fs_mode(path, mode);
#else
    return OPRT_NOT_SUPPORTED;
#endif
}

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
int tal_fs_is_exist(const char *path, BOOL_T *is_exist)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fs_is_exist(path, is_exist);
#else
    struct lfs_info info;
    int rt = lfs_stat(tal_lfs_get(), path, &info);
    *is_exist = (rt < 0) ? 0 : 1;
    return OPRT_OK;
#endif
}

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
int tal_fs_rename(const char *path_old, const char *path_new)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fs_rename(path_old, path_new);
#else
    return lfs_rename(tal_lfs_get(), path_old, path_new);
#endif
}

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
int tal_dir_open(const char *path, TUYA_DIR *dir)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_dir_open(path, dir);
#else
    lfs_dir_t *d = tal_malloc(sizeof(lfs_dir_t));
    if (!d)
        return OPRT_MALLOC_FAILED;

    if (0 != lfs_dir_open(tal_lfs_get(), d, path)) {
        tal_free(d);
        return OPRT_DIR_OPEN_FAILED;
    }

    *dir = d;
    return OPRT_OK;
#endif
}

/**
 * @brief Close directory
 *
 * @param[in] dir: handle of directory
 *
 * @note This API is used to close a directory
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_dir_close(TUYA_DIR dir)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_dir_close(dir);
#else
    if (NULL == dir)
        return OPRT_OK;

    lfs_dir_close(tal_lfs_get(), dir);
    tal_free(dir);
    dir = NULL;

    return OPRT_OK;
#endif
}

/**
 * @brief Read directory
 *
 * @param[in] dir: handle of directory
 * @param[out] info: file information
 *
 * @note This API is used to read a directory.
 * Read the file information of the current node, and the internal pointer points to the next node.
 *
 * @note need read in loop until return OPRT_EOD
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_dir_read(TUYA_DIR dir, TUYA_FILEINFO *info)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_dir_read(dir, info);
#else
    struct lfs_info *dir_info = tal_malloc(sizeof(struct lfs_info));
    if (!dir_info)
        return OPRT_MALLOC_FAILED;

    int rt = lfs_dir_read(tal_lfs_get(), dir, dir_info);
    if (rt <= 0) {
        return rt == 0 ? OPRT_EOD : OPRT_DIR_READ_FAILED;
    }

    *info = dir_info;
    return OPRT_OK;
#endif
}

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
int tal_dir_name(TUYA_FILEINFO info, const char **name)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_dir_name(info, name);
#else
    if (NULL == info) {
        return OPRT_INVALID_PARM;
    }

    struct lfs_info *dir_info = (struct lfs_info *)info;
    *name = dir_info->name;
    return OPRT_OK;
#endif
}

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
int tal_dir_is_directory(TUYA_FILEINFO info, BOOL_T *is_dir)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_dir_is_directory(info, is_dir);
#else
    if (NULL == info) {
        return OPRT_INVALID_PARM;
    }

    struct lfs_info *dir_info = (struct lfs_info *)info;
    *is_dir = (dir_info->type == LFS_TYPE_DIR) ? 1 : 0;
    return OPRT_OK;
#endif
}

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

int tal_dir_is_regular(TUYA_FILEINFO info, BOOL_T *is_regular)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_dir_is_regular(info, is_regular);
#else
    if (NULL == info) {
        return OPRT_INVALID_PARM;
    }

    struct lfs_info *dir_info = (struct lfs_info *)info;
    *is_regular = (dir_info->type == LFS_TYPE_REG) ? 1 : 0;
    return OPRT_OK;
#endif
}

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
TUYA_FILE tal_fopen(const char *path, const char *mode)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fopen(path, mode);
#else
    lfs_file_t *f = tal_malloc(sizeof(lfs_file_t));
    if (!f)
        return NULL;

    memset(f, 0, sizeof(lfs_file_t));
    if (0 != lfs_file_open(tal_lfs_get(), f, path, __lfs_get_cfg(mode))) {
        tal_free(f);
        return NULL;
    }

    return f;
#endif
}

/**
 * @brief Close file
 *
 * @param[in] file: file handle
 *
 * @note This API is used to close a file
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fclose(TUYA_FILE file)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fclose(file);
#else
    if (NULL == file)
        return OPRT_OK;

    lfs_file_close(tal_lfs_get(), (lfs_file_t *)file);
    tal_free(file);
    file = NULL;
    return OPRT_OK;
#endif
}

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
int tal_fread(void *buf, int bytes, TUYA_FILE file)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fread(buf, bytes, file);
#else
    return lfs_file_read(tal_lfs_get(), (lfs_file_t *)file, buf, bytes);
#endif
}

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
int tal_fwrite(void *buf, int bytes, TUYA_FILE file)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fwrite(buf, bytes, file);
#else
    return lfs_file_write(tal_lfs_get(), (lfs_file_t *)file, buf, bytes);
#endif
}

/**
 * @brief write buffer to flash
 *
 * @param[in] fd: file fd
 *
 * @note This API is used to write buffer to flash
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_fsync(TUYA_FILE file)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fsync(file);
#else
    return lfs_file_sync(tal_lfs_get(), (lfs_file_t *)file);
#endif
}

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
char *tal_fgets(char *buf, int len, TUYA_FILE file)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fgets(buf, len, file);
#else
    int i = 0;
    char c;
    while (i < len - 1) {
        int rt = lfs_file_read(tal_lfs_get(), file, &c, 1);
        if (rt < 0) {
            return NULL;
        } else if (rt == 0) {
            break;
        } else {
            buf[i++] = c;
            if (c == '\n') {
                break;
            }
        }
    }

    buf[i] = '\0';
    if (i == 0 && c != '\n') {
        return NULL;
    }

    return buf;
#endif
}

/**
 * @brief Check wheather to reach the end fo the file
 *
 * @param[in] file: file handle
 *
 * @note This API is used to check wheather to reach the end fo the file
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tal_feof(TUYA_FILE file)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_feof(file);
#else
    char ch;
    if (0 == lfs_file_read(tal_lfs_get(), (lfs_file_t *)file, &ch, 1))
        return 1;

    // if not EOF, need seek back (read will change the offset)
    lfs_file_seek(tal_lfs_get(), (lfs_file_t *)file, -1, LFS_SEEK_CUR);
    return 0;
#endif
}

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
int tal_fseek(TUYA_FILE file, int64_t offs, int whence)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fseek(file, offs, whence);
#else
    return lfs_file_seek(tal_lfs_get(), (lfs_file_t *)file, offs, whence);
#endif
}

/**
 * @brief Get current position of file
 *
 * @param[in] file: file handle
 *
 * @note This API is used to get current position of file.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int64_t tal_ftell(TUYA_FILE file)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_ftell(file);
#else
    return lfs_file_tell(tal_lfs_get(), (lfs_file_t *)file);
#endif
}

/**
 * @brief Get file size
 *
 * @param[in] filepath file path + file name
 *
 * @note This API is used to get the size of file.
 *
 * @return the sizeof of file
 */
int tal_fgetsize(const char *filepath)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fgetsize(filepath);
#else
    struct lfs_info info;
    int err = lfs_stat(tal_lfs_get(), filepath, &info);
    if (err < 0) {
        return 0;
    }

    return (info.type == LFS_TYPE_REG) ? info.size : -1;
#endif
}

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
int tal_faccess(const char *filepath, int mode)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_faccess(filepath, mode);
#else
    return -1;
#endif
}

/**
 * @brief read the next character from stream
 *
 * @param[in] file char stream
 *
 * @note This API is used to get one char from stream.
 *
 * @return as an unsigned char cast to a int ,or EOF on end of file or error
 */
int tal_fgetc(TUYA_FILE file)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fgetc(file);
#else
    char ch;
    if (0 == lfs_file_read(tal_lfs_get(), (lfs_file_t *)file, &ch, 1))
        ch = EOF;
    return ch;
#endif
}

/**
 * @brief flush the IO read/write stream
 *
 * @param[in] file char stream
 *
 * @note This API is used to flush the IO read/write stream.
 *
 * @return 0 success,-1 failed
 */
int tal_fflush(TUYA_FILE file)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fflush(file);
#else
    return lfs_file_sync(tal_lfs_get(), (lfs_file_t *)file);
#endif
}

/**
 * @brief get the file fd
 *
 * @param[in] file char stream
 *
 * @note This API is used to get the file fd.
 *
 * @return the file fd
 */
int tal_fileno(TUYA_FILE file)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_fileno(file);
#else
    return OPRT_NOT_SUPPORTED;
#endif
}

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
int tal_ftruncate(int fd, uint64_t length)
{
#if defined(ENABLE_FILE_SYSTEM) && (ENABLE_FILE_SYSTEM == 1)
    return tkl_ftruncate(fd, length);
#else
    return OPRT_NOT_SUPPORTED;
#endif
}
