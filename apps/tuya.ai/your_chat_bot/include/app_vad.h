/**
 * @file app_vad.h
 * @brief app_vad module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#ifndef __APP_VAD_H__
#define __APP_VAD_H__

#include "tuya_cloud_types.h"

#include "ty_vad_app.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET app_vad_init(uint16_t sample_rate, uint16_t channel);

OPERATE_RET app_vad_frame_put(uint8_t *pbuf, uint16_t len);

ty_vad_flag_e app_vad_get_flag(void);

OPERATE_RET app_vad_start(void);

OPERATE_RET app_vad_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_VAD_H__ */
