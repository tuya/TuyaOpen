/**
 * @file xteink_x4_pro_epd.h
 * @brief Xteink X4 Pro 800x480 E-Ink display driver (tkl_spi/tkl_gpio only).
 *        Auto-detects the panel controller at init: newer batches carry a
 *        UC8179/UC8279 instead of the SSD1677 (bus probe on the EPD pins).
 * @version 0.2
 * @date 2026-08-19
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __XTEINK_X4_PRO_EPD_H__
#define __XTEINK_X4_PRO_EPD_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Probe the panel controller and initialize it with the matching
 *        production sequence (UC8179/UC8279 or SSD1677).
 * @return OPRT_OK on success.
 */
OPERATE_RET xteink_x4_pro_epd_init(void);

/**
 * @brief Clear EPD to white and run a full update.
 * @return OPRT_OK on success.
 */
OPERATE_RET xteink_x4_pro_epd_clear(void);

/**
 * @brief Push mono frame (800x480, 1 bit per pixel, row-major, MSB first per byte).
 *
 * Fast differential refresh when a previous frame exists, otherwise a full
 * absolute refresh.
 *
 * @param[in] image non-NULL buffer of size (X4PRO_EPD_WIDTH / 8) * X4PRO_EPD_HEIGHT.
 * @return OPRT_OK on success.
 */
OPERATE_RET xteink_x4_pro_epd_display(uint8_t *image);

/**
 * @brief Push framebuffer with a full e-ink refresh (use before deep sleep to avoid ghosting).
 * @param[in] image 1bpp framebuffer (same as xteink_x4_pro_epd_display).
 * @return OPRT_OK on success.
 */
OPERATE_RET xteink_x4_pro_epd_display_full_refresh(uint8_t *image);

/**
 * @brief Power down the panel controller and enter deep sleep.
 * @return OPRT_OK on success.
 */
OPERATE_RET xteink_x4_pro_epd_sleep(void);

#ifdef __cplusplus
}
#endif

#endif /* __XTEINK_X4_PRO_EPD_H__ */
