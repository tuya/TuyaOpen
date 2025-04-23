/**
 * @file tdd_pixel_sm16703p.h
 * @author www.tuya.com
 * @brief tdd_pixel_sm16703p module is used to driving sm16703p chip
 * @version 0.1
 * @date 2022-03-08
 *
 * @copyright Copyright (c) tuya.inc 2022
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
 * @brief: 注册设备
 * @param[in]: *driver_name -> 设备名
 * @return: success -> OPRT_OK
 */
OPERATE_RET tdd_sm16703p_opt_driver_register(char *driver_name, PIXEL_DRIVER_CONFIG_T *init_param, PIXEL_PWM_CFG_T *pwm_cfg);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDD_PIXEL_SM16703P_OPT_H__*/
