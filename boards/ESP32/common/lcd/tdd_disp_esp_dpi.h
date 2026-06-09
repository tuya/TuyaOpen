/**
 * @file tdd_disp_esp_dpi.h
 * @brief Adapter that wraps an ESP-IDF MIPI-DSI DPI panel into a TuyaOpen TDD
 *        display driver.
 *
 * Unlike the framebuffer-less SPI/QSPI/I80 panels handled by
 * tdd_disp_esp_spi.h, a DPI panel owns its frame buffer(s) in PSRAM and is
 * refreshed continuously by hardware. A flush is therefore a single
 * esp_lcd_panel_draw_bitmap() that copies the VRAM frame into the internal
 * frame buffer (CPU or DMA2D, decided by the panel's use_dma2d flag); no
 * internal-SRAM bounce buffering and no per-line chunking are needed.
 *
 * The copy can be asynchronous (DMA2D), so this adapter waits for the panel's
 * on_color_trans_done event before recycling the VRAM frame.
 *
 * Call tdd_disp_esp_dpi_register() after the DPI panel has been initialised
 * (via a board-specific tdd_disp_esp_* register helper, e.g. ST7701).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_ESP_DPI_H__
#define __TDD_DISP_ESP_DPI_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_esp_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register an already-initialised MIPI-DSI DPI panel as a TuyaOpen TDD
 *        display device.
 *
 * @param name     Device name used to look up the device later (e.g. "lcd").
 * @param panel_io esp_lcd_panel_io_handle_t cast to void *.
 * @param panel    esp_lcd_panel_handle_t (the DPI panel) cast to void *.
 * @param cfg      Display geometry, pixel format, and backlight configuration.
 *
 * @return OPRT_OK on success.
 */
OPERATE_RET tdd_disp_esp_dpi_register(char *name, void *panel_io, void *panel,
                                      TDD_DISP_ESP_LCD_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ESP_DPI_H__ */
