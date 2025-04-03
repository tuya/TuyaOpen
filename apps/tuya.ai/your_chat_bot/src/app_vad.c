/**
 * @file app_vad.c
 * @brief app_vad module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#include "app_vad.h"

#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t is_init;
    uint8_t is_start;
    MUTEX_HANDLE mutex;
} APP_VAD_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_VAD_T sg_app_vad = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET app_vad_frame_put(uint8_t *pbuf, uint16_t len)
{
    return ty_vad_frame_put(pbuf, len);
}

ty_vad_flag_e app_vad_get_flag(void)
{
    return ty_get_vad_flag();
}

OPERATE_RET app_vad_start(void)
{
    if (sg_app_vad.is_start) {
        return OPRT_OK;
    }

    if (sg_app_vad.mutex == NULL || sg_app_vad.is_init == 0) {
        PR_ERR("vad mutex is NULL or not init, is_init=%d", sg_app_vad.is_init);
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_app_vad.mutex);
    ty_vad_app_start();
    sg_app_vad.is_start = 1;
    tal_mutex_unlock(sg_app_vad.mutex);
    PR_DEBUG("app vad start success");

    return OPRT_OK;
}

OPERATE_RET app_vad_stop(void)
{
    if (!sg_app_vad.is_start) {
        return OPRT_OK;
    }

    if (sg_app_vad.mutex == NULL || sg_app_vad.is_init == 0) {
        PR_ERR("vad mutex is NULL or not init, is_init=%d", sg_app_vad.is_init);
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_app_vad.mutex);
    ty_vad_app_stop();
    sg_app_vad.is_start = 0;
    tal_mutex_unlock(sg_app_vad.mutex);
    PR_DEBUG("app vad stop success");

    return OPRT_OK;
}

OPERATE_RET app_vad_init(uint16_t sample_rate, uint16_t channel)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_app_vad.mutex));

    tal_mutex_lock(sg_app_vad.mutex);

    ty_vad_config_t vad_config;
    memset(&vad_config, 0, sizeof(ty_vad_config_t));
    vad_config.start_threshold_ms = 300;
    vad_config.end_threshold_ms = 500;
    vad_config.silence_threshold_ms = 0;
    vad_config.sample_rate = sample_rate;
    vad_config.channel = channel;
    vad_config.vad_frame_duration = 10;
    vad_config.scale = 2.5;

    TUYA_CALL_ERR_RETURN(ty_vad_app_init(&vad_config));

    TUYA_CALL_ERR_RETURN(ty_vad_app_start());

    sg_app_vad.is_init = 1;
    sg_app_vad.is_start = 1;

    tal_mutex_unlock(sg_app_vad.mutex);

    PR_DEBUG("app vad init success");

    return rt;
}
