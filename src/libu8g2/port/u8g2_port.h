/**
 * @file u8g2_port.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __U8G2_PORT_H__
#define __U8G2_PORT_H__

#include "tuya_cloud_types.h"
#include "u8g2.h"

#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
#include "u8g2_port_setup.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
uint8_t u8x8_gpio_and_delay_tkl(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#if defined(ENABLE_SPI) && (ENABLE_SPI == 1)
uint8_t u8x8_byte_tkl_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
#endif

#if defined(ENABLE_I2C) && (ENABLE_I2C == 1)
uint8_t u8x8_byte_tkl_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
#endif

OPERATE_RET u8g2_alloc_buffer(u8g2_t *u8g2);

OPERATE_RET u8g2_free_buffer(u8g2_t *u8g2);

#ifdef __cplusplus
}
#endif

#endif /* __U8G2_PORT_H__ */
