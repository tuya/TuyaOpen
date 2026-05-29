/**
 * @file tdd_disp_esp_ssd1306.h
 * @brief One-shot init + register helper for SSD1306 monochrome OLED panels via I2C.
 *
 * Internally configures the I2C bus and the SSD1306 panel, then registers it
 * with TuyaOpen TDL display via tdd_disp_esp_lcd_register(). The board passes
 * I2C pinout / address through OLED_SSD1306_HW_CFG_T; geometry / pixel format
 * come through TDD_DISP_ESP_LCD_CFG_T.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_ESP_SSD1306_H__
#define __TDD_DISP_ESP_SSD1306_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_esp_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    int      i2c_port;
    int      sda_io;
    int      scl_io;
    uint16_t dev_addr;     /* SSD1306 7-bit I2C address (commonly 0x3C) */
    uint16_t panel_height; /* physical OLED panel height in rows (32 / 64) */
} OLED_SSD1306_HW_CFG_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Initialise an SSD1306 OLED and register it as a TuyaOpen TDD display device.
 *
 * @param[in] name Device name used for later lookup.
 * @param[in] hw   Hardware configuration (I2C / address / panel height).
 * @param[in] cfg  Generic display configuration.
 *
 * @return OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tdd_disp_esp_ssd1306_register(char *name,
                                          OLED_SSD1306_HW_CFG_T *hw,
                                          TDD_DISP_ESP_LCD_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ESP_SSD1306_H__ */
