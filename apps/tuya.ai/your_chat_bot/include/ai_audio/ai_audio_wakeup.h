/**
 * @file ai_audio_wakeup.h
 * @version 0.1
 * @date 2025-04-14
 */

#ifndef __AI_AUDIO_WAKEUP_H__
#define __AI_AUDIO_WAKEUP_H__

#include "tkl_asr.h"
#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    AI_AUDIO_WAKEUP_EVT_NONE,
    AI_AUDIO_WAKEUP_EVT_ENTER_IDLE,
    AI_AUDIO_WAKEUP_EVT_WAKEUP,
} AI_AUDIO_WAKEUP_EVENT_E;

typedef void (*AI_AUDIO_WAKEUP_ENTER_IDLE_CB)(void);
/***********************************************************a
********************function declaration********************
***********************************************************/
OPERATE_RET ai_audio_wakeup_init(uint32_t keepalive_time_ms, AI_AUDIO_WAKEUP_ENTER_IDLE_CB fun_cb);

OPERATE_RET ai_audio_wakeup_open_vad(void);

OPERATE_RET ai_audio_wakeup_open_asr(void);

OPERATE_RET ai_audio_wakeup_feed(uint8_t *data, uint32_t len);

AI_AUDIO_WAKEUP_EVENT_E ai_audio_wakeup_detect_event(void);

void ai_audio_set_wakeup_manual(bool is_wakeup);

bool ai_audio_is_awake(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_AUDIO_WAKEUP_H__ */
