/**
 * @file tdd_pixel_sk6812.h
 * @brief TDD layer driver for SK6812 RGBW LED pixel controller
 *
 * This header file provides the interface for the SK6812 LED pixel controller driver.
 * SK6812 is a 4-channel (RGBW) LED controller that supports individual pixel control
 * with built-in PWM generation. The driver implements the TDD (Tuya Device Driver)
 * layer interface for registering and managing SK6812 LED strips.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_PIXEL_SK6812_H__
#define __TDD_PIXEL_SK6812_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tdd_pixel_type.h"
/*********************************************************************
******************************macro define****************************
*********************************************************************/

/*********************************************************************
****************************typedef define****************************
*********************************************************************/

/*********************************************************************
****************************function define***************************
*********************************************************************/
/**
 * @function: tdd_sk6812_driver_register
 * @brief: Register device
 * @param[in] driver_name
 * @param[in] order_mode
 * @return: success -> OPRT_OK
 */
OPERATE_RET tdd_sk6812_driver_register(char *driver_name, PIXEL_DRIVER_CONFIG_T *init_param);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDD_PIXEL_SK6812_H__*/
