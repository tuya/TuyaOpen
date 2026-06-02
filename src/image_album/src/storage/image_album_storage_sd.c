/**
 * @file image_album_storage_sd.c
 * @brief SD card storage backend for image album
 *
 * Mounts the SD card (same flow as examples/peripherals/sd), ensures an album
 * directory under the mount point, and stores each picture as a file. The opaque
 * @c path handle returned by save() is a heap-allocated full path string
 * (contrast: memory backend uses @c path as data pointer).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
 #include <stdio.h>
 #include <string.h>
 #include "image_album_storage.h"
 #include "tal_log.h"
 #include "tal_memory.h"
 #include "tkl_fs.h"
 #include "mix_method.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define ALBUM_SD_MOUNT_PATH "/sdcard"

#ifndef ALBUM_SD_PATH_MAX_LEN
#define ALBUM_SD_PATH_MAX_LEN 128
#endif

#ifndef ALBUM_SD_ROOT_PATH_MAX_LEN
#define ALBUM_SD_ROOT_PATH_MAX_LEN 64
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char root_path[ALBUM_SD_ROOT_PATH_MAX_LEN+1];
} ALBUM_STORAGE_SD_CTX_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static IMAGE_ALBUM_STORAGE_INTFS_T sg_album_storage_sd;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Append @a filename to directory path @a dir_path
 */
 static OPERATE_RET __build_file_path(const char *dir_path, const char *filename,
                                      char *path, size_t path_len)
{
    if (dir_path == NULL || filename == NULL || path == NULL || path_len == 0) {
        return OPRT_INVALID_PARM;
    }

    size_t dir_len = strlen(dir_path);
    const char *sep = (dir_len > 0 && dir_path[dir_len - 1] == '/') ? "" : "/";
    int n = snprintf(path, path_len, "%s%s%s", dir_path, sep, filename);
    if (n < 0 || (size_t)n >= path_len) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    return OPRT_OK;
}

/**
 * @brief Initialize SD storage: mount SD, create album directory
 * @param[in] album_name Album subfolder under mount (may be NULL)
 * @param[out] handle Backend context
 * @return OPRT_OK on success
 */
static OPERATE_RET __sd_init(char *album_name, void **handle)
{
    OPERATE_RET rt = OPRT_OK;
    ALBUM_STORAGE_SD_CTX_T *ctx = NULL;

    rt = tkl_fs_mount(ALBUM_SD_MOUNT_PATH, DEV_SDCARD);
    if (rt != OPRT_OK) {
        PR_ERR("tkl_fs_mount %s failed, rt=%d", ALBUM_SD_MOUNT_PATH, rt);
        return rt;
    }

    ctx = (ALBUM_STORAGE_SD_CTX_T *)Malloc(sizeof(ALBUM_STORAGE_SD_CTX_T));
    if (ctx == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(ctx, 0, sizeof(ALBUM_STORAGE_SD_CTX_T));

    int w = snprintf(ctx->root_path, sizeof(ctx->root_path), "%s/%s", ALBUM_SD_MOUNT_PATH, album_name);
    if (w < 0 || (size_t)w >= sizeof(ctx->root_path)) {
        PR_ERR("snprintf failed: %s, expect %u got %d", ALBUM_SD_MOUNT_PATH, (unsigned)sizeof(ctx->root_path), w);
        Free(ctx);
        return OPRT_COM_ERROR;
    }

    BOOL_T is_exist =FALSE;

    tkl_fs_is_exist(ctx->root_path, &is_exist);
    if(!is_exist) {
        if(tkl_fs_mkdir(ctx->root_path) != 0) {
            PR_ERR("mkdir %s", ctx->root_path);
            Free(ctx);
            return OPRT_COM_ERROR;
        }
    }

    *handle = ctx;
    PR_DEBUG("sd storage init ok, path=%s", ctx->root_path);

    return OPRT_OK;
}

/**
 * @brief Free backend context (does not unmount SD)
 * @param[in] handle Backend context
 * @return none
 */
static void __sd_deinit(void *handle)
{
    if (handle == NULL) {
        return;
    }
    
    Free(handle);
}

/**
 * @brief Save picture to SD as a file; @a path receives allocated path string
 * @param[in] handle Backend context
 * @param[in] filename Picture filename key
 * @param[in] data Picture bytes
 * @param[in] size Data length
 * @param[out] path Full path (caller frees via remove())
 * @return OPRT_OK on success
 */
static OPERATE_RET __sd_save(void *handle, const char *filename,
                             const uint8_t *data, size_t size, void **path)
{
    if (path == NULL) {
        return OPRT_INVALID_PARM;
    }

    ALBUM_STORAGE_SD_CTX_T *ctx = (ALBUM_STORAGE_SD_CTX_T *)handle;
    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }

    char *full_path = (char *)Malloc(ALBUM_SD_PATH_MAX_LEN + 1);
    if (full_path == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(full_path, 0, ALBUM_SD_PATH_MAX_LEN + 1);

    OPERATE_RET rt = __build_file_path(ctx->root_path, filename, full_path, ALBUM_SD_PATH_MAX_LEN);
    if (rt != OPRT_OK) {
        PR_ERR("__build_file_path failed: %s, %s, rt: %d", ctx->root_path, filename, rt);
        Free(full_path);
        return rt;
    }

    TUYA_FILE fp = tkl_fopen(full_path, "w");
    if (NULL == fp) {
        PR_ERR("Open file %s failed", full_path);
        Free(full_path);
        return OPRT_FILE_OPEN_FAILED;
    }

    int written = tkl_fwrite((void *)data, (int)size, fp);
    tkl_fclose(fp);

    if (written != (int)size) {
        PR_ERR("tkl_fwrite failed: %s, expect %u got %d", full_path, (unsigned)size, written);
        tkl_fs_remove(full_path);
        return OPRT_FILE_WRITE_FAILED;
    }
    
    *path = full_path;

    return OPRT_OK;
}

/**
 * @brief Read picture bytes from file path
 * @param[in] handle Backend context (unused)
 * @param[in] path Full path (char *)
 * @param[in] size Bytes to read
 * @param[out] buf Output buffer
 * @return OPRT_OK on success
 */
static OPERATE_RET __sd_read(void *handle, void *path, size_t size, uint8_t *buf)
{
    (void)handle;

    if (path == NULL || buf == NULL || size == 0) {
        return OPRT_INVALID_PARM;
    }

    const char *fpath = (const char *)path;
    TUYA_FILE fp = tkl_fopen(fpath, "rb");
    if (fp == NULL) {
        return OPRT_FILE_OPEN_FAILED;
    }

    int rd = tkl_fread(buf, (int)size, fp);
    tkl_fclose(fp);

    if (rd != (int)size) {
        PR_ERR("tkl_fread failed: %s, expect %u got %d", fpath, (unsigned)size, rd);
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;   
}

/**
 * @brief Delete file and free path string
 * @param[in] handle Backend context (unused)
 * @param[in] path Full path allocated by save()
 * @return OPRT_OK on success
 */
static OPERATE_RET __sd_remove(void *handle, void *path)
{
    (void)handle;

    if (path == NULL) {
        return OPRT_INVALID_PARM;
    }

    char *fpath = (char *)path;
    if (fpath[0] != '\0') {
        tkl_fs_remove(fpath);
    }

    Free(fpath);
    
    return OPRT_OK;
}

/**
 * @brief Recover previously stored pictures from SD directory
 * @param[in] handle Backend context
 * @param[in] cb Per-item callback
 * @param[in] user_data Opaque context for @a cb
 * @return OPRT_OK on success (including empty directory)
 */
static OPERATE_RET __sd_recover(void *handle, ALBUM_STORAGE_RECOVER_CB cb, void *user_data)
{
    ALBUM_STORAGE_SD_CTX_T *ctx = (ALBUM_STORAGE_SD_CTX_T *)handle;

    if (ctx == NULL || cb == NULL) {
        return OPRT_INVALID_PARM;
    }

    TUYA_DIR dir = NULL;
    int rt = tkl_dir_open(ctx->root_path, &dir);
    if (rt != OPRT_OK || dir == NULL) {
        PR_DEBUG("sd recover: dir %s not found, skip", ctx->root_path);
        return OPRT_OK;
    }

    TUYA_FILEINFO info = NULL;
    while (tkl_dir_read(dir, &info) == OPRT_OK) {
        const char *name = NULL;
        if (tkl_dir_name(info, &name) != OPRT_OK || name == NULL || name[0] == '\0') {
            continue;
        }

        char *full_path = (char *)Malloc(ALBUM_SD_PATH_MAX_LEN + 1);
        if (full_path == NULL) {
            break;
        }
        memset(full_path, 0, ALBUM_SD_PATH_MAX_LEN + 1);

        if (__build_file_path(ctx->root_path, name, full_path, ALBUM_SD_PATH_MAX_LEN) != OPRT_OK) {
            Free(full_path);
            continue;
        }

        int fsize = tkl_fgetsize(full_path);
        if (fsize <= 0) {
            Free(full_path);
            continue;
        }

        ALBUM_STORAGE_RECOVER_ITEM_T item = {
            .filename  = name,
            .file_size = (size_t)fsize,
            .path      = full_path,
        };

        OPERATE_RET cb_rt = cb(&item, user_data);
        if (cb_rt != OPRT_OK) {
            Free(full_path);
            break;
        }
    }

    tkl_dir_close(dir);

    return OPRT_OK;
}

/**
 * @brief Register SD card storage backend
 * @return OPRT_OK on success
 */
OPERATE_RET image_album_storage_sd_register(void)
{
    memset(&sg_album_storage_sd, 0, sizeof(sg_album_storage_sd));
    sg_album_storage_sd.type    = IMAGE_ALBUM_STORAGE_TP_SD;
    sg_album_storage_sd.init    = __sd_init;
    sg_album_storage_sd.deinit  = __sd_deinit;
    sg_album_storage_sd.save    = __sd_save;
    sg_album_storage_sd.read    = __sd_read;
    sg_album_storage_sd.remove  = __sd_remove;
    sg_album_storage_sd.recover = __sd_recover;

    return image_album_register_storage(&sg_album_storage_sd);
}
