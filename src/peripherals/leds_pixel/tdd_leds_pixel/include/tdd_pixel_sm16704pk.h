/**
 * @file tdd_pixel_sm16704pk.h
 * @brief TDD layer driver for SM16704PK RGB LED pixel controller
 *
 * This header file provides the interface for the SM16704PK LED pixel controller driver.
 * SM16704PK is a 3-channel RGB LED controller that supports individual pixel control
 * with built-in PWM generation. The driver implements the TDD (Tuya Device Driver)
 * layer interface for registering and managing SM16704PK LED strips.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_PIXEL_SM16704PK_H__
#define __TDD_PIXEL_SM16704PK_H__

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
 * @function:tdd_sm16704pk_driver_register
 * @brief: Register device
 * @param[in]: *driver_name -> Device name
 * @return: success -> OPRT_OK
 */
OPERATE_RET tdd_sm16704pk_driver_register(char *driver_name, PIXEL_DRIVER_CONFIG_T *init_param);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDD_PIXEL_SM16704PK_H__*/
