/**
 * @file speaker_event.h
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 * @brief MQTT/OTA/reset event declarations for smart_speaker.
 */

#ifndef __SPEAKER_EVENT_H__
#define __SPEAKER_EVENT_H__

#include "tuya_cloud_types.h"
#include "tuya_iot.h"

#ifdef __cplusplus
extern "C" {
#endif

void speaker_event_mqtt_connected(tuya_iot_client_t *client);
void speaker_event_ota_notify(void);
void speaker_event_reset(tuya_reset_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* __SPEAKER_EVENT_H__ */
