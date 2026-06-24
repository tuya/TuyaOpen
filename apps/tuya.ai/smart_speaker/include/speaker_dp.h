/**
 * @file speaker_dp.h
 * @brief DP processing interface for smart_speaker
 */

#ifndef __SPEAKER_DP_H__
#define __SPEAKER_DP_H__

#include "tuya_cloud_types.h"
#include "tuya_iot.h"

#ifdef __cplusplus
extern "C" {
#endif

int speaker_dp_process(dp_obj_recv_t *dpobj);

int speaker_dp_report_volume(tuya_iot_client_t *client, uint8_t vol);
int speaker_dp_report_mic(tuya_iot_client_t *client, BOOL_T on);

int speaker_dp_report_play_state(tuya_iot_client_t *client);
int speaker_dp_report_ctrl_group(tuya_iot_client_t *client);

int speaker_dp_report_volume_current(tuya_iot_client_t *client);
int speaker_dp_report_mic_current(tuya_iot_client_t *client);

/* Product-side mic enable state (replaces voice_app mic state). */
BOOL_T speaker_dp_mic_is_enabled(void);
void   speaker_dp_set_mic_enabled(BOOL_T on);

/* Load persisted ctrl settings (DND / talk-mode / wakeup reply) from KV and
 * apply the talk-mode to ai_mode_speaker. Call once at app init. */
void speaker_dp_init(void);

/* TRUE if currently in do-not-disturb (manual DP4 toggle, or the configured
 * time window). Suppresses the wakeup chime and DP207 ring. */
BOOL_T speaker_dp_in_dnd_now(void);

/* Set the manual do-not-disturb toggle (DP4) and report it back to the cloud. */
void speaker_dp_set_dnd(BOOL_T on);

#ifdef __cplusplus
}
#endif

#endif /* __SPEAKER_DP_H__ */