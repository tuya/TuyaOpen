/**
 * @file tdd_camera_esp_dvp.h
 * @brief ESP32/ESP32-S3 DVP camera TDD driver using the esp32-camera IDF component.
 *
 * Bridges the espressif/esp32-camera component to the TDL camera layer.
 * Supports OV2640 (and other sensors supported by esp32-camera) connected via
 * the LCD_CAM DVP parallel interface on ESP32/ESP32-S3.
 *
 * PWDN / RESET are not driven by this driver; control them externally (e.g. via
 * XL9555 IO expander) before calling tdd_camera_esp_dvp_register().
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_CAMERA_ESP_DVP_H__
#define __TDD_CAMERA_ESP_DVP_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/**
 * @brief Hardware pin configuration for an ESP32/ESP32-S3 DVP camera.
 *
 * All GPIO numbers follow the ESP-IDF convention (GPIO_NUM_NC = -1 means "not used").
 */
typedef struct {
    /* Sensor control — use -1 when driven externally (e.g. XL9555 IO expander). */
    int pin_pwdn;  /*!< PWDN GPIO, -1 = external / not used */
    int pin_reset; /*!< RESET GPIO, -1 = external / not used */

    /* XCLK — use -1 when the sensor module has its own oscillator. */
    int pin_xclk;     /*!< XCLK output GPIO, -1 = not needed */
    int xclk_freq_hz; /*!< XCLK frequency in Hz (e.g. 20000000); ignored if pin_xclk < 0 */

    /* SCCB (sensor I2C) */
    int pin_sccb_scl;  /*!< SCCB SCL GPIO */
    int pin_sccb_sda;  /*!< SCCB SDA GPIO */
    int sccb_i2c_port; /*!< Existing I2C port to reuse (-1 = allocate a new one) */

    /* DVP data bus (8-bit) */
    int pin_d0; /*!< D0 GPIO (LSB) */
    int pin_d1;
    int pin_d2;
    int pin_d3;
    int pin_d4;
    int pin_d5;
    int pin_d6;
    int pin_d7; /*!< D7 GPIO (MSB) */

    /* DVP sync signals (sensor → host) */
    int pin_vsync; /*!< VSYNC GPIO */
    int pin_href;  /*!< HREF GPIO */
    int pin_pclk;  /*!< PCLK GPIO */
} TDD_CAMERA_ESP_DVP_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Register a DVP camera device with the TDL camera layer.
 *
 * Call this after asserting sensor power / releasing reset externally.
 * The camera is not yet started; it is opened on the first
 * tdl_camera_open() call.
 *
 * @param[in] name Unique camera device name (matches CAMERA_NAME).
 * @param[in] cfg  Hardware pin configuration, must not be NULL.
 * @return OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tdd_camera_esp_dvp_register(const char *name, const TDD_CAMERA_ESP_DVP_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_ESP_DVP_H__ */
