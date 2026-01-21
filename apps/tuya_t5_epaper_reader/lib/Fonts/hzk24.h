/**
 * @file hzk24.h
 * @brief HZK24 Chinese font library interface (24x24 pixels)
 *
 * This library provides access to HZK24 font data for displaying
 * Chinese characters in GB2312 encoding.
 *
 * Font specifications:
 * - Size: 24x24 pixels
 * - Encoding: GB2312
 * - Data size: 72 bytes per character (24*24/8)
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __HZK24_H__
#define __HZK24_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get font data for a GB2312 character
 *
 * @param gb_high High byte of GB2312 code (0xA1-0xF7)
 * @param gb_low Low byte of GB2312 code (0xA1-0xFE)
 * @param buffer Buffer to store font data (must be at least 72 bytes)
 * @return 0 on success, -1 if character not found
 */
int hzk24_get_font_data(uint8_t gb_high, uint8_t gb_low, uint8_t *buffer);

/**
 * @brief Check if font data exists for a character
 *
 * @param gb_high High byte of GB2312 code
 * @param gb_low Low byte of GB2312 code
 * @return 1 if font exists, 0 otherwise
 */
int hzk24_has_font(uint8_t gb_high, uint8_t gb_low);

#ifdef __cplusplus
}
#endif

#endif /* __HZK24_H__ */
