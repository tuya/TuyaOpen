/**
 * @file ai_chat_main.c
 * @brief AI chat main module implementation
 *
 * This module implements the main AI chat functionality, including mode management,
 * audio input/output, button handling, and configuration management.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "tkl_kws.h"

#include "cJSON.h"
#include "tuya_ai_agent.h"

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
#include "tdl_button_manage.h"
#endif

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
#include "ai_audio_input.h"
#include "ai_audio_player.h"
#endif

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
#include "ai_video_input.h"
#endif

#include "ai_agent.h"
#include "ai_manage_mode.h"
#include "ai_chat_main.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define AI_CHAT_BUTTON_NAME    "ai_chat_button"

#define TUYA_AI_CHAT_PAR       "ty_ai_chat_par"

#define AI_AUDIO_SLICE_TIME         80     
#define AI_AUDIO_VAD_ACTIVE_TIME    200 
#define AI_AUDIO_VAD_OFF_TIME       1000
/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_USER_EVENT_NOTIFY   sg_evt_notify_cb = NULL;
static THREAD_HANDLE          sg_ai_chat_mode_task = NULL;
static AI_CHAT_MODE_E         sg_ai_default_mode = AI_CHAT_MODE_HOLD;
static int                    sg_ai_default_vol = 70;
static bool                   sg_ai_agent_inited = false;

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
static TDL_BUTTON_HANDLE sg_button_hdl = NULL;
#endif

/***********************************************************
***********************function define**********************
***********************************************************/
#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
extern OPERATE_RET ai_chat_ui_init(void);
extern void ai_chat_ui_handle_event(AI_NOTIFY_EVENT_T *event);
#endif

/**
@brief Save chat mode and volume configuration
@param mode Chat mode value
@param volume Volume value
@return OPERATE_RET Operation result
*/
static OPERATE_RET __ai_chat_save_config(uint32_t mode, int volume)
{
    OPERATE_RET rt = OPRT_OK;
    char buf[64] = {0};

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf), "{\"volume\": %d, \"chat_mode\":%d}", volume, (int)mode);
    TUYA_CALL_ERR_RETURN(tal_kv_set(TUYA_AI_CHAT_PAR, (const uint8_t *)buf, strlen(buf)));

    PR_DEBUG("save chat mode config: %s", buf);
    return rt;
}

/**
@brief Load chat mode and volume configuration
@param mode Pointer to store mode value
@param volume Pointer to store volume value
@return OPERATE_RET Operation result
*/
static OPERATE_RET __ai_chat_load_config(uint32_t *mode, int *volume)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t *value = NULL;
    size_t len = 0;
    uint32_t read_mode = 0;
    int read_vol = sg_ai_default_vol;

    if(NULL == mode || NULL == volume) {
        return OPRT_INVALID_PARM;
    }

    /* Read volume from KV */
    TUYA_CALL_ERR_RETURN(tal_kv_get(TUYA_AI_CHAT_PAR, &value, &len));
    PR_DEBUG("read ai_toy config: %s", value);

    cJSON *root = cJSON_Parse((const char *)value);
    tal_kv_free(value);
    TUYA_CHECK_NULL_RETURN(root, OPRT_FILE_READ_FAILED);

    /* Read volume */
    cJSON *volum = cJSON_GetObjectItem(root, "volume");
    if (volum) {
        if (volum->valueint <= 100 && volum->valueint >= 0) {
            read_vol = volum->valueint;
        }
    }

    /* Read trigger mode */
    cJSON *chat_mode = cJSON_GetObjectItem(root, "chat_mode");
    if (chat_mode) {
        read_mode = (uint32_t)chat_mode->valueint;
    }

    cJSON_Delete(root);

    *mode = read_mode;
    *volume = read_vol;

    return OPRT_OK;
}

/**
@brief Handle AI user events
@param event Pointer to event structure
@return None
*/
static void __ai_handle_event(AI_NOTIFY_EVENT_T *event)
{
    if(NULL == event) {
        return;
    }

    ai_mode_handle_event(event);

    switch(event->type) {
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
        case AI_USER_EVT_PLAY_CTL_PLAY:
        case AI_USER_EVT_PLAY_CTL_RESUME:{
            ai_audio_player_set_resume(true);
        }
        break;
        case AI_USER_EVT_PLAY_CTL_PAUSE:{
            ai_audio_player_stop(AI_AUDIO_PLAYER_BG);
        }
        break;
        case AI_USER_EVT_PLAY_CTL_REPLAY:{
            ai_audio_player_set_replay(true);
        }
        break;
        case AI_USER_EVT_PLAY_ALERT:{
            ai_audio_player_alert((AI_AUDIO_ALERT_TYPE_E)event->data);
        }
        break;
#endif
        default:
#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
            ai_chat_ui_handle_event(event);
#endif
        break;
    }

    if (sg_evt_notify_cb) {
        sg_evt_notify_cb(event);
    }   
}

/**
@brief AI chat mode task function
@param args Task arguments
@return None
*/
static void __ai_chat_mode_task(void *args)
{
    while(tal_thread_get_state(sg_ai_chat_mode_task) == THREAD_STATE_RUNNING) {
        ai_mode_task_running(args);
        tal_system_sleep(20);
    }
}

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
/**
@brief Audio output callback function
@param data Pointer to audio data
@param datalen Audio data length
@return int Operation result
*/
static int __ai_audio_output(uint8_t *data, uint16_t datalen)
{
    OPERATE_RET rt = OPRT_OK;
    uint64_t   pts = 0;
    uint64_t   timestamp = 0;

    if(false == sg_ai_agent_inited) {
        return OPRT_OK;
    }

#if defined(ENABLE_AUDIO_AEC) && (ENABLE_AUDIO_AEC == 1)
    timestamp = pts = tal_system_get_millisecond();
    TUYA_CALL_ERR_LOG(tuya_ai_audio_input(timestamp, pts, data, datalen, datalen));
#else 
    if(false == ai_audio_player_is_playing()) {
        timestamp = pts = tal_system_get_millisecond();
        TUYA_CALL_ERR_LOG(tuya_ai_audio_input(timestamp, pts, data, datalen, datalen));
    }
#endif

    return rt;
}

/**
@brief Handle VAD (Voice Activity Detection) state change event callback
@param data Pointer to VAD state value (AI_AUDIO_VAD_STATE_E)
@return OPERATE_RET Operation result
*/
int __ai_vad_change_evt(void *data)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(data, OPRT_INVALID_PARM);

    AI_AUDIO_VAD_STATE_E vad_flag = (AI_AUDIO_VAD_STATE_E)data;

    TUYA_CALL_ERR_RETURN(ai_mode_vad_change(vad_flag));

    return rt;
}

#endif

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
/**
@brief Button function callback
@param name Button name
@param event Button touch event
@param arg Callback argument
@return None
*/
static void __ai_button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *arg)
{
    PR_DEBUG("ai chat button event: %d", event);

    if(TDL_BUTTON_PRESS_DOUBLE_CLICK == event) {
        #if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
        ai_audio_player_stop(AI_AUDIO_PLAYER_ALL);
        #endif

        /* AI agent interrupt */
        tuya_ai_agent_event(AI_EVENT_CHAT_BREAK, 0);

        uint32_t nxt_mode = ai_mode_switch_next();

        /* Save trigger mode */
        int volume = sg_ai_default_vol;

        #if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
        ai_audio_player_get_vol(&volume);
        #endif
        __ai_chat_save_config(nxt_mode, volume);

        #if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
        /* Player alert */
        ai_audio_player_alert(AI_AUDIO_ALERT_LONG_KEY_TALK + nxt_mode);
        #endif

        return;
    }

    ai_mode_handle_key(event, arg);
}

/**
@brief Open button functionality for AI chat mode
@return OPERATE_RET Operation result
*/
static OPERATE_RET __ai_chat_mode_open_button(void)
{
    OPERATE_RET rt = OPRT_OK;

    tdl_button_set_task_stack_size(4096);

    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 400,
                                   .long_keep_timer = 0,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 300};
    TUYA_CALL_ERR_RETURN(tdl_button_create(AI_CHAT_BUTTON_NAME, &button_cfg, &sg_button_hdl));

    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOWN, __ai_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_UP, __ai_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_SINGLE_CLICK, __ai_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOUBLE_CLICK, __ai_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_LONG_PRESS_START, __ai_button_function_cb);

    return rt;
}
#endif

/**
@brief MQTT connected event callback
@param data Event data (unused)
@return int Operation result
*/
static int __ai_mqtt_connected_evt(void *data)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t mode = sg_ai_default_mode;
    int vol = sg_ai_default_vol;

    TUYA_CALL_ERR_RETURN(ai_agent_init());

    __ai_chat_load_config(&mode, &vol);

    ai_mode_init(mode);

    sg_ai_agent_inited = true;

    return rt;
}

static OPERATE_RET __ai_chat_mode_register(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_COMP_AI_MODE_HOLD) && (ENABLE_COMP_AI_MODE_HOLD == 1)
    TUYA_CALL_ERR_RETURN(ai_mode_hold_register());
#endif

#if defined(ENABLE_COMP_AI_MODE_ONESHOT) && (ENABLE_COMP_AI_MODE_ONESHOT == 1)
    TUYA_CALL_ERR_RETURN(ai_mode_oneshot_register());
#endif

#if defined(ENABLE_COMP_AI_MODE_WAKEUP) && (ENABLE_COMP_AI_MODE_WAKEUP == 1)
    TUYA_CALL_ERR_RETURN(ai_mode_wakeup_register());
#endif

#if defined(ENABLE_COMP_AI_MODE_FREE) && (ENABLE_COMP_AI_MODE_FREE == 1)
    TUYA_CALL_ERR_RETURN(ai_mode_free_register());
#endif

    return rt;
}


/**
@brief Initialize AI chat module
@param cfg Chat mode configuration
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_chat_init(AI_CHAT_MODE_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t mode = sg_ai_default_mode;
    int vol = sg_ai_default_vol;

    if(NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(__ai_chat_mode_register());

    sg_ai_default_mode = cfg->default_mode;
    sg_ai_default_vol  = cfg->default_vol;
    mode = sg_ai_default_mode;
    vol  = sg_ai_default_vol;

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    TUYA_CALL_ERR_RETURN(ai_chat_ui_init());
#endif

    rt = __ai_chat_load_config(&mode, &vol);
    if (OPRT_OK != rt) {
        mode = sg_ai_default_mode;
        vol = sg_ai_default_vol;
        __ai_chat_save_config(mode, vol);
        PR_NOTICE("load chat mode config failed, use default mode %d, volume %d", mode, vol);
    }

    sg_evt_notify_cb = cfg->evt_cb;

    ai_user_event_notify_register(__ai_handle_event);

    TUYA_CALL_ERR_RETURN(tal_event_subscribe(EVENT_MQTT_CONNECTED, "ai_agent_init", __ai_mqtt_connected_evt, SUBSCRIBE_TYPE_EMERGENCY));
    TUYA_CALL_ERR_RETURN(tal_event_subscribe(EVENT_AI_CLIENT_RUN, "client_run", ai_mode_client_run, SUBSCRIBE_TYPE_NORMAL));

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
    AI_AUDIO_INPUT_CFG_T input_cfg= {
        .vad_mode      = AI_AUDIO_VAD_MANUAL,
        .vad_off_ms    = AI_AUDIO_VAD_OFF_TIME,
        .vad_active_ms = AI_AUDIO_VAD_ACTIVE_TIME,
        .slice_ms      = AI_AUDIO_SLICE_TIME,
        .output_cb     = __ai_audio_output,  
    };
    TUYA_CALL_ERR_RETURN(ai_audio_input_init(&input_cfg));

    TUYA_CALL_ERR_RETURN(ai_audio_player_init());

    TUYA_CALL_ERR_RETURN(tkl_kws_init());

    TUYA_CALL_ERR_LOG(ai_audio_player_set_vol(vol));

    TUYA_CALL_ERR_RETURN(tal_event_subscribe(EVENT_AUDIO_VAD, "vad_change", __ai_vad_change_evt, SUBSCRIBE_TYPE_NORMAL));
#endif

    THREAD_CFG_T thrd_cfg = {
        .priority = THREAD_PRIO_5,
        .stackDepth = 3* 1024,
        .thrdname = "ai_chat_mode",
        #ifdef ENABLE_EXT_RAM
        .psram_mode = 1,
        #endif            
    };

    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_ai_chat_mode_task, NULL, NULL,\
                                                     __ai_chat_mode_task, NULL, &thrd_cfg));

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
    TUYA_CALL_ERR_LOG(__ai_chat_mode_open_button());
#endif
    PR_DEBUG("ai chat mode init mode %d success", mode);

    return OPRT_OK;
}

/**
@brief Set chat volume
@param volume Volume value (0-100)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_chat_set_volume(int volume)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
    AI_CHAT_MODE_E mode = sg_ai_default_mode;
    TUYA_CALL_ERR_RETURN(ai_audio_player_set_vol(volume));

    /* Save volume */
    ai_mode_get_curr_mode(&mode);
    __ai_chat_save_config(mode, volume);

#endif

    return rt;
}

/**
@brief Get chat volume
@return int Volume value (0-100)
*/
int ai_chat_get_volume(void)
{
    int volume = sg_ai_default_vol;

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(ai_audio_player_get_vol(&volume));
#endif

    return volume;
}

