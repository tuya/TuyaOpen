/**
 * @file tdd_disp_esp_lcd.h
 * @brief Adapter that wraps an ESP-IDF esp_lcd_panel into a TuyaOpen TDD display driver.
 *
 * Call tdd_disp_esp_lcd_register() after the panel has been initialised (via
 * board-specific tdd_disp_esp_* register helpers), then use lv_vendor_init(name)
 * from TuyaOpen's LVGL port to bring up LVGL.
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

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Register an already-initialised ESP-IDF LCD panel as a TuyaOpen TDD display device.
 *
 * @param name     Device name used to look up the device later (e.g. "lcd").
 * @param panel_io esp_lcd_panel_io_handle_t cast to void *.
 * @param panel    esp_lcd_panel_handle_t cast to void *.
 * @param cfg      Display geometry, pixel format, and backlight configuration.
 *
 * @return OPRT_OK on success.
 */
OPERATE_RET tdd_disp_esp_lcd_register(char *name, void *panel_io, void *panel,
                                       TDD_DISP_ESP_LCD_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ESP_LCD_H__ */
