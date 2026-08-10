/**
 * @file app_lowpower.c
 * @brief Idle low-power (power-management) integration layer for the chatbot app.
 *
 * Idle low-power (key-mode only): in HOLD / push-to-talk ("按键") mode, where the mic is
 * idle between key presses, let the device idle into either WiFi AP keep-alive
 * (ULP_ONLINE: DTIM, uA, still cloud-reachable) or offline deep sleep - chosen by
 * Kconfig. Other modes (oneshot / wakeup / free) keep the mic on, so they stay pinned at
 * ACTIVE. Compiled out (entry points become no-ops) unless ENABLE_APP_LOWPOWER.
 *
 * @version 0.1
 * @date 2025-03-25
 */

#include "tal_api.h"

#include "app_lowpower.h"

#if defined(ENABLE_APP_LOWPOWER) && (ENABLE_APP_LOWPOWER == 1)

#include "tuya_pm.h"
#include "ai_manage_mode.h"
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
#include "ai_audio_input.h"
#endif
#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
#include "lv_vendor.h" // lv_vendor_suspend/resume (stop render + power down flush DMA2D)
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* Idle target scheme + idle time, from Kconfig. */
#if defined(APP_LOWPOWER_DEEP_SLEEP)
#define PM_IDLE_SCHEME TUYA_PM_DEEPSLEEP
#else
#define PM_IDLE_SCHEME TUYA_PM_ULP_ONLINE
#endif
#define PM_IDLE_MS (APP_LOWPOWER_IDLE_TIME_S * 1000)

/***********************************************************
***********************variable define**********************
***********************************************************/
/* Two gates pinned at ACTIVE; the idle scheme is reached only when BOTH are released.
   mode gate: released only in HOLD / push-to-talk mode. link gate: released only while
   cloud-online (ULP_ONLINE's DTIM keep-alive needs the AP associated). */
static TUYA_PM_LOCK_HANDLE sg_mode_lock = NULL;
static TUYA_PM_LOCK_HANDLE sg_link_lock = NULL;
static BOOL_T              sg_mode_held = FALSE;
static BOOL_T              sg_link_held = FALSE;

/***********************************************************
***********************function define**********************
***********************************************************/
static const char *__pm_scheme_str(uint8_t s)
{
    switch (s) {
    case TUYA_PM_ACTIVE:     return "ACTIVE";
    case TUYA_PM_ULP_ONLINE: return "ULP_ONLINE";
    case TUYA_PM_DEEPSLEEP:  return "DEEPSLEEP";
    default:                 return "?";
    }
}

static void __pm_on_change(uint8_t from, uint8_t to, void *arg)
{
    (void)arg;
    PR_NOTICE("[pm] scheme change: %s -> %s", __pm_scheme_str(from), __pm_scheme_str(to));
}

/* Pin ACTIVE unless the chatbot is in HOLD (push-to-talk) mode: only there is the mic idle
   between key presses, so only there is it safe to let the codec sleep and rebuild it on the
   next key press (AI_USER_EVT_KEY_WAKEUP). The other modes keep the mic on, so they stay ACTIVE. */
static void __pm_mode_gate_update(void)
{
    AI_CHAT_MODE_E mode = AI_CHAT_MODE_FREE;
    BOOL_T         lp_mode;

    if (NULL == sg_mode_lock) {
        return;
    }
    ai_mode_get_curr_mode(&mode);
    lp_mode = (AI_CHAT_MODE_HOLD == mode) ? TRUE : FALSE;

    if (lp_mode && sg_mode_held) {
        tuya_pm_lock_release(sg_mode_lock); // hold/push-to-talk -> allow idle descent to ULP
        sg_mode_held = FALSE;
    } else if (!lp_mode && !sg_mode_held) {
        tuya_pm_lock_acquire(sg_mode_lock); // other modes (mic always on) -> pin ACTIVE
        sg_mode_held = TRUE;
    }
}

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
/* Mic-capture consumer. Below ACTIVE (idle scheme), deinit the audio input: this stops the
   capture thread AND the codec/DMA (via tdl_audio_close), which is what actually lets the
   CPU sleep and drops the ~18mA. On return to ACTIVE, reinit rebuilds capture from the
   saved cfg. (ai_audio_input_stop alone only stops VAD, not the codec.) */
static OPERATE_RET __pm_mic_suspend(void *arg)
{
    (void)arg;
    PR_NOTICE("[pm] suspend mic: deinit audio input (stop codec)");
    return ai_audio_input_deinit();
}
static OPERATE_RET __pm_mic_resume(void *arg)
{
    (void)arg;
    PR_NOTICE("[pm] resume mic: reinit audio input");
    return ai_audio_input_reinit();
}
#endif

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
/* Display consumer: below ACTIVE, stop the LVGL render task and power down the flush DMA2D.
   The always-on DMA2D power domain was the entire ~5mA; with it off the panel is left OPEN
   but idle, which is already uA (an idle SSD2683 draws almost nothing - a full deep-sleep +
   HW-wake cycle is not worth it and its heavy re-init/full-refresh would block the key-handler
   resume path and hang). resume just restarts DMA2D + the render task, so it's light and safe. */
static OPERATE_RET __pm_disp_suspend(void *arg)
{
    (void)arg;
    PR_NOTICE("[pm] suspend display: stop LVGL + DMA2D");
    lv_vendor_suspend();
    return OPRT_OK;
}
static OPERATE_RET __pm_disp_resume(void *arg)
{
    (void)arg;
    PR_NOTICE("[pm] resume display: LVGL + DMA2D");
    lv_vendor_resume();
    return OPRT_OK;
}
#endif

/* Compose the two-level chain, bind tdl_power, register the mic consumer, start pinned
   ACTIVE behind the mode + link gates. BLE-off / WiFi-PS are handled inside the built-in
   ULP_ONLINE scheme's enter(). */
void app_lowpower_init(void)
{
    OPERATE_RET rt;
    TUYA_PM_CONSUMER_HANDLE h = NULL;
    (void)h;

    /* Two levels: full-speed ACTIVE and the configured idle scheme (AP keep-alive or
       deep sleep); descend after PM_IDLE_MS of idle. */
    static const TUYA_PM_CHAIN_STEP_T chain[] = {
        {TUYA_PM_ACTIVE, 0         },
        {PM_IDLE_SCHEME, PM_IDLE_MS},
    };
    rt = tuya_pm_set_chain(chain, sizeof(chain) / sizeof(chain[0]));
    if (OPRT_OK != rt) {
        PR_ERR("pm set_chain failed: %d", rt);
        return;
    }
    rt = tuya_pm_init(POWER_NAME);
    if (OPRT_OK != rt) {
        PR_ERR("pm init failed: %d", rt);
        return;
    }
    tuya_pm_on_change(__pm_on_change, NULL);

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
    /* Mic powered only at ACTIVE; suspend (deinit codec) when the scheme goes deeper. */
    static const TUYA_PM_CONSUMER_T cMIC = {
        .name              = "mic",
        .min_powered_level = TUYA_PM_ACTIVE,
        .priority          = 10,
        .suspend           = __pm_mic_suspend,
        .resume            = __pm_mic_resume,
    };
    tuya_pm_consumer_register(&cMIC, &h);
#endif

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    /* Display powered only at ACTIVE; below it stop LVGL + power down the flush DMA2D. */
    static const TUYA_PM_CONSUMER_T cDISP = {
        .name              = "display",
        .min_powered_level = TUYA_PM_ACTIVE,
        .priority          = 5,
        .suspend           = __pm_disp_suspend,
        .resume            = __pm_disp_resume,
    };
    tuya_pm_consumer_register(&cDISP, &h);
#endif

    /* Both gates start held -> stay ACTIVE until key-mode AND cloud-online are satisfied. */
    tuya_pm_lock_create("mode", TUYA_PM_ACTIVE, &sg_mode_lock);
    tuya_pm_lock_create("link", TUYA_PM_ACTIVE, &sg_link_lock);
    tuya_pm_lock_acquire(sg_mode_lock);
    sg_mode_held = TRUE;
    tuya_pm_lock_acquire(sg_link_lock);
    sg_link_held = TRUE;

    PR_NOTICE("[pm] idle low-power ready: %s in HOLD/push-to-talk mode after %d s idle",
              __pm_scheme_str(PM_IDLE_SCHEME), APP_LOWPOWER_IDLE_TIME_S);
}

/* Drive activity / re-arm the key-mode gate from chatbot user events. */
void app_lowpower_feed_event(AI_NOTIFY_EVENT_T *event)
{
    if (NULL == event) {
        return;
    }
    switch (event->type) {
    case AI_USER_EVT_KEY_WAKEUP:
        tuya_pm_activity(); // key press -> resume codec synchronously before capture
        break;
    case AI_USER_EVT_MODE_SWITCH:
        __pm_mode_gate_update(); // mode changed (double-click) -> re-arm the key-mode gate
        tuya_pm_activity();
        break;
    case AI_USER_EVT_MODE_STATE_UPDATE: {
        AI_MODE_STATE_E st = (AI_MODE_STATE_E)(intptr_t)event->data;
        if (AI_MODE_STATE_IDLE != st && AI_MODE_STATE_INIT != st) {
            tuya_pm_activity(); // listen/upload/think/speak = active -> bounce to ACTIVE
        }
        // IDLE -> let idle decay run toward the idle scheme
    } break;
    case AI_USER_EVT_ASR_OK:
    case AI_USER_EVT_TTS_PRE:
    case AI_USER_EVT_TTS_START:
        tuya_pm_activity();
        break;
    default:
        break;
    }
}

void app_lowpower_set_online(BOOL_T online)
{
    if (NULL == sg_link_lock) {
        return;
    }
    if (online && sg_link_held) {
        tuya_pm_lock_release(sg_link_lock); // associated -> allow descent (if key-mode too)
        sg_link_held = FALSE;
        __pm_mode_gate_update();            // mode is initialized by the time we're online
    } else if (!online && !sg_link_held) {
        tuya_pm_lock_acquire(sg_link_lock); // offline -> pin ACTIVE
        sg_link_held = TRUE;
    }
}

#else /* !ENABLE_APP_LOWPOWER */

void app_lowpower_init(void)
{
}

void app_lowpower_feed_event(AI_NOTIFY_EVENT_T *event)
{
    (void)event;
}

void app_lowpower_set_online(BOOL_T online)
{
    (void)online;
}

#endif /* ENABLE_APP_LOWPOWER */
