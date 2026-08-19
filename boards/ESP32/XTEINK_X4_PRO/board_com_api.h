/**
 * @file board_com_api.h
 * @brief Xteink X4 Pro board-level APIs (ESP32-S3).
 * @version 0.1
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * @note All peripheral access goes through the TuyaOpen tkl_/tal_ wrappers.
 *       The only documented exceptions are the SDMMC mount (no TAL SDMMC
 *       wrapper exists) and the deep-sleep shutdown, both confined to
 *       xteink_x4_pro_sdcard.c / xteink_x4_pro.c respectively.
 */
#ifndef __BOARD_COM_API_H__
#define __BOARD_COM_API_H__

#include "tuya_cloud_types.h"
#include "xteink_x4_pro_battery.h"
#include "xteink_x4_pro_sdcard.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register all X4 Pro peripherals for TuyaOpen.
 *
 * Powers the rails in the required order (GPIO1 master rail first, then the
 * active-LOW GT911 enable on GPIO2), then initializes EPD, touch, frontlight,
 * buttons and the battery gauge. Safe to call once from the application.
 *
 * @return OPRT_OK on success.
 */
OPERATE_RET board_register_hardware(void);

/* ------------------------------------------------------------------ panel */

/**
 * @brief Viewable content rectangle inside the physical panel
 *        (FreeInk ViewableInsets: top=9, right=7, bottom=3, left=7).
 * @param[out] out_x left inset in panel coordinates (pixels).
 * @param[out] out_y top inset in panel coordinates (pixels).
 * @param[out] out_w width of the viewable rectangle.
 * @param[out] out_h height of the viewable rectangle.
 * @return OPRT_OK on success, OPRT_INVALID_PARM if any pointer is NULL.
 */
OPERATE_RET board_x4pro_panel_viewable_get(uint32_t *out_x, uint32_t *out_y, uint32_t *out_w, uint32_t *out_h);

/* -------------------------------------------------------------------- epd */

/**
 * @brief Initialize the SSD1677 EPD driver (tkl_spi + tkl_gpio only).
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_epd_init(void);

/**
 * @brief Clear EPD to white and run a full update.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_epd_clear(void);

/**
 * @brief Push a mono frame (800x480, 1 bit per pixel, row-major, MSB first).
 *
 * Uses the fast (differential) update mode when possible.
 *
 * @param[in] image non-NULL buffer of (X4PRO_EPD_WIDTH / 8) * X4PRO_EPD_HEIGHT bytes.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_epd_display(uint8_t *image);

/**
 * @brief Push framebuffer with a full e-ink refresh (use before deep sleep
 *        to avoid ghosting).
 * @param[in] image 1bpp framebuffer (same format as board_x4pro_epd_display).
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_epd_display_full_refresh(uint8_t *image);

/**
 * @brief Put the panel into SSD1677 deep sleep.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_epd_sleep(void);

/* ------------------------------------------------------------------ touch */

/**
 * @brief GT911 touch state snapshot.
 */
typedef struct {
    BOOL_T   pressed; /**< at least one finger down */
    uint16_t x;       /**< first touch point X in panel pixel coordinates */
    uint16_t y;       /**< first touch point Y in panel pixel coordinates */
    BOOL_T   home;    /**< capacitive Home key (GT911 status bit 0x10) */
    uint8_t  points;  /**< number of active touch points (0..5) */
} X4PRO_TOUCH_STATE_T;

/**
 * @brief Initialize the GT911 touch controller (tkl_i2c + tkl_gpio only).
 *
 * Performs the documented reset dance (RST low with INT low for address
 * selection, timed release) and verifies the controller answers on I2C.
 * The panel is mounted portrait: raw X/Y are swapped and Y is flipped.
 *
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_touch_init(void);

/**
 * @brief Poll the current touch state (non-blocking).
 * @param[out] state non-NULL receives the snapshot.
 * @return OPRT_OK on success, OPRT_INVALID_PARM if state is NULL.
 * @note The status register is cleared on every poll, so call this
 *       periodically (e.g. from the LVGL indev read callback).
 */
OPERATE_RET board_x4pro_touch_poll(X4PRO_TOUCH_STATE_T *state);

/* ------------------------------------------------------------- frontlight */

/**
 * @brief Initialize the warm/cold frontlight PWM pair (tkl_pwm).
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_frontlight_init(void);

/**
 * @brief Set overall frontlight brightness.
 * @param[in] percent 0..100 total brightness (split between cool and warm
 *                    LEDs according to the current warmth).
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_frontlight_set_brightness(uint8_t percent);

/**
 * @brief Set color temperature mix.
 * @param[in] percent 0 = all cool/white, 100 = all warm.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_frontlight_set_warmth(uint8_t percent);

/**
 * @brief Read back the current settings.
 * @param[out] brightness 0..100, may be NULL.
 * @param[out] warmth 0..100, may be NULL.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_frontlight_get(uint8_t *brightness, uint8_t *warmth);

/* ---------------------------------------------------------------- buttons */

/**
 * @brief Initialize the three digital buttons as pull-up inputs (tkl_gpio).
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_buttons_init(void);

/**
 * @brief Raw bitmask of pressed buttons (see X4PRO_BTN_* in
 *        xteink_x4_pro_buttons.h).
 * @param[out] state non-NULL receives the bitmask.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_buttons_get_state(uint8_t *state);

/* ---------------------------------------------------------------- battery */

/**
 * @brief Initialize the CW2017 fuel gauge and (re)load the BATINFO profile.
 *
 * The X4 Pro battery is sensed by a CW2017 I2C gauge at 0x63, not a bare ADC
 * divider. Without the 80-byte BATINFO profile the gauge reports 0%, so the
 * driver verifies and uploads the OEM profile on init.
 *
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_battery_init(void);

/**
 * @brief Read battery state of charge and cell voltage.
 * @param[out] voltage_mv cell voltage in millivolts, may be NULL.
 * @param[out] percentage 0..100, may be NULL.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_battery_read(uint32_t *voltage_mv, uint8_t *percentage);

/**
 * @brief Get the estimated charge state (idle / charging / full).
 *
 * No charger IC or VBUS sense pin exists on this board, so the battery
 * driver estimates the state from the VCELL slope (see
 * xteink_x4_pro_battery.h for the estimator behavior).
 *
 * @param[out] state non-NULL receives the state.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_battery_get_charge_state(X4PRO_CHARGE_STATE_E *state);

/**
 * @brief Register the charge-state change callback (single slot; NULL
 *        unregisters). Fires from the battery estimator task context.
 * @param[in] cb callback or NULL.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4pro_battery_on_charge_state(X4PRO_CHARGE_STATE_CB cb);

/* ---------------------------------------------------------------- sd card */

/**
 * @brief Power-cycle the SD data path and mount the card with SDMMC+FATFS.
 *
 * The card is silent to SPI-mode CMD0, so native SDMMC (1-bit, slot 1) is
 * used. This is the one documented ESP-IDF API usage in the BSP because no
 * TAL SDMMC wrapper exists. GPIO5 sequencing (HIGH 80 ms -> LOW 120 ms) and
 * a real sector-0 read validation with full-mount retry are performed.
 *
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4pro_sdcard_mount(void);

/**
 * @brief Unmount the microSD card.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4pro_sdcard_unmount(void);

/**
 * @brief Check whether the microSD card is mounted.
 * @return true if mounted, otherwise false.
 */
bool board_x4pro_sdcard_ready(void);

/**
 * @brief Read total and free capacity of the mounted card.
 * @param[out] total_bytes total filesystem bytes, may be NULL.
 * @param[out] free_bytes free filesystem bytes, may be NULL.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4pro_sdcard_get_usage(uint64_t *total_bytes, uint64_t *free_bytes);

/**
 * @brief List files under a directory.
 * @param[in] path absolute path relative to SD root, for example "/" or "/dir".
 * @param[in] max_files maximum number of entries to report.
 * @param[in] cb callback invoked for each entry.
 * @param[in] user_data caller context passed to callback.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4pro_sdcard_list(const char *path, uint32_t max_files, X4PRO_SDCARD_LIST_CB cb, void *user_data);

/**
 * @brief Read a file into a caller-provided buffer.
 * @param[in] path absolute path relative to SD root.
 * @param[out] buffer destination buffer.
 * @param[in] buffer_size destination buffer size.
 * @param[in] max_bytes optional maximum bytes to read, 0 means buffer_size - 1.
 * @param[out] bytes_read bytes read before null termination, may be NULL.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4pro_sdcard_read_file_to_buffer(const char *path, char *buffer, size_t buffer_size,
                                                   size_t max_bytes, size_t *bytes_read);

/**
 * @brief Write a buffer to a file, replacing existing contents.
 * @param[in] path absolute path relative to SD root.
 * @param[in] content data to write, may be NULL only when content_len is 0.
 * @param[in] content_len data length.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4pro_sdcard_write_file(const char *path, const char *content, size_t content_len);

/**
 * @brief Ensure a directory exists.
 * @param[in] path absolute directory path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4pro_sdcard_ensure_dir(const char *path);

/**
 * @brief Check whether a file or directory exists.
 * @param[in] path absolute path relative to SD root.
 * @return true if the path exists, otherwise false.
 */
bool board_x4pro_sdcard_exists(const char *path);

/**
 * @brief Remove one file.
 * @param[in] path absolute path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4pro_sdcard_remove(const char *path);

/**
 * @brief Remove one empty directory.
 * @param[in] path absolute directory path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4pro_sdcard_rmdir(const char *path);

/**
 * @brief Rename a file or directory.
 * @param[in] old_path current absolute path relative to SD root.
 * @param[in] new_path new absolute path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4pro_sdcard_rename(const char *old_path, const char *new_path);

/* ------------------------------------------------------------------ power */

/**
 * @brief Show nothing more, sleep the panel and enter deep sleep until the
 *        power key wakes the MCU.
 * @return Does not return if deep sleep succeeds.
 * @note Waits until the power key is released before arming wake-on-press.
 * @note The deep-sleep APIs themselves are ESP-IDF specific (no TAL deep
 *       sleep wrapper exists); the usage is confined to xteink_x4_pro.c.
 */
OPERATE_RET board_x4pro_power_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_COM_API_H__ */
