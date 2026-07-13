/**
 * @file tdd_tp_esp_ft5x06.h
 * @brief One-shot init + register helper for FT5x06 capacitive touch panels via ESP-IDF esp_lcd_touch.
 *
 * Internally configures the I2C bus, creates the esp_lcd_touch_handle_t with
 * esp_lcd_touch_new_i2c_ft5x06(), and registers the resulting handle with the
 * TuyaOpen TDL tp framework via tdd_tp_esp_register(). Callers can then look the
 * device up by name with tdl_tp_find_dev() and use the standard tdl_tp_dev_*
 * APIs.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_TP_ESP_FT5X06_H__
#define __TDD_TP_ESP_FT5X06_H__

#include "tuya_cloud_types.h"
#include "tdd_tp_esp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    int              i2c_port;   /* I2C peripheral number */
    int              i2c_scl_io; /* SCL gpio number */
    int              i2c_sda_io; /* SDA gpio number */
    int              rst_io;     /* reset gpio number; <0 if unused */
    int              int_io;     /* interrupt gpio number; <0 if unused */
    TDD_TP_ESP_CFG_T tp;         /* logical resolution and mirror flags */
} TDD_TP_ESP_FT5X06_CFG_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Initialise an FT5x06 touch controller and register it as a TuyaOpen TDD tp device.
 *
 * @param[in] name Device name used for later lookup (e.g. "tp").
 * @param[in] cfg  I2C / GPIO / TP configuration. Must be non-NULL.
 *
 * @return OPRT_OK on success, OPRT_INVALID_PARM on bad arguments,
 *         OPRT_COM_ERROR if the underlying ESP-IDF init fails.
 */
OPERATE_RET tdd_tp_esp_i2c_ft5x06_register(char *name, TDD_TP_ESP_FT5X06_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_TP_ESP_FT5X06_H__ */
