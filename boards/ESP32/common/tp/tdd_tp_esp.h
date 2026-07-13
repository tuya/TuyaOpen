/**
 * @file tdd_tp_esp.h
 * @brief Adapter that wraps an ESP-IDF esp_lcd_touch_handle_t into a TuyaOpen TDD tp driver.
 *
 * Call tdd_tp_esp_register() after the underlying touch controller has been
 * initialised (via esp_lcd_touch_new_i2c_xxx() / esp_lcd_touch_new_spi_xxx()),
 * then look up the device with tdl_tp_find_dev(name) and use the standard
 * tdl_tp_dev_open() / tdl_tp_dev_read() / tdl_tp_dev_close() interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_TP_ESP_H__
#define __TDD_TP_ESP_H__

#include "tuya_cloud_types.h"
#include "tdl_tp_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    TDD_TP_CONFIG_T tp_cfg; /* x_max / y_max / swap_xy / mirror_x / mirror_y */
} TDD_TP_ESP_CFG_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Register an already-initialised ESP-IDF touch panel as a TuyaOpen TDD tp device.
 *
 * @param[in] name        Device name used for later lookup (e.g. "tp").
 * @param[in] esp_tp_hdl  esp_lcd_touch_handle_t cast to void *. Must be non-NULL
 *                        and already created by esp_lcd_touch_new_xxx() before
 *                        this call.
 * @param[in] cfg         TP configuration (logical resolution and mirror flags).
 *
 * @return OPRT_OK on success, OPRT_INVALID_PARM on bad arguments,
 *         OPRT_MALLOC_FAILED if internal context allocation fails.
 *
 * @note   The adapter does not take ownership of esp_tp_hdl; the caller is
 *         responsible for its lifecycle (esp_lcd_touch_del) if needed.
 */
OPERATE_RET tdd_tp_esp_register(char *name, void *esp_tp_hdl, TDD_TP_ESP_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_TP_ESP_H__ */
