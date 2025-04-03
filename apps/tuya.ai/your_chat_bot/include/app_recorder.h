/**
 * @file app_recorder.h
 * @brief app_recorder module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#ifndef __APP_RECORDER_H__
#define __APP_RECORDER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
// PCM frame size: 320 bytes
#define RECORDER_FRAME_TM_MS (10)
#define RECORDER_FRAME_SIZE  (320)

typedef enum {
    VOICE_STATE_IN_IDLE = 0,
    VOICE_STATE_IN_SILENCE,
    VOICE_STATE_IN_START,
    VOICE_STATE_IN_VOICE,
    VOICE_STATE_IN_STOP,
    VOICE_STATE_IN_WAIT_ASR,
    VOICE_STATE_IN_RESUME,
} TUYA_AUDIO_VOICE_STATE;

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET app_recorder_init(void);

OPERATE_RET app_recorder_rb_write(const void *data, uint32_t len);

OPERATE_RET app_recorder_rb_reset(void);

OPERATE_RET app_recorder_stat_post(TUYA_AUDIO_VOICE_STATE stat);

TUYA_AUDIO_VOICE_STATE app_recorder_stat_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_RECORDER_H__ */
