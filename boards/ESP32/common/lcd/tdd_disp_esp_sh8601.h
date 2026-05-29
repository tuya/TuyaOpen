/**
 * @file tdd_disp_esp_sh8601.h
 * @brief One-shot init + register helper for SH8601 QSPI LCD panels via ESP-IDF esp_lcd.
 *
 * Internally configures the QSPI bus, creates the esp_lcd panel handle, and
 * registers it with TuyaOpen TDL display via tdd_disp_esp_lcd_register(). The
 * board passes pinout / SPI host / mirror flags through LCD_SH8601_HW_CFG_T;
 * generic display geometry / pixel format / backlight come through the shared
 * TDD_DISP_ESP_LCD_CFG_T.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_ESP_SH8601_H__
#define __TDD_DISP_ESP_SH8601_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_esp_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    int  spi_host;  /* SPI peripheral host id (e.g. SPI2_HOST/SPI3_HOST integer) */
    int  sclk_io;   /* SPI clock gpio */
    int  data0_io;  /* QSPI data line 0 (MOSI) */
    int  data1_io;  /* QSPI data line 1 (MISO) */
    int  data2_io;  /* QSPI data line 2 */
    int  data3_io;  /* QSPI data line 3 */
    int  cs_io;     /* SPI chip-select gpio */
    bool mirror_x;
    bool mirror_y;
} LCD_SH8601_HW_CFG_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Initialise an SH8601 QSPI LCD and register it as a TuyaOpen TDD display device.
 *
 * @param[in] name Device name used for later lookup (e.g. "lcd").
 * @param[in] hw   Hardware configuration (SPI host / pinout / mirror).
 * @param[in] cfg  Generic display configuration (width / height / pixel fmt / bl).
 *
 * @return OPRT_OK on success, error code otherwise.
 *
 * @note The backlight is driven via SH8601 register 0x51 by an internal callback,
 *       so the board should leave cfg->bl.type as TUYA_DISP_BL_TP_NONE / its own
 *       choice; the helper unconditionally re-routes it to TUYA_DISP_BL_TP_CUSTOM.
 */
OPERATE_RET tdd_disp_esp_sh8601_register(char *name,
                                         LCD_SH8601_HW_CFG_T *hw,
                                         TDD_DISP_ESP_LCD_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ESP_SH8601_H__ */
