/**
 * @file tdd_disp_esp_lcd.h
 * @brief Shared display configuration for the ESP-IDF esp_lcd TDD adapters.
 *
 * This header only carries the geometry / pixel-format / backlight description
 * that is common to every ESP panel, regardless of the bus it is driven over.
 * The actual register helpers live in bus-specific adapters:
 *   - SPI / QSPI / I80 / I2C panels  -> tdd_disp_esp_spi.h
 *   - MIPI-DSI DPI panels            -> tdd_disp_esp_dpi.h
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_ESP_LCD_H__
#define __TDD_DISP_ESP_LCD_H__

#include "tuya_cloud_types.h"
#include "tdl_display_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint16_t                 width;
    uint16_t                 height;
    TUYA_DISPLAY_PIXEL_FMT_E pixel_fmt;
    TUYA_DISPLAY_ROTATION_E  rotation;
    bool                     is_swap;   /* RGB565 byte-swap needed */
    TUYA_DISPLAY_BL_CTRL_T   bl;        /* GPIO/PWM backlight; NONE if handled elsewhere */
    TUYA_DISPLAY_IO_CTRL_T   power;     /* power-enable pin; leave zeroed if unused */
} TDD_DISP_ESP_LCD_CFG_T;

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ESP_LCD_H__ */
