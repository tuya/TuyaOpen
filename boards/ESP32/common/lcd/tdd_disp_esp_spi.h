/**
 * @file tdd_disp_esp_spi.h
 * @brief Adapter that wraps a framebuffer-less ESP-IDF esp_lcd panel (SPI / QSPI
 *        / I80 / I2C) into a TuyaOpen TDD display driver.
 *
 * These panels have no internal frame buffer: every flush must push the pixels
 * over the bus with esp_lcd_panel_draw_bitmap(). On ESP32-S3 the bus DMA also
 * cannot read PSRAM, so the VRAM frame is bounced through internal-SRAM buffers.
 *
 * For MIPI-DSI DPI panels (which own their PSRAM frame buffer and are refreshed
 * continuously by hardware) use tdd_disp_esp_dpi.h instead.
 *
 * Call tdd_disp_esp_spi_register() after the panel has been initialised (via
 * a board-specific tdd_disp_esp_* register helper).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_ESP_SPI_H__
#define __TDD_DISP_ESP_SPI_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_esp_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register an already-initialised framebuffer-less esp_lcd panel as a
 *        TuyaOpen TDD display device.
 *
 * @param name     Device name used to look up the device later (e.g. "lcd").
 * @param panel_io esp_lcd_panel_io_handle_t cast to void *.
 * @param panel    esp_lcd_panel_handle_t cast to void *.
 * @param cfg      Display geometry, pixel format, and backlight configuration.
 *
 * @return OPRT_OK on success.
 */
OPERATE_RET tdd_disp_esp_spi_register(char *name, void *panel_io, void *panel,
                                          TDD_DISP_ESP_LCD_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ESP_SPI_H__ */
