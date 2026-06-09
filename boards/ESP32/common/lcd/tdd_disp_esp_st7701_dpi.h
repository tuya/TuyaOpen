/**
 * @file tdd_disp_esp_st7701_dpi.h
 * @brief Hardware config for ST7701 (JD9365-compatible) MIPI DSI DPI LCD panels.
 *
 * Used by: WAVESHARE_ESP32P4C6_TOUCH_LCD_4.3 (4.3" 480x800 IPS, ST7701/JD9365)
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_ESP_ST7701_DPI_H__
#define __TDD_DISP_ESP_ST7701_DPI_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_esp_dpi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */

typedef struct {
    /* Hardware pins */
    int  rst_io;              /* Reset GPIO, <0 if not used */

    /* Panel orientation */
    bool mirror_x;
    bool mirror_y;
    bool swap_xy;

    /* MIPI DSI PHY power (ESP32-P4 specific) */
    int  phy_pwr_ldo_chan;    /* LDO channel for DSI PHY, <0 to skip */
    int  phy_pwr_ldo_mv;      /* LDO voltage in mV */
} LCD_ST7701_DPI_HW_CFG_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Initialise an ST7701 MIPI DSI DPI panel and register it as a TuyaOpen
 *        TDD display device.
 *
 * Uses the ESP-IDF esp_lcd_st7701 managed component which correctly handles
 * ST7701-specific DCS commands (reset, init, disp_on_off, mirror, sleep, etc.)
 * over the MIPI DSI DBI command interface.
 *
 * MIPI DSI bus parameters and DPI video timing are board-specific constants
 * defined in the .c implementation file.
 *
 * @param[in] name Device name used for later lookup (e.g. "lcd").
 * @param[in] hw   Hardware configuration (reset pin, LDO, orientation).
 * @param[in] cfg  Generic display configuration (geometry / pixel fmt / bl).
 *
 * @return OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tdd_disp_esp_st7701_dpi_register(char *name,
                                              LCD_ST7701_DPI_HW_CFG_T *hw,
                                              TDD_DISP_ESP_LCD_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ESP_ST7701_DPI_H__ */
