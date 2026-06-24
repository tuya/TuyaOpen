/**
 * @file speaker_event.c
 * @brief MQTT/OTA/reset event handlers for smart_speaker.
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * Called from tuya_main.c on MQTT connect, OTA notify, and device reset.
 * Re-platformed off voice_app_compat onto ai_components.
 */

#include "tal_api.h"

#include "speaker_event.h"
#include "speaker_config.h"
#include "speaker_dp.h"
#include "app_smart_speaker.h"

#include "tuya_iot.h"
#include "ai_chat_main.h"
#include "ai_audio_player.h"
#include "tuya_ai_agent.h"
#include "mqc_app.h"

static void __interrupt_all(void)
{
    ai_audio_player_stop(AI_AUDIO_PLAYER_ALL);
    tuya_ai_agent_event(AI_EVENT_CHAT_BREAK, 0);
}

/**
 * @brief MQTT connected (first connect and every reconnect) — restore custom
 *        protocol handlers wiped by tuya_mqtt_init, then report initial DPs.
 *
 * The AI agent is started by ai_chat_main, which subscribes EVENT_MQTT_CONNECTED
 * on its own (ai_mode framework), so nothing else is needed here.
 */
void speaker_event_mqtt_connected(tuya_iot_client_t *client)
{
    OPERATE_RET rt;

    PR_DEBUG("%s", __func__);
    PR_NOTICE("speaker MQTT connected — reporting initial DPs");

    rt = mqc_app_reregister_all();
    if (rt != OPRT_OK) {
        PR_ERR("mqc_app_reregister_all failed %d", rt);
        return;
    }

    int vol = ai_chat_get_volume();
    if (vol >= 0) {
        speaker_dp_report_volume(client, (uint8_t)vol);
    }

    speaker_dp_report_mic(client, speaker_dp_mic_is_enabled());
    speaker_dp_report_play_state(client);
    speaker_dp_report_ctrl_group(client);
}

/**
 * @brief OTA notify — interrupt all voice activity.
 */
void speaker_event_ota_notify(void)
{
    PR_DEBUG("%s", __func__);
    PR_NOTICE("OTA notify — interrupting voice activity");
    __interrupt_all();
    speaker_dp_set_mic_enabled(FALSE);
    speaker_hw_alert_set(SPEAKER_ALERT_OTA_START);
}

/**
 * @brief Device reset — interrupt voice activity on factory reset.
 */
void speaker_event_reset(tuya_reset_type_t type)
{
    PR_DEBUG("%s", __func__);
    PR_NOTICE("Device reset type:%d", type);

    if (type == TUYA_RESET_TYPE_FACTORY) {
        PR_NOTICE("Factory reset — interrupting voice activity");
        __interrupt_all();
    }

    speaker_hw_alert_set(SPEAKER_ALERT_IDLE);
}
