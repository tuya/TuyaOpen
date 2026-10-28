/**
 * @file tdd_led_esp_ws1280_spi.h
 * @brief ESP32 WS1280 LED driver interface (SPI backend, for chips without RMT).
 *
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_LED_WS1280_SPI_H__
#define __TDD_LED_WS1280_SPI_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define TDD_LED_WS1280_SPI_COUNT_MAX 8

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint16_t led_count;
    uint32_t gpio;
    uint32_t color; // 24-bit color rgb/grb/.. format, depends on the LED strip
} TDD_LED_WS1280_SPI_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tdd_led_esp_ws1280_spi_register(char *dev_name, TDD_LED_WS1280_SPI_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_LED_WS1280_SPI_H__ */
