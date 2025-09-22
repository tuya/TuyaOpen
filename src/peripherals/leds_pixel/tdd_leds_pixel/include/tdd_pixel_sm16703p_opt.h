/**
 * @file tdd_pixel_sm16703p_opt.h
 * @brief TDD layer optimized driver for SM16703P RGB LED pixel controller
 *
 * This header file provides the optimized interface for the SM16703P LED pixel controller driver.
 * This optimized version provides enhanced performance for SM16703P RGB LED controllers
 * with additional PWM configuration options. The driver implements the TDD (Tuya Device Driver)
 * layer interface for registering and managing SM16703P LED strips with optimized timing.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_PIXEL_SM16703P_OPT_H__
#define __TDD_PIXEL_SM16703P_OPT_H__

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
 * @function:tdd_sm16703p_driver_register
 * @brief: Register device
 * @param[in]: *driver_name -> Device name
 * @return: success -> OPRT_OK
 */
OPERATE_RET tdd_sm16703p_opt_driver_register(char *driver_name, PIXEL_DRIVER_CONFIG_T *init_param,
                                             PIXEL_PWM_CFG_T *pwm_cfg);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDD_PIXEL_SM16703P_OPT_H__*/
