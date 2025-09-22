/**
 * @file tdd_pixel_pwm.c
 * @brief TDD layer PWM support for LED pixel controllers
 *
 * This source file provides PWM functionality for LED pixel controllers that require
 * additional PWM channels for features like color temperature control or brightness
 * adjustment. The module handles PWM initialization, control, and output operations
 * for LED strips that combine addressable pixels with traditional PWM-controlled
 * lighting elements.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_log.h"
#include "tkl_pwm.h"

#include "tdd_pixel_pwm.h"
/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief open pwm dimmer
 *
 * @param[in] drv_handle: dimmer driver handle
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdd_pixel_pwm_open(PIXEL_PWM_CFG_T *p_drv)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t i = 0;
    TUYA_PWM_BASE_CFG_T pwm_cfg = {0};

    if (NULL == p_drv) {
        return OPRT_INVALID_PARM;
    }

    memset((uint8_t *)&pwm_cfg, 0x00, SIZEOF(pwm_cfg));

    pwm_cfg.frequency = p_drv->pwm_freq;
    
    /*��������ƽ̨��֧�ּ������ã��������ߵ���Ч���߼������� tdd �����д���*/
    pwm_cfg.polarity  = TUYA_PWM_POSITIVE;
    pwm_cfg.duty      = (FALSE == p_drv->active_level) ? PIXEL_PWM_DUTY_MAX : 0;

    for (i = 0; i < PIXEL_PWM_NUM_MAX; i++) {
        if (PIXEL_PWM_ID_INVALID == p_drv->pwm_ch_arr[i]) {
            continue;
        }
        TUYA_CALL_ERR_RETURN(tkl_pwm_init(p_drv->pwm_ch_arr[i], &pwm_cfg));
    }

    return OPRT_OK;
}
/**
 * @brief close pwm dimmer
 *
 * @param[in] drv_handle: dimmer driver handle
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdd_pixel_pwm_close(PIXEL_PWM_CFG_T *p_drv)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t i = 0;

    if(NULL == p_drv) {
        return OPRT_INVALID_PARM;
    }

    for (i = 0; i < PIXEL_PWM_NUM_MAX; i++) {
        if(PIXEL_PWM_ID_INVALID == p_drv->pwm_ch_arr[i]) {
            continue;
        }
        if(PIXEL_PWM_CH_IDX_COLD == i || PIXEL_PWM_CH_IDX_WARM == i){
            TUYA_CALL_ERR_RETURN(tkl_pwm_stop(p_drv->pwm_ch_arr[i]));
            TUYA_CALL_ERR_RETURN(tkl_pwm_deinit(p_drv->pwm_ch_arr[i]));
        }
    }

    return OPRT_OK;
}

/**
 * @brief control dimmer output
 *
 * @param[in] drv_handle: driver handle
 * @param[in] p_rgbcw: the value of the value
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdd_pixel_pwm_output(PIXEL_PWM_CFG_T *p_drv, LIGHT_RGBCW_U *p_rgbcw)
{
    uint16_t i = 0, pwm_duty = 0;

    for (i = 0; i < PIXEL_PWM_NUM_MAX; i++) {
        if(PIXEL_PWM_ID_INVALID == p_drv->pwm_ch_arr[i]) {
            continue;
        }
        if(PIXEL_PWM_CH_IDX_COLD == i || PIXEL_PWM_CH_IDX_WARM == i){

            pwm_duty = (TRUE == p_drv->active_level) ? p_rgbcw->array[i + 3] :\
                                                    (PIXEL_PWM_DUTY_MAX - p_rgbcw->array[i + 3]);
            
            tkl_pwm_duty_set(p_drv->pwm_ch_arr[i], pwm_duty);
            tkl_pwm_start(p_drv->pwm_ch_arr[i]);
            if(TRUE == p_drv->active_level && p_rgbcw->array[i + 3] == 0){
                tkl_pwm_stop(p_drv->pwm_ch_arr[i]);//��ʱ���t5ƽ̨pwm setduty0�޷�ֹͣ������� 
            }
        }

    }

    return OPRT_OK;
}