/**
 * @file tdd_pixel_yx1903b.h
 * @brief TDD layer driver for YX1903B RGB LED pixel controller
 *
 * This header file provides the interface for the YX1903B LED pixel controller driver.
 * YX1903B is a 3-channel RGB LED controller that supports individual pixel control
 * with built-in PWM generation. The driver implements the TDD (Tuya Device Driver)
 * layer interface for registering and managing YX1903B LED strips.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_PIXEL_YX1903B_H__
#define __TDD_PIXEL_YX1903B_H__

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
 * @brief tdd_yx1903b_driver_register
 *
 * @param[in] driver_name
 * @param[in] order_mode
 * @return
 */
OPERATE_RET tdd_yx1903b_driver_register(char *driver_name, PIXEL_DRIVER_CONFIG_T *init_param);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDD_PIXEL_YX1903B_H__*/
