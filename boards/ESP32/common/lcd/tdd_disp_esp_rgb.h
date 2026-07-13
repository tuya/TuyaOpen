/**
 * @file tdd_disp_esp_rgb.h
 * @brief ESP32-S3 RGB parallel panel TDD display adapter.
 *
 * For panels connected via the ESP32-S3 LCD_CAM peripheral in RGB mode.
 * The panel owns a PSRAM frame buffer and is refreshed continuously by hardware.
 * No driver IC initialization sequence is needed (pure DE-mode panel).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_ESP_RGB_H__
#define __TDD_DISP_ESP_RGB_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_esp_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Pixel clock */
    int pclk_hz;

    /* Timing (in pixel clocks / lines) */
    uint16_t h_res;
    uint16_t v_res;
    uint16_t hsync_back_porch;
    uint16_t hsync_front_porch;
    uint16_t hsync_pulse_width;
    uint16_t vsync_back_porch;
    uint16_t vsync_front_porch;
    uint16_t vsync_pulse_width;

    /* Control pins (-1 if not connected) */
    int pclk_gpio;
    int de_gpio;
    int hsync_gpio;
    int vsync_gpio;

    /* Data pins (16 for RGB565) */
    int data_gpio[16];

    /* Bounce buffer size (0 = use default).
     * ESP32-S3 RGB panel needs bounce buffers in internal SRAM for PSRAM access. */
    uint32_t bounce_buffer_size;
} TDD_DISP_ESP_RGB_HW_CFG_T;

/**
 * @brief Create an ESP32-S3 RGB panel and register it as a TuyaOpen TDD display device.
 *
 * @param name     Device name (e.g. "lcd").
 * @param hw       Hardware pin and timing configuration.
 * @param cfg      Display geometry, pixel format, and backlight config.
 *
 * @return OPRT_OK on success.
 */
OPERATE_RET tdd_disp_esp_rgb_register(char *name, TDD_DISP_ESP_RGB_HW_CFG_T *hw,
                                      TDD_DISP_ESP_LCD_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ESP_RGB_H__ */
