/**
 * @file tdd_pixel_basic.h
 * @author www.tuya.com
 * @brief tdd_pixel_basic module is used to provid chip basic driver api
 * @version 0.1
 * @date 2022-07-14
 *
 * @copyright Copyright (c) tuya.inc 2022
 *
 */

#ifndef __TDD_PIXEL_PWM_H__
#define __TDD_PIXEL_PWM_H__

#include "tdd_pixel_type.h"
#include "tdu_light_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/**
 * @brief pwm duty limit
 */
#define PIXEL_PWM_DUTY_MAX                10000  // max: 100%

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief open pwm dimmer
 *
 * @param[in] drv_handle: dimmer driver handle
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdd_pixel_pwm_open(PIXEL_PWM_CFG_T *p_drv);
/**
 * @brief close pwm dimmer
 *
 * @param[in] drv_handle: dimmer driver handle
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdd_pixel_pwm_close(PIXEL_PWM_CFG_T *p_drv);

/**
 * @brief control dimmer output
 *
 * @param[in] drv_handle: driver handle
 * @param[in] p_rgbcw: the value of the value
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdd_pixel_pwm_output(PIXEL_PWM_CFG_T *p_drv, LIGHT_RGBCW_U *p_rgbcw);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_PIXEL_PWM_H__ */