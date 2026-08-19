/**
 * @file xteink_x4_pro_sdcard.c
 * @brief microSD helpers for Xteink X4 Pro using native SDMMC + FATFS.
 * @version 0.1
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * @note The X4 Pro card is silent to SPI-mode CMD0, so native SDMMC is the
 *       only working transport (confirmed on hardware: 1-bit, slot 1,
 *       CLK=41 CMD=42 DAT0=40, internal pull-ups, 40 MHz).
 * @note TAL gap: TuyaOpen has no native-SDMMC wrapper (tkl_fs_mount is
 *       SPI-mode only, which this card rejects), so this file is the one
 *       documented place that calls ESP-IDF SDMMC/VFS APIs, reduced to the
 *       irreducible three: esp_vfs_fat_sdmmc_mount / sdmmc_read_sectors /
 *       esp_vfs_fat_info (the ESP newlib ships no sys/statvfs.h, so POSIX
 *       statvfs is unavailable on this platform). Unmount goes through
 *       tkl_fs_unmount; every GPIO uses tkl_gpio and every file op POSIX.
 * @note No interrupts are used anywhere on this board: buttons, touch,
 *       EPD BUSY and the GT911 INT pin are all polled via tkl_gpio.
 * @note GPIO5 is the ACTIVE-LOW SD data-path enable: pulse it HIGH 80 ms ->
 *       LOW 120 ms before each mount attempt and keep it LOW afterwards —
 *       holding it HIGH breaks every block read with 0x107. Each attempt is
 *       validated with a real sector-0 read (FreeInk SdmmcBlockDevice flow).
 */
#include "xteink_x4_pro_sdcard.h"

#include "board_config.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tkl_fs.h"
#include "tkl_gpio.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TAG "x4pro_sd"

#define X4PRO_SDCARD_MAX_PATH         256U
#define X4PRO_SDCARD_DEFAULT_MAX_FILE 5
#define X4PRO_SDCARD_FREQ_KHZ         40000
#define X4PRO_SDCARD_MOUNT_ATTEMPTS   3U

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
 * @brief Drive the ACTIVE-LOW SD enable line.
 * @param[in] level pin level.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __pwr_write(TUYA_GPIO_LEVEL_E level)
{
    return tkl_gpio_write(X4PRO_SD_PIN_PWR, level);
}

/**
 * @brief Initialize the SD enable pin and run the OEM power pulse
 *        (HIGH 80 ms -> LOW 120 ms).
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __power_pulse(void)
{
    OPERATE_RET            rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T   cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.mode   = TUYA_GPIO_PUSH_PULL;
    cfg.direct = TUYA_GPIO_OUTPUT;
    cfg.level  = TUYA_GPIO_LEVEL_HIGH;

    TUYA_CALL_ERR_RETURN(tkl_gpio_init(X4PRO_SD_PIN_PWR, &cfg));
    tal_system_sleep(80);
    TUYA_CALL_ERR_RETURN(__pwr_write(TUYA_GPIO_LEVEL_LOW));
    tal_system_sleep(120);

    return OPRT_OK;
}

/**
 * @brief One mount attempt: mount the card and validate a real sector-0 read.
 * @return OPRT_OK on success, error code on failure (card handle cleaned up).
 */
static OPERATE_RET __mount_once(void)
{
    esp_err_t                  esp_rt = ESP_OK;
    sdmmc_host_t               host   = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t        slot   = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_mount_config_t mount_config;
    uint8_t                    sector0[512];

    host.slot         = 1; /* SDMMC slot 1 routed through the GPIO matrix */
    host.max_freq_khz = X4PRO_SDCARD_FREQ_KHZ;
    host.flags        = SDMMC_HOST_FLAG_1BIT;

    slot.clk   = (gpio_num_t)X4PRO_SD_PIN_CLK;
    slot.cmd   = (gpio_num_t)X4PRO_SD_PIN_CMD;
    slot.d0    = (gpio_num_t)X4PRO_SD_PIN_DAT0;
    slot.width = 1;
    slot.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    mount_config                          = (esp_vfs_fat_mount_config_t)VFS_FAT_MOUNT_DEFAULT_CONFIG();
    mount_config.max_files                = X4PRO_SDCARD_DEFAULT_MAX_FILE;
    mount_config.format_if_mount_failed   = false;
    mount_config.disk_status_check_enable = true;

    esp_rt = esp_vfs_fat_sdmmc_mount(X4PRO_SDCARD_MOUNT_PATH, &host, &slot, &mount_config, &s_sd_card);
    if (ESP_OK != esp_rt) {
        PR_WARN("[" TAG "] sdmmc mount failed: 0x%X", (unsigned)esp_rt);
        s_sd_card = NULL;
        return __esp_to_oprt(esp_rt);
    }

    /* A mount can succeed while reads are broken (0x107 class failures):
     * validate with a real sector-0 read before declaring the card usable. */
    esp_rt = sdmmc_read_sectors(s_sd_card, sector0, 0, 1);
    if (ESP_OK != esp_rt) {
        PR_WARN("[" TAG "] sector-0 validation failed: 0x%X", (unsigned)esp_rt);
        (void)tkl_fs_unmount(X4PRO_SDCARD_MOUNT_PATH);
        s_sd_card = NULL;
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET xteink_x4_pro_sdcard_mount(void)
{
    OPERATE_RET rt      = OPRT_OK;
    uint32_t    attempt = 0;

    if (s_sd_mounted) {
        return OPRT_OK;
    }

    for (attempt = 0; attempt < X4PRO_SDCARD_MOUNT_ATTEMPTS; attempt++) {
        TUYA_CALL_ERR_RETURN(__power_pulse());

        rt = __mount_once();
        if (OPRT_OK == rt) {
            uint32_t mb = 0;

            s_sd_mounted = TRUE;
            /* CID/CSD fields of the mounted handle: no extra ESP API call. */
            mb = (uint32_t)(((uint64_t)s_sd_card->csd.capacity * s_sd_card->csd.sector_size) / (1024U * 1024U));
            PR_NOTICE("[" TAG "] card mounted: name=%.6s, %lu MB, %lu kHz",
                      s_sd_card->cid.name, (unsigned long)mb, (unsigned long)(s_sd_card->max_freq_khz / 1000U));
            /* Keep the ACTIVE-LOW enable held LOW — HIGH breaks reads. */
            (void)__pwr_write(TUYA_GPIO_LEVEL_LOW);
            return OPRT_OK;
        }

        PR_WARN("[" TAG "] SD mount attempt %u failed", (unsigned)(attempt + 1U));
    }

    (void)__pwr_write(TUYA_GPIO_LEVEL_LOW);
    return rt;
}

OPERATE_RET xteink_x4_pro_sdcard_unmount(void)
{
    if (!s_sd_mounted) {
        return OPRT_OK;
    }

    /* tkl path-based unmount resolves the card registered at the mount
     * point, so the board code never calls esp_vfs_fat_sdcard_unmount. */
    if (OPRT_OK != tkl_fs_unmount(X4PRO_SDCARD_MOUNT_PATH)) {
        PR_WARN("[" TAG "] unmount failed");
        return OPRT_COM_ERROR;
    }

    s_sd_mounted = FALSE;
    s_sd_card    = NULL;
    return OPRT_OK;
}

bool xteink_x4_pro_sdcard_ready(void)
{
    return s_sd_mounted ? true : false;
}

OPERATE_RET xteink_x4_pro_sdcard_get_usage(uint64_t *total_bytes, uint64_t *free_bytes)
{
    esp_err_t esp_rt = ESP_OK;
    uint64_t  total  = 0;
    uint64_t  free_b = 0;

    if (!s_sd_mounted) {
        return OPRT_RESOURCE_NOT_READY;
    }
    if (NULL == total_bytes || NULL == free_bytes) {
        return OPRT_INVALID_PARM;
    }

    /* ESP newlib has no sys/statvfs.h, so the FATFS usage query stays on
     * the documented ESP-IDF path (esp_vfs_fat_info). */
    esp_rt = esp_vfs_fat_info(X4PRO_SDCARD_MOUNT_PATH, &total, &free_b);
    if (ESP_OK != esp_rt) {
        return OPRT_COM_ERROR;
    }

    *total_bytes = total;
    *free_bytes  = free_b;
    return OPRT_OK;
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
        n = snprintf(out_path, out_len, "%s", X4PRO_SDCARD_MOUNT_PATH);
    } else {
        n = snprintf(out_path, out_len, "%s%s", X4PRO_SDCARD_MOUNT_PATH, path);
    }
    if (n < 0 || (size_t)n >= out_len) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    return OPRT_OK;
}

OPERATE_RET xteink_x4_pro_sdcard_list(const char *path, uint32_t max_files, X4PRO_SDCARD_LIST_CB cb, void *user_data)
{
    DIR           *dir = NULL;
    struct dirent *ent = NULL;
    char           vfs_path[X4PRO_SDCARD_MAX_PATH];
    char           item_path[X4PRO_SDCARD_MAX_PATH];
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

OPERATE_RET xteink_x4_pro_sdcard_read_file_to_buffer(const char *path, char *buffer, size_t buffer_size,
                                                     size_t max_bytes, size_t *bytes_read)
{
    FILE       *fp    = NULL;
    char        vfs_path[X4PRO_SDCARD_MAX_PATH];
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

OPERATE_RET xteink_x4_pro_sdcard_write_file(const char *path, const char *content, size_t content_len)
{
    FILE       *fp = NULL;
    char        vfs_path[X4PRO_SDCARD_MAX_PATH];
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

OPERATE_RET xteink_x4_pro_sdcard_ensure_dir(const char *path)
{
    char        vfs_path[X4PRO_SDCARD_MAX_PATH];
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

bool xteink_x4_pro_sdcard_exists(const char *path)
{
    char vfs_path[X4PRO_SDCARD_MAX_PATH];

    if (!s_sd_mounted || OPRT_OK != __make_vfs_path(path, vfs_path, sizeof(vfs_path))) {
        return false;
    }
    return (access(vfs_path, F_OK) == 0) ? true : false;
}

OPERATE_RET xteink_x4_pro_sdcard_remove(const char *path)
{
    char        vfs_path[X4PRO_SDCARD_MAX_PATH];
    OPERATE_RET rt = OPRT_OK;

    if (!s_sd_mounted) {
        return OPRT_RESOURCE_NOT_READY;
    }
    TUYA_CALL_ERR_RETURN(__make_vfs_path(path, vfs_path, sizeof(vfs_path)));
    return (remove(vfs_path) == 0) ? OPRT_OK : OPRT_COM_ERROR;
}

OPERATE_RET xteink_x4_pro_sdcard_rmdir(const char *path)
{
    char        vfs_path[X4PRO_SDCARD_MAX_PATH];
    OPERATE_RET rt = OPRT_OK;

    if (!s_sd_mounted) {
        return OPRT_RESOURCE_NOT_READY;
    }
    TUYA_CALL_ERR_RETURN(__make_vfs_path(path, vfs_path, sizeof(vfs_path)));
    return (rmdir(vfs_path) == 0) ? OPRT_OK : OPRT_COM_ERROR;
}

OPERATE_RET xteink_x4_pro_sdcard_rename(const char *old_path, const char *new_path)
{
    char        old_vfs_path[X4PRO_SDCARD_MAX_PATH];
    char        new_vfs_path[X4PRO_SDCARD_MAX_PATH];
    OPERATE_RET rt = OPRT_OK;

    if (!s_sd_mounted) {
        return OPRT_RESOURCE_NOT_READY;
    }
    TUYA_CALL_ERR_RETURN(__make_vfs_path(old_path, old_vfs_path, sizeof(old_vfs_path)));
    TUYA_CALL_ERR_RETURN(__make_vfs_path(new_path, new_vfs_path, sizeof(new_vfs_path)));
    return (rename(old_vfs_path, new_vfs_path) == 0) ? OPRT_OK : OPRT_COM_ERROR;
}
