/**
 * @file xteink_x4_sdcard.c
 * @brief microSD helpers for Xteink X4 using ESP-IDF SDSPI + FATFS APIs.
 * @version 0.1
 * @date 2026-04-30
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#include "xteink_x4_sdcard.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "tal_log.h"
#include "xteink_x4_spi.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TAG "x4_sd"

#define X4_SDCARD_MAX_PATH         256U
#define X4_SDCARD_DEFAULT_MAX_FILE 5
#define X4_SDCARD_SPI_FREQ_KHZ     20000

#define X4_IDF_SPI_HOST SPI2_HOST

static BOOL_T        s_sd_mounted;
static sdmmc_card_t *s_sd_card;

/**
 * @brief Map ESP-IDF return values to TuyaOpen return values.
 * @param[in] esp_rt ESP-IDF return value.
 * @return TuyaOpen return value.
 */
static OPERATE_RET __esp_to_oprt(esp_err_t esp_rt)
{
    switch (esp_rt) {
    case ESP_OK:
        return OPRT_OK;
    case ESP_ERR_NO_MEM:
        return OPRT_MALLOC_FAILED;
    case ESP_ERR_INVALID_ARG:
        return OPRT_INVALID_PARM;
    case ESP_ERR_INVALID_STATE:
        return OPRT_RESOURCE_NOT_READY;
    case ESP_ERR_NOT_FOUND:
        return OPRT_NOT_FOUND;
    default:
        return OPRT_COM_ERROR;
    }
}

/**
 * @brief Convert a root-relative SD path into a mounted VFS path.
 * @param[in] path root-relative path that must start with '/'.
 * @param[out] out_path output buffer.
 * @param[in] out_len output buffer length.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __make_vfs_path(const char *path, char *out_path, size_t out_len)
{
    int n = 0;

    if (NULL == path || NULL == out_path || out_len == 0U || path[0] != '/') {
        return OPRT_INVALID_PARM;
    }
    if (NULL != strstr(path, "/../") || NULL != strstr(path, "/..") || NULL != strstr(path, "../")) {
        return OPRT_INVALID_PARM;
    }

    if (0 == strcmp(path, "/")) {
        n = snprintf(out_path, out_len, "%s", X4_SDCARD_MOUNT_PATH);
    } else {
        n = snprintf(out_path, out_len, "%s%s", X4_SDCARD_MOUNT_PATH, path);
    }
    if (n < 0 || (size_t)n >= out_len) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    return OPRT_OK;
}

/**
 * @brief Mount the X4 microSD card with ESP-IDF SDSPI + FATFS APIs.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_mount(void)
{
    OPERATE_RET                rt     = OPRT_OK;
    esp_err_t                  esp_rt = ESP_OK;
    sdmmc_host_t               host   = SDSPI_HOST_DEFAULT();
    sdspi_device_config_t      slot   = SDSPI_DEVICE_CONFIG_DEFAULT();
    esp_vfs_fat_mount_config_t mount_config;

    if (s_sd_mounted) {
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(xteink_x4_spi_bus_init(4096U));

    host.slot         = X4_IDF_SPI_HOST;
    host.max_freq_khz = X4_SDCARD_SPI_FREQ_KHZ;

    slot.host_id = X4_IDF_SPI_HOST;
    slot.gpio_cs = (gpio_num_t)X4_SD_PIN_CS;
    slot.gpio_cd = SDSPI_SLOT_NO_CD;
    slot.gpio_wp = SDSPI_SLOT_NO_WP;

    mount_config                          = (esp_vfs_fat_mount_config_t)VFS_FAT_MOUNT_DEFAULT_CONFIG();
    mount_config.max_files                = X4_SDCARD_DEFAULT_MAX_FILE;
    mount_config.format_if_mount_failed   = false;
    mount_config.disk_status_check_enable = true;

    esp_rt = esp_vfs_fat_sdspi_mount(X4_SDCARD_MOUNT_PATH, &host, &slot, &mount_config, &s_sd_card);
    if (ESP_OK != esp_rt) {
        ESP_LOGW(TAG, "esp_vfs_fat_sdspi_mount failed: %s", esp_err_to_name(esp_rt));
        s_sd_card = NULL;
        return __esp_to_oprt(esp_rt);
    }

    s_sd_mounted = TRUE;
    sdmmc_card_print_info(stdout, s_sd_card);
    return OPRT_OK;
}

/**
 * @brief Unmount the X4 microSD card.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_unmount(void)
{
    esp_err_t esp_rt = ESP_OK;

    if (!s_sd_mounted) {
        return OPRT_OK;
    }

    esp_rt = esp_vfs_fat_sdcard_unmount(X4_SDCARD_MOUNT_PATH, s_sd_card);
    if (ESP_OK != esp_rt) {
        ESP_LOGW(TAG, "esp_vfs_fat_sdcard_unmount failed: %s", esp_err_to_name(esp_rt));
        return __esp_to_oprt(esp_rt);
    }

    s_sd_mounted = FALSE;
    s_sd_card    = NULL;
    return OPRT_OK;
}

/**
 * @brief Check whether the X4 microSD card is mounted.
 * @return true if mounted, otherwise false.
 */
bool xteink_x4_sdcard_ready(void)
{
    return s_sd_mounted ? true : false;
}

/**
 * @brief Read total and free capacity for the mounted card.
 * @param[out] total_bytes total filesystem bytes, may be NULL.
 * @param[out] free_bytes free filesystem bytes, may be NULL.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_get_usage(uint64_t *total_bytes, uint64_t *free_bytes)
{
    esp_err_t esp_rt = ESP_OK;

    if (!s_sd_mounted) {
        return OPRT_RESOURCE_NOT_READY;
    }

    esp_rt = esp_vfs_fat_info(X4_SDCARD_MOUNT_PATH, total_bytes, free_bytes);
    return __esp_to_oprt(esp_rt);
}

/**
 * @brief List files under a directory.
 * @param[in] path absolute path relative to SD root, for example "/" or "/dir".
 * @param[in] max_files maximum number of entries to report.
 * @param[in] cb callback invoked for each entry.
 * @param[in] user_data caller context passed to callback.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_list(const char *path, uint32_t max_files, X4_SDCARD_LIST_CB cb, void *user_data)
{
    DIR           *dir = NULL;
    struct dirent *ent = NULL;
    char           vfs_path[X4_SDCARD_MAX_PATH];
    char           item_path[X4_SDCARD_MAX_PATH];
    uint32_t       count = 0;
    OPERATE_RET    rt    = OPRT_OK;

    if (!s_sd_mounted) {
        return OPRT_RESOURCE_NOT_READY;
    }
    if (NULL == cb || max_files == 0U) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(__make_vfs_path(path, vfs_path, sizeof(vfs_path)));

    dir = opendir(vfs_path);
    if (NULL == dir) {
        return OPRT_DIR_OPEN_FAILED;
    }

    while ((ent = readdir(dir)) != NULL && count < max_files) {
        int n = snprintf(item_path, sizeof(item_path), "%s/%s", vfs_path, ent->d_name);
        if (n < 0 || (size_t)n >= sizeof(item_path)) {
            rt = OPRT_BUFFER_NOT_ENOUGH;
            break;
        }
        {
            struct stat st;
            bool        is_dir = false;

            (void)memset(&st, 0, sizeof(st));
            if (stat(item_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                is_dir = true;
            }
            rt = cb(item_path, is_dir, user_data);
        }
        if (OPRT_OK != rt) {
            break;
        }
        count++;
    }

    (void)closedir(dir);
    return rt;
}

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
                                                 size_t *bytes_read)
{
    FILE       *fp = NULL;
    char        vfs_path[X4_SDCARD_MAX_PATH];
    size_t      limit = 0;
    size_t      rd    = 0;
    OPERATE_RET rt    = OPRT_OK;

    if (!s_sd_mounted) {
        return OPRT_RESOURCE_NOT_READY;
    }
    if (NULL == buffer || buffer_size == 0U) {
        return OPRT_INVALID_PARM;
    }
    TUYA_CALL_ERR_RETURN(__make_vfs_path(path, vfs_path, sizeof(vfs_path)));

    limit = (max_bytes == 0U || max_bytes >= buffer_size) ? (buffer_size - 1U) : max_bytes;
    fp    = fopen(vfs_path, "rb");
    if (NULL == fp) {
        buffer[0] = '\0';
        return OPRT_FILE_OPEN_FAILED;
    }

    rd = fread(buffer, 1U, limit, fp);
    if (ferror(fp) != 0) {
        rt = OPRT_FILE_READ_FAILED;
    }
    buffer[rd] = '\0';
    if (NULL != bytes_read) {
        *bytes_read = rd;
    }

    (void)fclose(fp);
    return rt;
}

/**
 * @brief Write a buffer to a file, replacing existing contents.
 * @param[in] path absolute path relative to SD root.
 * @param[in] content data to write, may be NULL only when content_len is 0.
 * @param[in] content_len data length.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_write_file(const char *path, const char *content, size_t content_len)
{
    FILE       *fp = NULL;
    char        vfs_path[X4_SDCARD_MAX_PATH];
    size_t      wr = 0;
    OPERATE_RET rt = OPRT_OK;

    if (!s_sd_mounted) {
        return OPRT_RESOURCE_NOT_READY;
    }
    if (NULL == content && content_len != 0U) {
        return OPRT_INVALID_PARM;
    }
    TUYA_CALL_ERR_RETURN(__make_vfs_path(path, vfs_path, sizeof(vfs_path)));

    fp = fopen(vfs_path, "wb");
    if (NULL == fp) {
        return OPRT_FILE_OPEN_FAILED;
    }

    if (content_len != 0U) {
        wr = fwrite(content, 1U, content_len, fp);
        if (wr != content_len) {
            (void)fclose(fp);
            return OPRT_FILE_WRITE_FAILED;
        }
    }

    (void)fclose(fp);
    return OPRT_OK;
}

/**
 * @brief Ensure a directory exists.
 * @param[in] path absolute directory path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_ensure_dir(const char *path)
{
    char        vfs_path[X4_SDCARD_MAX_PATH];
    OPERATE_RET rt = OPRT_OK;

    if (!s_sd_mounted) {
        return OPRT_RESOURCE_NOT_READY;
    }
    TUYA_CALL_ERR_RETURN(__make_vfs_path(path, vfs_path, sizeof(vfs_path)));

    if (mkdir(vfs_path, 0775) == 0 || errno == EEXIST) {
        return OPRT_OK;
    }
    return OPRT_COM_ERROR;
}

/**
 * @brief Check whether a file or directory exists.
 * @param[in] path absolute path relative to SD root.
 * @return true if the path exists, otherwise false.
 */
bool xteink_x4_sdcard_exists(const char *path)
{
    char vfs_path[X4_SDCARD_MAX_PATH];

    if (!s_sd_mounted || OPRT_OK != __make_vfs_path(path, vfs_path, sizeof(vfs_path))) {
        return false;
    }
    return (access(vfs_path, F_OK) == 0) ? true : false;
}

/**
 * @brief Remove one file.
 * @param[in] path absolute path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_remove(const char *path)
{
    char        vfs_path[X4_SDCARD_MAX_PATH];
    OPERATE_RET rt = OPRT_OK;

    if (!s_sd_mounted) {
        return OPRT_RESOURCE_NOT_READY;
    }
    TUYA_CALL_ERR_RETURN(__make_vfs_path(path, vfs_path, sizeof(vfs_path)));
    return (remove(vfs_path) == 0) ? OPRT_OK : OPRT_COM_ERROR;
}

/**
 * @brief Remove one empty directory.
 * @param[in] path absolute directory path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_rmdir(const char *path)
{
    char        vfs_path[X4_SDCARD_MAX_PATH];
    OPERATE_RET rt = OPRT_OK;

    if (!s_sd_mounted) {
        return OPRT_RESOURCE_NOT_READY;
    }
    TUYA_CALL_ERR_RETURN(__make_vfs_path(path, vfs_path, sizeof(vfs_path)));
    return (rmdir(vfs_path) == 0) ? OPRT_OK : OPRT_COM_ERROR;
}

/**
 * @brief Rename a file or directory.
 * @param[in] old_path current absolute path relative to SD root.
 * @param[in] new_path new absolute path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_sdcard_rename(const char *old_path, const char *new_path)
{
    char        old_vfs_path[X4_SDCARD_MAX_PATH];
    char        new_vfs_path[X4_SDCARD_MAX_PATH];
    OPERATE_RET rt = OPRT_OK;

    if (!s_sd_mounted) {
        return OPRT_RESOURCE_NOT_READY;
    }
    TUYA_CALL_ERR_RETURN(__make_vfs_path(old_path, old_vfs_path, sizeof(old_vfs_path)));
    TUYA_CALL_ERR_RETURN(__make_vfs_path(new_path, new_vfs_path, sizeof(new_vfs_path)));
    return (rename(old_vfs_path, new_vfs_path) == 0) ? OPRT_OK : OPRT_COM_ERROR;
}
