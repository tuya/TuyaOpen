/**
 * @file board_com_api.h
 * @brief Xteink X4 board-level APIs (ESP32-C3).
 * @version 0.1
 * @date 2026-04-30
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __BOARD_COM_API_H__
#define __BOARD_COM_API_H__

#include "tuya_cloud_types.h"
#include "xteink_x4_sdcard.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Classified first-boot / wake cause aligned with CrossPoint Reader HalGPIO::WakeupReason.
 */
typedef enum {
    X4_WAKEUP_CLASS_POWER_BUTTON = 0,
    X4_WAKEUP_CLASS_AFTER_FLASH,
    X4_WAKEUP_CLASS_AFTER_USB_POWER,
    X4_WAKEUP_CLASS_OTHER,
} X4_WAKEUP_CLASS_E;

/**
 * @brief Viewable content rectangle inside the physical panel (CrossPoint AA / bezel margins).
 * @param[out] out_x left inset in panel coordinates (pixels).
 * @param[out] out_y top inset in panel coordinates (pixels).
 * @param[out] out_w width of the viewable rectangle.
 * @param[out] out_h height of the viewable rectangle.
 * @return OPRT_OK on success, OPRT_INVALID_PARM if any pointer is NULL.
 */
OPERATE_RET board_x4_panel_viewable_get(uint32_t *out_x, uint32_t *out_y, uint32_t *out_w, uint32_t *out_h);

/**
 * @brief Classify ESP32 wake/reset into CrossPoint-style categories (USB sense via charge GPIO).
 * @param[out] out_class non-NULL receives the classification.
 * @return OPRT_OK on success, OPRT_INVALID_PARM if out_class is NULL.
 */
OPERATE_RET board_x4_sleep_classify_wakeup(X4_WAKEUP_CLASS_E *out_class);

/**
 * @brief After GPIO deep sleep wake, optionally require a minimum PWR hold (CrossPoint verifyPowerButtonWakeup).
 * @param[in] required_duration_ms minimum time PWR must stay low to continue (e.g. 3000).
 * @param[in] short_press_allowed if TRUE, returns immediately without checking duration.
 * @return OPRT_OK if verification passes.
 * @note If verification fails, calls board_x4_power_shutdown() and does not return.
 * @note Call only when esp_sleep_get_wakeup_cause() indicates a GPIO wake if you mirror CrossPoint main().
 */
OPERATE_RET board_x4_power_verify_gpio_wake(uint32_t required_duration_ms, BOOL_T short_press_allowed);

/**
 * @brief Register X4 peripherals for TuyaOpen (ADC, GPIO, soft-SPI EPD stack, SD CS).
 * @return OPRT_OK on success.
 */
OPERATE_RET board_register_hardware(void);

/**
 * @brief Initialize battery ADC (GPIO0 divider sense).
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4_battery_adc_init(void);

/**
 * @brief Read battery terminal voltage (after divider correction) and estimated percentage.
 * @param[out] voltage_mv battery voltage in millivolts (may be NULL).
 * @param[out] percentage 0-100 (may be NULL).
 * @return OPRT_OK on success.
 * @note Percentage curve is adapted from OpenX4 community-sdk BatteryMonitor (MIT).
 */
OPERATE_RET board_x4_battery_read(uint32_t *voltage_mv, uint8_t *percentage);

/**
 * @brief Initialize resistor-ladder button inputs (GPIO1/2 analog, GPIO3 power).
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4_buttons_init(void);

/**
 * @brief Raw bitmask of logical buttons (see X4_BTN_* in xteink_x4_buttons.h).
 * @param[out] state non-NULL receives bitmask.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4_buttons_get_state(uint8_t *state);

/**
 * @brief Prepare SD card chip-select and MISO as GPIO.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4_sdcard_gpio_prepare(void);

/**
 * @brief Mount the X4 microSD card with ESP-IDF SDSPI + FATFS APIs.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4_sdcard_mount(void);

/**
 * @brief Unmount the X4 microSD card.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4_sdcard_unmount(void);

/**
 * @brief Check whether the X4 microSD card is mounted.
 * @return true if mounted, otherwise false.
 */
bool board_x4_sdcard_ready(void);

/**
 * @brief Read total and free capacity for the mounted card.
 * @param[out] total_bytes total filesystem bytes, may be NULL.
 * @param[out] free_bytes free filesystem bytes, may be NULL.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4_sdcard_get_usage(uint64_t *total_bytes, uint64_t *free_bytes);

/**
 * @brief List files under a directory.
 * @param[in] path absolute path relative to SD root, for example "/" or "/dir".
 * @param[in] max_files maximum number of entries to report.
 * @param[in] cb callback invoked for each entry.
 * @param[in] user_data caller context passed to callback.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4_sdcard_list(const char *path, uint32_t max_files, X4_SDCARD_LIST_CB cb, void *user_data);

/**
 * @brief Read a file into a caller-provided buffer.
 * @param[in] path absolute path relative to SD root.
 * @param[out] buffer destination buffer.
 * @param[in] buffer_size destination buffer size.
 * @param[in] max_bytes optional maximum bytes to read, 0 means buffer_size - 1.
 * @param[out] bytes_read bytes read before null termination, may be NULL.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4_sdcard_read_file_to_buffer(const char *path, char *buffer, size_t buffer_size, size_t max_bytes,
                                                size_t *bytes_read);

/**
 * @brief Write a buffer to a file, replacing existing contents.
 * @param[in] path absolute path relative to SD root.
 * @param[in] content data to write, may be NULL only when content_len is 0.
 * @param[in] content_len data length.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4_sdcard_write_file(const char *path, const char *content, size_t content_len);

/**
 * @brief Ensure a directory exists.
 * @param[in] path absolute directory path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4_sdcard_ensure_dir(const char *path);

/**
 * @brief Check whether a file or directory exists.
 * @param[in] path absolute path relative to SD root.
 * @return true if the path exists, otherwise false.
 */
bool board_x4_sdcard_exists(const char *path);

/**
 * @brief Remove one file.
 * @param[in] path absolute path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4_sdcard_remove(const char *path);

/**
 * @brief Remove one empty directory.
 * @param[in] path absolute directory path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4_sdcard_rmdir(const char *path);

/**
 * @brief Rename a file or directory.
 * @param[in] old_path current absolute path relative to SD root.
 * @param[in] new_path new absolute path relative to SD root.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_x4_sdcard_rename(const char *old_path, const char *new_path);

/**
 * @brief Optional USB charge sense (GPIO20).
 * @param[out] charging true if charger appears active (HIGH on GPIO20 per common X4 wiring).
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4_charge_sense_get(bool *charging);

/**
 * @brief Initialize the OpenX4-derived SSD1677 driver for X4 pins.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4_epd_init(void);

/**
 * @brief Clear EPD to white and run full update.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4_epd_clear(void);

/**
 * @brief Push mono frame (800x480, 1 bit per pixel, row-major, MSB first per byte).
 * @param[in] image non-NULL buffer of size (X4_EPD_WIDTH / 8) * X4_EPD_HEIGHT (driver may read only).
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4_epd_display(uint8_t *image);

/**
 * @brief Push framebuffer with a full e-ink refresh (use before deep sleep to avoid ghosting).
 * @param[in] image 1bpp framebuffer (same as board_x4_epd_display).
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4_epd_display_full_refresh(uint8_t *image);

/**
 * @brief Deep sleep panel to save power.
 * @return OPRT_OK on success.
 */
OPERATE_RET board_x4_epd_sleep(void);

/**
 * @brief Show UI then enter deep sleep until the power key wakes the MCU.
 * @return Does not return if deep sleep succeeds.
 * @note Waits until the power key is released before arming wake-on-press; otherwise immediate wake/restart.
 * @note Hardware power latch may fully remove VDD; in that case the next boot is cold reset.
 *       If the MCU stays powered, GPIO wake is armed on the power key (active low).
 * @note CrossPoint Reader HalPowerManager::startDeepSleep() additionally drives GPIO13 latch hold; this BSP
 *       targets the stock X4 wiring and does not assert that path unless added at board level.
 */
OPERATE_RET board_x4_power_shutdown(void);

/**
 * @brief After GPIO deep sleep wake, return to deep sleep unless PWR is held low for at least 3 seconds.
 * @note Call after board_register_hardware() so the power GPIO is readable. May not return (re-enters deep sleep).
 */
void board_x4_deep_sleep_wake_gate(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_COM_API_H__ */
