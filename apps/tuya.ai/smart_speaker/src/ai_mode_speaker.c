/**
 * @file ai_mode_speaker.c
 * @brief smart_speaker custom chat mode — single wakeword → multi-turn dialog.
 *
 * Modeled on ai_components/ai_mode/ai_mode_wakeup.c, but lives in the product app
 * so smart_speaker owns its dialog policy. Behavior:
 *   - KWS wakeword ("你好涂鸦") OR a single button click opens a dialog window.
 *   - Within the window the user can take MULTIPLE turns without re-waking:
 *     after each AI reply (PLAY_END) we return to LISTEN instead of IDLE.
 *   - Each LISTEN/THINK restarts the idle timer; once the user stays silent for
 *     AI_SPEAKER_DIALOG_WINDOW_MS the dialog ends and we return to await-wakeword.
 *
 * The state machine (IDLE/LISTEN/UPLOAD/THINK/SPEAK) is driven by the framework:
 *   - task()        applies set_state -> cur_state transitions (20ms tick)
 *   - handle_event() maps cloud AI_USER_EVT_* to state changes
 *   - vad_change()   starts/stops uplink on speech edges (only while woken)
 *   - handle_key()   single click = manual wake (push-to-talk start)
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tuya_ai_agent.h"
#include "tkl_kws.h"
#include "tkl_vad.h"

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
#include "tdl_led_manage.h"
#endif

#include "ai_user_event.h"
#include "ai_audio_input.h"
#include "ai_audio_player.h"
#include "ai_manage_mode.h"
#include "ai_mode_speaker.h"
#include "speaker_dp.h" /* product DND window state */

/***********************************************************
************************macro define************************
***********************************************************/
#define MODE_STATE_CHANGE(_old, _new)                                                                                  \
    do {                                                                                                               \
        PR_DEBUG("mode speaker state change from %s to %s", ai_get_mode_state_str(_old),                               \
                 ai_get_mode_state_str(_new));                                                                         \
        _old = _new;                                                                                                   \
    } while (0)

/* Multi-turn window: silence for this long ends the dialog → back to wakeword.
 * Each LISTEN/THINK restarts this timer, so the window is per-turn, not absolute. */
#ifndef AI_SPEAKER_DIALOG_WINDOW_MS
#define AI_SPEAKER_DIALOG_WINDOW_MS (30 * 1000)
#endif

/***********************************************************
***********************variable define**********************
***********************************************************/
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
static TDL_LED_HANDLE_T sg_led_hdl = NULL;
#endif

static AI_MODE_STATE_E sg_mode_set_state = AI_MODE_STATE_INIT;
static AI_MODE_STATE_E sg_mode_cur_state = AI_MODE_STATE_INVALID;
static bool            sg_is_wakeup      = false;
static TIMER_ID        sg_idle_timer     = NULL;
static AI_SPEAKER_TALK_MODE_E sg_talk_mode = AI_SPEAKER_TALK_MULTI;
/* TRUE once a real user turn is being answered by the cloud (ASR_OK seen), so a
 * PLAY_END can be told apart from a local prompt/alert ending (e.g. the wakeup
 * chime), which must NOT end the dialog. */
static bool            sg_reply_active   = false;

/***********************************************************
***********************function define**********************
***********************************************************/
/* Open a dialog window: stop any playback, break the cloud turn, play the
 * wakeup chime and enter LISTEN. Shared by KWS wakeword and button click. */
static void __speaker_open_dialog(void)
{
    ai_audio_player_stop(AI_AUDIO_PLAYER_ALL);
    ai_audio_input_reset();
    tuya_ai_agent_event(AI_EVENT_CHAT_BREAK, 0);

    /* Suppress the wakeup chime during the Do-Not-Disturb window. */
    if (!speaker_dp_in_dnd_now()) {
        ai_audio_player_alert(AI_AUDIO_ALERT_WAKEUP);
    }

    MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_LISTEN);
    sg_is_wakeup    = true;
    sg_reply_active = false;
}

static void __speaker_kws_wakeup(TKL_KWS_WAKEUP_WORD_E wakeup_word)
{
    (void)wakeup_word;
    __speaker_open_dialog();
}

static void __speaker_enter_idle(void)
{
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    if (sg_led_hdl != NULL) {
        tdl_led_set_status(sg_led_hdl, TDL_LED_OFF);
    }
#endif
    tal_sw_timer_stop(sg_idle_timer);
    /* Dialog window closed: disable uplink VAD, wait for next wakeword. */
    ai_audio_input_wakeup_set(false);
    sg_is_wakeup    = false;
    sg_reply_active = false;
}

static void __speaker_enter_listen(void)
{
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    if (sg_led_hdl != NULL) {
        tdl_led_flash(sg_led_hdl, 500);
    }
#endif
    /* (Re)start the per-turn silence timer — keeps the window open for follow-up. */
    tal_sw_timer_start(sg_idle_timer, AI_SPEAKER_DIALOG_WINDOW_MS, TAL_TIMER_ONCE);
    /* Most-sensitive VAD threshold so speech onset is detected fast and the head
     * of the utterance isn't clipped (matches ai_mode_free). Without this the
     * default/leftover threshold can be too high -> VAD locks late -> dropped head. */
    tkl_vad_set_threshold(TKL_AUDIO_VAD_LOW);
    sg_is_wakeup = true;
    ai_audio_input_wakeup_set(true);
}

static void __speaker_enter_upload(void)
{
    PR_DEBUG("[ai_speaker] upload");
}

static void __speaker_enter_think(void)
{
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    if (sg_led_hdl != NULL) {
        tdl_led_flash(sg_led_hdl, 2000);
    }
#endif
    tal_sw_timer_start(sg_idle_timer, AI_SPEAKER_DIALOG_WINDOW_MS, TAL_TIMER_ONCE);
    /* Cloud is processing — pause uplink until the reply is done. */
    ai_audio_input_wakeup_set(false);
    sg_is_wakeup = false;
}

static void __speaker_enter_speak(void)
{
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    if (sg_led_hdl != NULL) {
        tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
    }
#endif
    tal_sw_timer_stop(sg_idle_timer);
}

static void __speaker_idle_timer_cb(TIMER_ID timer_id, void *arg)
{
    (void)arg;
    if (ai_audio_player_is_playing()) {
        /* Still playing a reply — extend the window rather than cutting off. */
        PR_NOTICE("[ai_speaker] player busy, idle timer reset");
        tal_sw_timer_start(timer_id, AI_SPEAKER_DIALOG_WINDOW_MS, TAL_TIMER_ONCE);
        return;
    }
    /* Silence timeout → dialog over → await wakeword. */
    MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_IDLE);
}

static OPERATE_RET __speaker_mode_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    sg_led_hdl = tdl_led_find_dev(LED_NAME);
    if (sg_led_hdl != NULL) {
        if (tdl_led_open(sg_led_hdl) != OPRT_OK) {
            PR_WARN("[ai_speaker] LED open failed, continuing without LED");
            sg_led_hdl = NULL;
        }
    } else {
        PR_WARN("[ai_speaker] LED \"%s\" not registered, continuing without LED", LED_NAME);
    }
#endif

    /* Auto VAD: hardware VAD detects speech edges within the wakeword window. */
    ai_audio_input_wakeup_mode_set(AI_AUDIO_VAD_AUTO);

    tkl_kws_reg_wakeup_cb(__speaker_kws_wakeup);
    tkl_kws_enable();

    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__speaker_idle_timer_cb, NULL, &sg_idle_timer));

    MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_IDLE);
    sg_is_wakeup = false;

    return rt;
}

static OPERATE_RET __speaker_mode_deinit(void)
{
    tkl_kws_disable();
    tuya_ai_input_stop();
    if (sg_idle_timer) {
        tal_sw_timer_delete(sg_idle_timer);
        sg_idle_timer = NULL;
    }
    sg_mode_cur_state = AI_MODE_STATE_INVALID;
    return OPRT_OK;
}

static OPERATE_RET __speaker_mode_task(void *args)
{
    (void)args;
    if (sg_mode_cur_state == sg_mode_set_state) {
        return OPRT_OK;
    }

    switch (sg_mode_set_state) {
    case AI_MODE_STATE_IDLE:
        __speaker_enter_idle();
        break;
    case AI_MODE_STATE_LISTEN:
        __speaker_enter_listen();
        break;
    case AI_MODE_STATE_UPLOAD:
        __speaker_enter_upload();
        break;
    case AI_MODE_STATE_THINK:
        __speaker_enter_think();
        break;
    case AI_MODE_STATE_SPEAK:
        __speaker_enter_speak();
        break;
    default:
        break;
    }

    sg_mode_cur_state = sg_mode_set_state;
    ai_user_event_notify(AI_USER_EVT_MODE_STATE_UPDATE, (void *)sg_mode_cur_state);
    return OPRT_OK;
}

static OPERATE_RET __speaker_mode_handle_event(AI_NOTIFY_EVENT_T *event)
{
    TUYA_CHECK_NULL_RETURN(event, OPRT_INVALID_PARM);

    if (event->type != AI_USER_EVT_MIC_DATA && event->type != AI_USER_EVT_TTS_DATA) {
        PR_DEBUG("[ai_speaker] event type: %d", event->type);
    }

    switch (event->type) {
    case AI_USER_EVT_ASR_EMPTY:
    case AI_USER_EVT_ASR_ERROR:
        /* Nothing recognized — keep listening within the window. */
        MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_LISTEN);
        break;
    case AI_USER_EVT_ASR_OK:
        /* User spoke and cloud is now producing a reply. */
        sg_reply_active = true;
        MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_THINK);
        break;
    case AI_USER_EVT_TTS_PRE:
        MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_SPEAK);
        break;
    case AI_USER_EVT_EXIT:
        MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_IDLE);
        break;
    case AI_USER_EVT_PLAY_CTL_END:
    case AI_USER_EVT_PLAY_END:
        if (!sg_reply_active) {
            /* A LOCAL prompt/alert finished. Only resume LISTEN if we are inside
             * a wakeword-opened dialog (the "你好我在" chime case) — sg_is_wakeup
             * is true then. For prompts played OUTSIDE a dialog (boot/网络/配网
             * 欢迎语 etc.) sg_is_wakeup is false, so go IDLE — otherwise the
             * device would start listening without any wakeword. */
            MODE_STATE_CHANGE(sg_mode_set_state, sg_is_wakeup ? AI_MODE_STATE_LISTEN : AI_MODE_STATE_IDLE);
        } else {
            /* A cloud reply finished. SINGLE -> end the dialog (await wakeword);
             * MULTI/FREE -> keep the window open for the next turn. */
            sg_reply_active = false;
            MODE_STATE_CHANGE(sg_mode_set_state,
                              (sg_talk_mode == AI_SPEAKER_TALK_MULTI) ? AI_MODE_STATE_LISTEN : AI_MODE_STATE_IDLE);
        }
        break;
    default:
        break;
    }

    return OPRT_OK;
}

static AI_MODE_STATE_E __speaker_mode_get_state(void)
{
    return sg_mode_set_state;
}

static OPERATE_RET __speaker_mode_client_run(void *data)
{
    (void)data;
    PR_NOTICE("[ai_speaker] connected to server");
    MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_IDLE);
    return OPRT_OK;
}

static OPERATE_RET __speaker_mode_vad_change(AI_AUDIO_VAD_STATE_E vad_flag)
{
    /* Only react to VAD while the dialog window is open. Outside it the mic VAD
     * still runs (for KWS), but speech must not start an upload. */
    if (false == sg_is_wakeup) {
        return OPRT_OK;
    }

    PR_DEBUG("[ai_speaker] vad: [%d]", vad_flag);

    if (AI_AUDIO_VAD_START == vad_flag) {
        tuya_ai_agent_set_scode(AI_AGENT_SCODE_DEFAULT);
        tuya_ai_input_start(false);
    } else {
        tuya_ai_input_stop();
    }

    return OPRT_OK;
}

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
static OPERATE_RET __speaker_mode_handle_key(TDL_BUTTON_TOUCH_EVENT_E event, void *arg)
{
    (void)arg;
    switch (event) {
    case TDL_BUTTON_PRESS_SINGLE_CLICK:
        /* Manual wake (push-to-talk start) — same as a wakeword. */
        __speaker_open_dialog();
        break;
    default:
        break;
    }
    return OPRT_OK;
}
#endif

void ai_mode_speaker_set_talk_mode(AI_SPEAKER_TALK_MODE_E mode)
{
    sg_talk_mode = mode;
    PR_NOTICE("[ai_speaker] talk mode -> %s", (mode == AI_SPEAKER_TALK_MULTI) ? "multi/free" : "single");
}

OPERATE_RET ai_mode_speaker_register(void)
{
    OPERATE_RET      rt = OPRT_OK;
    AI_MODE_HANDLE_T handle;

    memset(&handle, 0, sizeof(AI_MODE_HANDLE_T));

    handle.name         = "soundbox";
    handle.init         = __speaker_mode_init;
    handle.deinit       = __speaker_mode_deinit;
    handle.task         = __speaker_mode_task;
    handle.handle_event = __speaker_mode_handle_event;
    handle.get_state    = __speaker_mode_get_state;
    handle.client_run   = __speaker_mode_client_run;
    handle.vad_change   = __speaker_mode_vad_change;
#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
    handle.handle_key   = __speaker_mode_handle_key;
#endif

    TUYA_CALL_ERR_RETURN(ai_mode_register(AI_CHAT_MODE_SPEAKER, &handle));

    return rt;
}
