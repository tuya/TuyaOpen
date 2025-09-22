/**
 * @file tdd_pixel_ws2812.h
 * @brief TDD layer driver for WS2812 RGB LED pixel controller
 *
 * This header file provides the interface for the WS2812 LED pixel controller driver.
 * WS2812 is a popular 3-channel RGB LED controller that supports individual pixel control
 * with built-in PWM generation and daisy-chain connectivity. The driver implements the
 * TDD (Tuya Device Driver) layer interface for registering and managing WS2812 LED strips.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_PIXEL_WS2812_H__
#define __TDD_PIXEL_WS2812_H__

#include "tdd_pixel_type.h"
#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
******************************macro define****************************
*********************************************************************/

/*********************************************************************
****************************typedef define****************************
*********************************************************************/

/*********************************************************************
****************************variable define***************************
*********************************************************************/

/*********************************************************************
****************************function define***************************
*********************************************************************/
/**
 * @brief  tdd_ws2812_driver_register
 *
 * @param[in] driver_name
 * @param[in] order_mode
 * @return
 */
OPERATE_RET tdd_ws2812_driver_register(char *driver_name, PIXEL_DRIVER_CONFIG_T *init_param);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDD_PIXEL_WS2812_H__*/
