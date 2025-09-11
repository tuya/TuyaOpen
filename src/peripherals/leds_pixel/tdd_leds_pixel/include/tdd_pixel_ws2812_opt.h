/**
 * @file tdd_pixel_ws2812_opt.h
 * @brief TDD layer optimized driver for WS2812 RGB LED pixel controller
 *
 * This header file provides the optimized interface for the WS2812 LED pixel controller driver.
 * This optimized version provides enhanced performance for WS2812 RGB LED controllers
 * with additional PWM configuration options and 4-bit encoding optimizations. The driver
 * implements the TDD (Tuya Device Driver) layer interface for registering and managing
 * WS2812 LED strips with optimized timing and performance.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_PIXEL_WS2812_4BIT_H__
#define __TDD_PIXEL_WS2812_4BIT_H__

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
 * @brief tdd_ws2812_opt_driver_register
 *
 * @param[in] driver_name
 * @param[in] order_mode
 * @return
 */
OPERATE_RET tdd_ws2812_opt_driver_register(char *driver_name, PIXEL_DRIVER_CONFIG_T *init_param,
                                           PIXEL_PWM_CFG_T *pwm_cfg);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDD_PIXEL_WS2812_H__*/
