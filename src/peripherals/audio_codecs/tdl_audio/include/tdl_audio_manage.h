/**
 * @file tdl_audio_manage.h
 * @brief tdl_audio_manage module is used to
 * @version 0.1
 * @date 2025-04-08
 */

#ifndef __TDL_AUDIO_MANAGE_H__
#define __TDL_AUDIO_MANAGE_H__

#include "tuya_cloud_types.h"

#include "tdl_audio_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef void *TDL_AUDIO_HANDLE_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tdl_audio_find(char *name, TDL_AUDIO_HANDLE_T *handle);

OPERATE_RET tdl_audio_open(TDL_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb);

OPERATE_RET tdl_audio_play(TDL_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len);

OPERATE_RET tdl_audio_play_stop(TDL_AUDIO_HANDLE_T handle);

OPERATE_RET tdl_audio_volume_set(TDL_AUDIO_HANDLE_T handle, uint8_t volume);

OPERATE_RET tdl_audio_close(TDL_AUDIO_HANDLE_T handle);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_AUDIO_MANAGE_H__ */
