/**
 * @file ai_mode_wakeup.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#if defined(ENABLE_COMP_AI_MODE_WAKEUP) && (ENABLE_COMP_AI_MODE_WAKEUP == 1)

#include "tal_api.h"
#include "tuya_ai_agent.h"

#include "tkl_kws.h"
#include "tkl_vad.h"

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
#include "tdl_led_manage.h"
#endif

#include "lang_config.h"
#include "ai_user_event.h"

#include "ai_audio_input.h"
#include "ai_audio_player.h"
#include "ai_manage_mode.h"
#include "ai_mode_wakeup.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define MODE_STATE_CHANGE(_old, _new) \
do { \
    PR_DEBUG("mode wakeup state change form %s to %s", ai_get_mode_state_str(_old),\
                                                     ai_get_mode_state_str(_new));\
    _old = _new; \
}while (0)

#define AI_CHAT_WAKEUP_TIME_MS     (7 * 1000)       // 7sec listen, then blink standby if nothing heard
#define AI_CHAT_FOLLOWUP_TIME_MS   (15 * 1000)      // after TTS, wait 15s for the next user turn
#define AI_CHAT_KWS_TAIL_MS        150              // skip leftover wake-word audio; no chime
#define AI_CHAT_TTS_FLUSH_MS       400              // wait out speaker tail, then capture

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
static TDL_LED_HANDLE_T sg_led_hdl = NULL;
#endif

static AI_MODE_STATE_E sg_mode_set_state = AI_MODE_STATE_INIT;
static AI_MODE_STATE_E sg_mode_cur_state = AI_MODE_STATE_INVALID;
static bool            sg_is_wakeup = false;
static bool            sg_user_speaking = false;
static TIMER_ID        sg_enter_idle_timer = NULL;
static TIMER_ID        sg_vad_grace_timer = NULL;
static uint32_t        sg_wakeup_time_ms = AI_CHAT_WAKEUP_TIME_MS;
static SYS_TIME_T      sg_vad_ignore_until = 0;
static SYS_TIME_T      sg_flush_until = 0;
static bool            sg_pending_speech = false;
static bool            sg_flush_chime = false;
static bool            sg_in_conversation = false;
static bool            sg_capture_armed = false;
static bool            sg_wait_asr = false;
static bool            sg_heard_speech = false;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __ai_mode_arm_first_listen(void)
{
    tal_sw_timer_start(sg_enter_idle_timer, sg_wakeup_time_ms, TAL_TIMER_ONCE);
}

static void __ai_mode_arm_followup_listen(void)
{
    tal_sw_timer_start(sg_enter_idle_timer, AI_CHAT_FOLLOWUP_TIME_MS, TAL_TIMER_ONCE);
}

static void __ai_mode_start_user_input(void)
{
    if (sg_user_speaking) {
        sg_pending_speech = false;
        return;
    }

    if (sg_mode_set_state != AI_MODE_STATE_LISTEN &&
        sg_mode_cur_state != AI_MODE_STATE_LISTEN) {
        return;
    }
    if (ai_audio_player_is_playing()) {
        return;
    }
    if (!tuya_ai_agent_is_ready()) {
        PR_DEBUG("[====ai_wakeup] agent not ready, hold capture");
        sg_pending_speech = true;
        return;
    }

    PR_NOTICE("[====ai_wakeup] start capture");
    sg_user_speaking = true;
    sg_pending_speech = false;
    tuya_ai_agent_set_scode(AI_AGENT_SCODE_DEFAULT);
    tuya_ai_input_start(false);
    /* Count the listen window from when the mic is actually open. */
    if (sg_in_conversation) {
        __ai_mode_arm_followup_listen();
    } else {
        __ai_mode_arm_first_listen();
    }
}

static bool __ai_mode_mic_gated(void)
{
    SYS_TIME_T now;

    if (!sg_flush_chime) {
        return false;
    }
    now = tal_system_get_millisecond();
    if (sg_flush_until && now >= sg_flush_until) {
        sg_flush_chime = false;
        return false;
    }
    return true;
}

static void __ai_mode_try_capture(void)
{
    if (!sg_is_wakeup) {
        return;
    }
    if (sg_mode_set_state != AI_MODE_STATE_LISTEN &&
        sg_mode_cur_state != AI_MODE_STATE_LISTEN) {
        return;
    }
    if (tal_system_get_millisecond() < sg_vad_ignore_until) {
        return;
    }
    if (__ai_mode_mic_gated() || ai_audio_player_is_playing()) {
        return;
    }
    if (!tal_sw_timer_is_running(sg_enter_idle_timer) &&
        (sg_capture_armed || sg_user_speaking) &&
        !sg_wait_asr) {
        if (sg_in_conversation) {
            __ai_mode_arm_followup_listen();
        } else {
            __ai_mode_arm_first_listen();
        }
    }
    if (sg_user_speaking) {
        return;
    }
    /* Open the mic for this listen window. Do not wait for VAD_START —
     * a missed edge left the board looking like it was listening while
     * uploading nothing. VAD_STOP still ends the utterance. */
    if (sg_capture_armed || sg_pending_speech) {
        __ai_mode_start_user_input();
    }
}

static void __ai_mode_vad_grace_cb(TIMER_ID timer_id, void *arg)
{
    (void)timer_id;
    (void)arg;
    /* After TTS, restart VAD in the quiet that follows the speaker. */
    if (sg_in_conversation && sg_is_wakeup) {
        sg_flush_chime = false;
        ai_audio_input_reset();
    }
    sg_capture_armed = true;
    __ai_mode_try_capture();
}

static void __ai_mode_begin_listen_window(void)
{
    sg_user_speaking = false;
    sg_pending_speech = false;
    sg_flush_chime = false;
    sg_flush_until = 0;
    sg_in_conversation = false;
    sg_wait_asr = false;
    sg_capture_armed = true;
    sg_heard_speech = false;
    ai_audio_input_reset();
    /* Short gate for leftover "Hey Tuya" energy only — no wake chime. */
    sg_vad_ignore_until = tal_system_get_millisecond() + AI_CHAT_KWS_TAIL_MS;
    if (sg_vad_grace_timer) {
        tal_sw_timer_start(sg_vad_grace_timer, AI_CHAT_KWS_TAIL_MS, TAL_TIMER_ONCE);
    }

    sg_is_wakeup = true;
    ai_audio_input_wakeup_set(true);
    __ai_mode_arm_first_listen();
    MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_LISTEN);
}

static void __ai_mode_kws_wakeup(TKL_KWS_WAKEUP_WORD_E wakeup_word)
{
    PR_NOTICE("[====ai_wakeup] word %d", wakeup_word);

    /* CHAT_BREAK while idle reconnects the AI client and the client_run
     * callback used to force Standby — skip it unless we are interrupting TTS. */
    if (sg_mode_set_state == AI_MODE_STATE_SPEAK ||
        sg_mode_cur_state == AI_MODE_STATE_SPEAK) {
        ai_audio_player_stop(AI_AUDIO_PLAYER_ALL);
        tuya_ai_agent_event(AI_EVENT_CHAT_BREAK, 0);
    }

    __ai_mode_begin_listen_window();
}

static void __ai_mode_enter_idle(void)
{
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    if (sg_led_hdl != NULL) {
        tdl_led_set_status(sg_led_hdl, TDL_LED_OFF);
    }
#endif

    tal_sw_timer_stop(sg_enter_idle_timer);
    if (sg_vad_grace_timer) {
        tal_sw_timer_stop(sg_vad_grace_timer);
    }

    ai_audio_input_wakeup_set(false);
    sg_is_wakeup = false;
    sg_user_speaking = false;
    sg_pending_speech = false;
    sg_flush_chime = false;
    sg_flush_until = 0;
    sg_in_conversation = false;
    sg_capture_armed = false;
    sg_wait_asr = false;
    sg_heard_speech = false;
    /* Keep the on-chip engine armed so the next "Hey Tuya" / 你好涂鸦 works. */
    tkl_kws_enable();
}

static void __ai_mode_enter_listen(void)
{
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    if (sg_led_hdl != NULL) {
        tdl_led_flash(sg_led_hdl, 500);
    }
#endif

    sg_is_wakeup = true;
    ai_audio_input_wakeup_set(true);
    __ai_mode_try_capture();
}

static void __ai_mode_enter_upload(void)
{
    PR_DEBUG("[====ai_wakeup] upload");
}

static void __ai_mode_enter_think(void)
{
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    if (sg_led_hdl != NULL) {
        tdl_led_flash(sg_led_hdl, 2000);
    }
#endif

    /* Mute VAD uploads while the LLM answers. Keep the session alive so
     * a late PLAY_END cannot dump us to Standby. */
    ai_audio_input_wakeup_set(false);
}

static void __ai_mode_enter_speak(void)
{
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    if (sg_led_hdl != NULL) {
        tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
    }
#endif

    /* Mute the mic while the board talks — speaker echo was captured as
     * the next user turn and stalled long replies in THINK. */
    ai_audio_input_wakeup_set(false);
    tal_sw_timer_stop(sg_enter_idle_timer);
}

static void __ai_mode_enter_idle_time_cb(TIMER_ID timer_id, void *arg)
{
    if (ai_audio_player_is_playing() ||
        sg_mode_set_state == AI_MODE_STATE_UPLOAD ||
        sg_mode_set_state == AI_MODE_STATE_THINK ||
        sg_mode_set_state == AI_MODE_STATE_SPEAK) {
        /* Finish the in-flight reply or user utterance; then idle. */
        tal_sw_timer_start(timer_id, 200, TAL_TIMER_ONCE);
        return;
    }

    if (sg_capture_armed && !sg_user_speaking && !tuya_ai_agent_is_ready()) {
        PR_DEBUG("[====ai_wakeup] wait for agent before ending listen");
        tal_sw_timer_start(timer_id, 500, TAL_TIMER_ONCE);
        return;
    }

    /* Still talking — keep the window open until VAD hangover. */
    if (tkl_vad_get_status() == TKL_VAD_STATUS_SPEECH) {
        tal_sw_timer_start(timer_id, 200, TAL_TIMER_ONCE);
        return;
    }

    if (sg_user_speaking) {
        PR_NOTICE("[====ai_wakeup] listen window ended, stop capture");
        sg_capture_armed = false;
        sg_user_speaking = false;
        sg_pending_speech = false;
        sg_wait_asr = true;
        tuya_ai_input_stop();
        tal_sw_timer_start(timer_id, 8 * 1000, TAL_TIMER_ONCE);
        return;
    }

    if (sg_wait_asr) {
        PR_NOTICE("[====ai_wakeup] ASR wait timed out");
        sg_wait_asr = false;
    }

    PR_NOTICE("[====ai_wakeup] listen window ended");
    MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_IDLE);
}

static OPERATE_RET __ai_mode_wakeup_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    /* LED is an auxiliary indicator: tolerate the device not being
     * registered (board did not enable LED, or LED name mismatch) so
     * that chat mode can still start. */
    sg_led_hdl = tdl_led_find_dev(LED_NAME);
    if (sg_led_hdl != NULL) {
        OPERATE_RET led_rt = tdl_led_open(sg_led_hdl);
        if (led_rt != OPRT_OK) {
            PR_WARN("LED open failed (rt=%d), continuing without LED", led_rt);
            sg_led_hdl = NULL;
        }
    } else {
        PR_WARN("LED \"%s\" not registered, continuing without LED", LED_NAME);
    }
#endif

    //set vad mode
    ai_audio_input_wakeup_mode_set(AI_AUDIO_VAD_AUTO);

    tkl_kws_reg_wakeup_cb(__ai_mode_kws_wakeup);
    tkl_kws_enable();

    //create idle timer
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__ai_mode_enter_idle_time_cb, NULL, &sg_enter_idle_timer));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__ai_mode_vad_grace_cb, NULL, &sg_vad_grace_timer));

    MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_IDLE);
    sg_is_wakeup = false;

    return rt;
}

static OPERATE_RET __ai_mode_wakeup_deinit(void)
{
    tkl_kws_disable();

    tuya_ai_input_stop();

    sg_mode_cur_state = AI_MODE_STATE_INVALID;

    return OPRT_OK;
}

static OPERATE_RET __ai_mode_wakeup_task(void *args)
{
    if(sg_mode_cur_state == sg_mode_set_state) {
        __ai_mode_try_capture();
        return OPRT_OK;
    }

    switch(sg_mode_set_state) {
        case AI_MODE_STATE_IDLE: {
            __ai_mode_enter_idle();
        }
        break;
        case AI_MODE_STATE_LISTEN: {
            __ai_mode_enter_listen();
        }
        break;
        case AI_MODE_STATE_UPLOAD: {
            __ai_mode_enter_upload();
        }
        break;
        case AI_MODE_STATE_THINK: {
            __ai_mode_enter_think();
        }
        break;
        case AI_MODE_STATE_SPEAK: {
            __ai_mode_enter_speak();
        }
        break;
        default:
        break;
    }

    sg_mode_cur_state = sg_mode_set_state;

    ai_user_event_notify(AI_USER_EVT_MODE_STATE_UPDATE, (void *)sg_mode_cur_state);

    __ai_mode_try_capture();

    return OPRT_OK;
}

static OPERATE_RET __ai_mode_wakeup_handle_event(AI_NOTIFY_EVENT_T *event)
{
    TUYA_CHECK_NULL_RETURN(event, OPRT_INVALID_PARM);

    if(event->type != AI_USER_EVT_MIC_DATA && event->type != AI_USER_EVT_TTS_DATA) {
        PR_DEBUG("[====ai_wakeup] event type: %d", event->type);
    }

    switch (event->type) {
        case AI_USER_EVT_ASR_EMPTY:
        case AI_USER_EVT_ASR_ERROR: {
            sg_wait_asr = false;
            sg_capture_armed = false;
            sg_user_speaking = false;
            sg_pending_speech = false;
            /* Nothing usable — go standby. Another wake/button starts a new turn. */
            MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_IDLE);
        }
        break;
        case AI_USER_EVT_ASR_OK: {
            sg_wait_asr = false;
            sg_capture_armed = false;
            sg_in_conversation = true;
            sg_is_wakeup = true;
            MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_THINK);
        }
        break;
        case AI_USER_EVT_TTS_PRE: {
            /* Long replies can pause the player between sentences. LISTEN must
             * go back to SPEAK or the rest of the answer stalls in THINK. */
            if (sg_mode_set_state != AI_MODE_STATE_IDLE &&
                sg_mode_cur_state != AI_MODE_STATE_IDLE) {
                sg_is_wakeup = true;
                MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_SPEAK);
            }
        }
        break;
        case AI_USER_EVT_EXIT: {
            MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_IDLE);
        }
        break;
        case AI_USER_EVT_PLAY_CTL_END:
        case AI_USER_EVT_PLAY_END: {
            /* A gap in a long TTS stream can STOP then START again. Don't
             * open the mic while audio is still coming. */
            if (ai_audio_player_is_playing()) {
                break;
            }
            /* Button/KWS may stop TTS and already be in LISTEN — don't steal that. */
            if (sg_mode_set_state == AI_MODE_STATE_LISTEN) {
                break;
            }
            /* THINK is included so a missed TTS_PRE cannot leave the mic muted. */
            if (sg_mode_set_state == AI_MODE_STATE_SPEAK ||
                sg_mode_cur_state == AI_MODE_STATE_SPEAK ||
                sg_mode_set_state == AI_MODE_STATE_THINK ||
                sg_mode_cur_state == AI_MODE_STATE_THINK ||
                sg_mode_set_state == AI_MODE_STATE_UPLOAD ||
                sg_mode_cur_state == AI_MODE_STATE_UPLOAD) {
                sg_in_conversation = true;
                sg_user_speaking = false;
                sg_pending_speech = false;
                sg_wait_asr = false;
                sg_capture_armed = true;
                sg_heard_speech = false;
                sg_flush_chime = true;
                sg_vad_ignore_until = 0;
                sg_flush_until = tal_system_get_millisecond() + AI_CHAT_TTS_FLUSH_MS;
                if (sg_vad_grace_timer) {
                    tal_sw_timer_start(sg_vad_grace_timer, AI_CHAT_TTS_FLUSH_MS, TAL_TIMER_ONCE);
                }
                __ai_mode_arm_followup_listen();
                MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_LISTEN);
            }
        }
        break;
        default:
        break;
    }

    return OPRT_OK;
}

static AI_MODE_STATE_E __ai_mode_wakeup_get_state(void)
{
    return sg_mode_set_state;
}

static OPERATE_RET __ai_mode_wakeup_client_run(void *data)
{
    PR_NOTICE("connected to server");

    /* First connect only. A CHAT_BREAK / session recreate must not yank
     * Listening back to Standby. */
    if (sg_mode_set_state == AI_MODE_STATE_INIT ||
        sg_mode_set_state == AI_MODE_STATE_INVALID) {
        MODE_STATE_CHANGE(sg_mode_set_state, AI_MODE_STATE_IDLE);
    }

    return OPRT_OK;
}

static OPERATE_RET __ai_mode_wakeup_vad_change(AI_AUDIO_VAD_STATE_E vad_flag)
{
    if(false == sg_is_wakeup) {
        return OPRT_OK;
    }

    if (sg_mode_set_state != AI_MODE_STATE_LISTEN &&
        sg_mode_cur_state != AI_MODE_STATE_LISTEN) {
        return OPRT_OK;
    }
    if (ai_audio_player_is_playing()) {
        return OPRT_OK;
    }

    /* TTS speaker tail: do not treat it as the user's next turn. */
    if (sg_flush_chime) {
        PR_DEBUG("[====ai_wakeup] vad flush playback: [%d]", vad_flag);
        return OPRT_OK;
    }

    /* Latch START during the short KWS tail so a fast talker is not dropped. */
    if (tal_system_get_millisecond() < sg_vad_ignore_until) {
        if (AI_AUDIO_VAD_START == vad_flag) {
            sg_pending_speech = true;
        }
        PR_DEBUG("[====ai_wakeup] vad during kws tail: [%d]", vad_flag);
        return OPRT_OK;
    }

    PR_DEBUG("[====ai_wakeup] vad: [%d]", vad_flag);

    if (AI_AUDIO_VAD_START == vad_flag) {
        sg_heard_speech = true;
        sg_capture_armed = true;
        __ai_mode_start_user_input();
    } else if (sg_user_speaking && sg_heard_speech) {
        /* End of utterance after real speech. Ignore STOP from wake-word leftover. */
        sg_capture_armed = false;
        sg_user_speaking = false;
        sg_pending_speech = false;
        sg_wait_asr = true;
        tuya_ai_input_stop();
    }

    return OPRT_OK;
}

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
static OPERATE_RET __ai_mode_wakeup_handle_key(TDL_BUTTON_TOUCH_EVENT_E event, void *arg)
{
    OPERATE_RET rt = OPRT_OK;

    switch(event) {
        case TDL_BUTTON_PRESS_SINGLE_CLICK: {
            if (sg_mode_set_state == AI_MODE_STATE_SPEAK ||
                sg_mode_cur_state == AI_MODE_STATE_SPEAK) {
                ai_audio_player_stop(AI_AUDIO_PLAYER_ALL);
                tuya_ai_agent_event(AI_EVENT_CHAT_BREAK, 0);
            }

            __ai_mode_begin_listen_window();
        }
        break;
        default:
            break;
    }

    return rt;

}
#endif

OPERATE_RET ai_mode_wakeup_register(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_MODE_HANDLE_T handle;

    memset(&handle, 0, sizeof(AI_MODE_HANDLE_T));

    handle.name         = WAKEUP_TALK;
    handle.init         = __ai_mode_wakeup_init;
    handle.deinit       = __ai_mode_wakeup_deinit;
    handle.task         = __ai_mode_wakeup_task;
    handle.handle_event = __ai_mode_wakeup_handle_event;
    handle.get_state    = __ai_mode_wakeup_get_state; 
    handle.client_run   = __ai_mode_wakeup_client_run;
    handle.vad_change   = __ai_mode_wakeup_vad_change; 
    
#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
    handle.handle_key   = __ai_mode_wakeup_handle_key;
#endif    

    TUYA_CALL_ERR_RETURN(ai_mode_register(AI_CHAT_MODE_WAKEUP, &handle));

    return rt;
}

#else 
OPERATE_RET ai_mode_wakeup_register(void)
{
    return OPRT_NOT_SUPPORTED;
}
#endif