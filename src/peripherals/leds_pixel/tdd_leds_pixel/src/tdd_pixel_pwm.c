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
    UINT_T i = 0;    
    TUYA_PWM_BASE_CFG_T pwm_cfg = {0};

    if(NULL == p_drv) {
        return OPRT_INVALID_PARM;
    }

    memset((UCHAR_T *)&pwm_cfg, 0x00, SIZEOF(pwm_cfg));

    pwm_cfg.frequency = p_drv->pwm_freq;
    
    pwm_cfg.polarity  = TUYA_PWM_POSITIVE;
    pwm_cfg.duty      = (FALSE == p_drv->active_level) ? PIXEL_PWM_DUTY_MAX : 0;

    for (i = 0; i < PIXEL_PWM_NUM_MAX; i++) {
        if(PIXEL_PWM_ID_INVALID == p_drv->pwm_ch_arr[i]) {
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
    UINT_T i = 0;

    if(NULL == p_drv) {
        return OPRT_INVALID_PARM;
    }

    for (i = 0; i < PIXEL_PWM_NUM_MAX; i++) {
        if(PIXEL_PWM_ID_INVALID == p_drv->pwm_ch_arr[i]) {
            continue;
        }

        if(PIXEL_PWM_CH_IDX_COLD == i && PIXEL_PWM_ID_INVALID != p_drv->pwm_ch_arr[PIXEL_PWM_CH_IDX_WARM] && TRUE == p_drv->active_level) {
            ;
        }else if(PIXEL_PWM_CH_IDX_WARM == i && PIXEL_PWM_ID_INVALID != p_drv->pwm_ch_arr[PIXEL_PWM_CH_IDX_COLD] && TRUE == p_drv->active_level){
            TUYA_CALL_ERR_RETURN(tkl_pwm_multichannel_stop(&p_drv->pwm_ch_arr[PIXEL_PWM_CH_IDX_COLD], 2));
        }else {
            TUYA_CALL_ERR_RETURN(tkl_pwm_stop(p_drv->pwm_ch_arr[i]));
        }

        TUYA_CALL_ERR_RETURN(tkl_pwm_deinit(p_drv->pwm_ch_arr[i]));
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
    USHORT_T pwm_duty = 0, i = 0;

    for (i = 0; i < PIXEL_PWM_NUM_MAX; i++) {
        if(PIXEL_PWM_ID_INVALID == p_drv->pwm_ch_arr[i]) {
            continue;
        }

        pwm_duty = (TRUE == p_drv->active_level) ? p_rgbcw->array[i + 3] :\
                                                  (PIXEL_PWM_DUTY_MAX - p_rgbcw->array[i + 3]);

        tkl_pwm_duty_set(p_drv->pwm_ch_arr[i], pwm_duty);

        if(PIXEL_PWM_CH_IDX_COLD == i && PIXEL_PWM_ID_INVALID != p_drv->pwm_ch_arr[PIXEL_PWM_CH_IDX_WARM] && TRUE == p_drv->active_level) {
            ;
        }else if(PIXEL_PWM_CH_IDX_WARM == i && PIXEL_PWM_ID_INVALID != p_drv->pwm_ch_arr[PIXEL_PWM_CH_IDX_COLD] && TRUE == p_drv->active_level) {
            tkl_pwm_multichannel_start(&p_drv->pwm_ch_arr[PIXEL_PWM_CH_IDX_COLD], 2);
        }else {
            tkl_pwm_start(p_drv->pwm_ch_arr[i]);
        }
    }

    return OPRT_OK;
}