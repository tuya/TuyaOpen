/**
 * @file tdd_disp_esp_st7789_spi.h
 * @brief One-shot init + register helper for ST7789 panels behind a single-line SPI bus.
 *
 * Internally configures the SPI bus and the ST7789 panel, then registers it
 * with TuyaOpen TDL display via tdd_disp_esp_lcd_register(). The board passes
 * pinout / SPI host / orientation through LCD_ST7789_SPI_HW_CFG_T; geometry /
 * pixel format / backlight come through TDD_DISP_ESP_LCD_CFG_T.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_ESP_ST7789_SPI_H__
#define __TDD_DISP_ESP_ST7789_SPI_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_esp_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    int  spi_host;     /* SPI peripheral host id (e.g. SPI2_HOST/SPI3_HOST integer) */
    int  sclk_io;
    int  mosi_io;
    int  cs_io;
    int  dc_io;
    int  rst_io;       /* <0 if not used */
    bool invert_color;
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
} LCD_ST7789_SPI_HW_CFG_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Initialise an ST7789 SPI panel and register it as a TuyaOpen TDD display device.
 *
 * @param[in] name Device name used for later lookup.
 * @param[in] hw   Hardware configuration (SPI host / pinout / orientation).
 * @param[in] cfg  Generic display configuration (geometry / pixel fmt / bl).
 *
 * @return OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tdd_disp_esp_st7789_spi_register(char *name,
                                             LCD_ST7789_SPI_HW_CFG_T *hw,
                                             TDD_DISP_ESP_LCD_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ESP_ST7789_SPI_H__ */
