/**
 * @file xteink_x4_epd.h
 * @brief Xteink X4 SSD1677 E-Ink display driver API.
 * @version 0.1
 * @date 2026-04-30
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __XTEINK_X4_EPD_H__
#define __XTEINK_X4_EPD_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the X4 SSD1677 controller.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_epd_init(void);

/**
 * @brief Clear the X4 display to white using a full refresh.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_epd_clear(void);

/**
 * @brief Display one full 800x480 1-bit framebuffer.
 * @param[in] image 1bpp row-major framebuffer, MSB first, 1 is white and 0 is black.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_epd_display(uint8_t *image);

/**
 * @brief Same as xteink_x4_epd_display but always uses a full panel refresh (slow, clears ghosting).
 * @param[in] image 1bpp framebuffer (same layout as xteink_x4_epd_display).
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_epd_display_full_refresh(uint8_t *image);

/**
 * @brief Put the SSD1677 controller into deep sleep.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_epd_sleep(void);

#ifdef __cplusplus
}
#endif

#endif /* __XTEINK_X4_EPD_H__ */
