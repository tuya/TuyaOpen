/**
 * @file tdd_pixel_ws2814.h
 * @brief TDD layer driver for WS2814 RGBW LED pixel controller
 *
 * This header file provides the interface for the WS2814 LED pixel controller driver.
 * WS2814 is a 4-channel RGBW LED controller that supports individual pixel control
 * with built-in PWM generation and daisy-chain connectivity. The driver implements the
 * TDD (Tuya Device Driver) layer interface for registering and managing WS2814 LED strips.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_PIXEL_WS2814_H__
#define __TDD_PIXEL_WS2814_H__

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
 * @brief tdd_ws2814_driver_register
 *
 * @param[in] driver_name
 * @param[in] order_mode
 * @return
 */
OPERATE_RET tdd_ws2814_driver_register(char *driver_name, PIXEL_DRIVER_CONFIG_T *init_param);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDD_PIXEL_WS2814_H__*/
