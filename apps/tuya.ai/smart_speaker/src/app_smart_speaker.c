/**
 * @file app_smart_speaker.c
 * @brief Application init for smart_speaker.
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * Drives the ai_components ai_mode framework: registers the custom
 * ai_mode_speaker (single wakeword -> multi-turn) and starts ai_chat_main,
 * which owns audio capture/playback, KWS, the button, the cloud agent and the
 * 20 ms mode task. Product features (DP, prompts, BT, CLI) hang off the
 * ai_user_event stream via __speaker_ai_event_cb.
 */

#include "tal_api.h"

#include "app_smart_speaker.h"
#include "speaker_config.h"
#include "speaker_dp.h"
#include "speaker_bt_player.h"

#include "ai_chat_main.h"
#include "ai_user_event.h"
#include "ai_mode_speaker.h"
#include "ai_audio_player.h"

#define PRINTF_FREE_HEAP_TTIME (10 * 1000)

static TIMER_ID sg_printf_heap_tm;

/**
 * @brief Map a product alert event onto ai_audio_player's built-in tone.
 *
 * IDLE / OTA_START have no dedicated product tone — dialog feedback is driven
 * by ai_mode_speaker (LED) and ai_audio_player (TTS/alerts).
 */
int speaker_hw_alert_set(SPEAKER_ALERT_E evt)
{
    PR_DEBUG("speaker_hw_alert_set: evt=%d", evt);

    switch (evt) {
    case SPEAKER_ALERT_POWER_ON:
        return ai_audio_player_alert(AI_AUDIO_ALERT_POWER_ON);
    case SPEAKER_ALERT_NETWORK_CFG:
        return ai_audio_player_alert(AI_AUDIO_ALERT_NETWORK_CFG);
    case SPEAKER_ALERT_NO_INTERNET:
        return ai_audio_player_alert(AI_AUDIO_ALERT_NETWORK_DISCONNECT);
    default:
        return OPRT_OK;
    }
}

static void __printf_free_heap_tm_cb(TIMER_ID timer_id, void *arg)
{
    (void)timer_id;
    (void)arg;
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    PR_INFO("Free heap size:%d, Free psram heap size:%d", tal_system_get_free_heap_size(),
            tal_psram_get_free_heap_size());
#else
    PR_INFO("Free heap size:%d", tal_system_get_free_heap_size());
#endif
}

/**
 * @brief Product event sink — all AI_USER_EVT_* from the ai_mode framework.
 *        Product layers (DP reporting, prompts) react here; the dialog state
 *        machine itself is handled inside ai_mode_speaker.
 */
static void __speaker_ai_event_cb(AI_NOTIFY_EVENT_T *event)
{
    if (event == NULL) {
        return;
    }
    /* TODO(migration): route product-relevant events (play state / mode change)
     * to the DP layer once speaker_dp is re-platformed onto ai_components. */
}

OPERATE_RET app_smart_speaker_init(void)
{
    PR_DEBUG("%s", __func__);
    OPERATE_RET rt = OPRT_OK;

    /* 1. Register the custom mode BEFORE ai_chat_init so AI_CHAT_MODE_SPEAKER is
     *    in the registry when ai_chat_init -> ai_mode_init(default_mode) runs. */
    TUYA_CALL_ERR_RETURN(ai_mode_speaker_register());

    /* 2. Start the ai_mode framework. ai_chat_init owns audio in/out, KWS, the
     *    button (BUTTON_NAME="ai_chat_button"), the cloud agent and the mode
     *    task; on MQTT connect it calls ai_mode_init(default_mode). */
    AI_CHAT_MODE_CFG_T ai_chat_cfg = {
        .default_mode = AI_CHAT_MODE_SPEAKER,
        .default_vol  = SPEAKER_DEFAULT_VOLUME,
        .evt_cb       = __speaker_ai_event_cb,
    };
    TUYA_CALL_ERR_RETURN(ai_chat_init(&ai_chat_cfg));

    /* 3. Load product ctrl settings (DND / talk-mode / reply) and apply them. */
    speaker_dp_init();

    /* 4. Optional Bluetooth audio (scaffold). */
    rt = speaker_bt_player_init();
    if (rt != OPRT_OK) {
        PR_WARN("speaker_bt_player_init failed %d", rt);
    }

    /* 5. Free-heap monitor. */
    tal_sw_timer_create(__printf_free_heap_tm_cb, NULL, &sg_printf_heap_tm);
    tal_sw_timer_start(sg_printf_heap_tm, PRINTF_FREE_HEAP_TTIME, TAL_TIMER_CYCLE);

    PR_NOTICE("smart_speaker: initialized on ai_mode_speaker");
    return OPRT_OK;
}
