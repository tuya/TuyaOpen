/**
 * @file tkl_kws_vad_stub.c
 * @brief Stub implementations of KWS and VAD for Ubuntu x86 platform.
 *        Ubuntu desktop does not support on-device KWS/VAD inference.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tkl_kws.h"
#include "tkl_vad.h"

/***********************************************************
 * KWS stubs
 ***********************************************************/

OPERATE_RET tkl_kws_init(void)
{
    return OPRT_OK;
}

OPERATE_RET tkl_kws_reg_wakeup_cb(TKL_KWS_WAKEUP_CB wakeup_cb)
{
    (void)wakeup_cb;
    return OPRT_OK;
}

OPERATE_RET tkl_kws_enable(void)
{
    return OPRT_OK;
}

OPERATE_RET tkl_kws_disable(void)
{
    return OPRT_OK;
}

OPERATE_RET tkl_kws_deinit(void)
{
    return OPRT_OK;
}

/***********************************************************
 * VAD stubs
 ***********************************************************/

OPERATE_RET tkl_vad_set_threshold(TKL_AUDIO_VAD_THRESHOLD_E level)
{
    (void)level;
    return OPRT_OK;
}

OPERATE_RET tkl_vad_init(TKL_VAD_CONFIG_T *config)
{
    (void)config;
    return OPRT_OK;
}

OPERATE_RET tkl_vad_feed(uint8_t *data, uint32_t len)
{
    (void)data;
    (void)len;
    return OPRT_OK;
}

TKL_VAD_STATUS_T tkl_vad_get_status(void)
{
    return TKL_VAD_STATUS_SPEECH;
}

OPERATE_RET tkl_vad_start(void)
{
    return OPRT_OK;
}

OPERATE_RET tkl_vad_stop(void)
{
    return OPRT_OK;
}

OPERATE_RET tkl_vad_deinit(void)
{
    return OPRT_OK;
}
