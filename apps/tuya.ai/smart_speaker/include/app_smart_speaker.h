/**
 * @file app_smart_speaker.h
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 * @brief Public API for the smart_speaker application
 */

#ifndef __APP_SMART_SPEAKER_H__
#define __APP_SMART_SPEAKER_H__

#include "tuya_cloud_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Product alert tones. Button/KWS/dialog modes/LED/idle re-arm are owned by
 * ai_chat_main + ai_mode_speaker; this maps the few product-level alerts
 * (config/OTA/no-internet) onto ai_audio_player's built-in tones. */
typedef enum {
    SPEAKER_ALERT_POWER_ON,
    SPEAKER_ALERT_NETWORK_CFG,
    SPEAKER_ALERT_NO_INTERNET,
    SPEAKER_ALERT_IDLE,
    SPEAKER_ALERT_OTA_START,
} SPEAKER_ALERT_E;

OPERATE_RET app_smart_speaker_init(void);
int         speaker_hw_alert_set(SPEAKER_ALERT_E evt);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SMART_SPEAKER_H__ */
