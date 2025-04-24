/**
 * @file ai_audio_main.c
 * @brief Main implementation file for the audio module, which handles audio initialization, 
 *        volume control, open/close operations, and work mode settings.
 *
 * This file contains the core functions for initializing and managing the audio module, 
 * including setting up input handling, volume management, and different work modes.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tkl_audio.h"
#include "tkl_queue.h"
#include "tkl_memory.h"
#include "tkl_thread.h"
#include "tkl_asr.h"

#include "tal_api.h"
#include "ai_audio.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_AUDIO_SPEAK_VOLUME_KEY "spk_volume"

#define AI_AUDIO_INPUT_EVT_CHANGE(last_evt, new_evt)                                                                \
    do {                                                                                                            \
        if(last_evt != new_evt) {                                                                                   \
            PR_DEBUG("ai audio event changed: %d->%d", last_evt, new_evt);                                          \
        }                                                                                                           \
    } while (0)

#define AI_AUDIO_STATE_EVT_CHANGE(last_state, new_state)                                                           \
do {                                                                                                               \
    if(last_state != new_state) {                                                                                  \
        PR_DEBUG("ai audio state changed: %d->%d", last_state, new_state);                                         \
    }                                                                                                              \
} while (0)    

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint32_t frame_len;
    uint8_t *frame;
} AI_AUDIO_FRAME_MSG_T;

typedef enum {
    AI_AUDIO_STATE_DETECT_WAKEUP,
    AI_AUDIO_STATE_UPLAOD,
    AI_AUDIO_STATE_WAIT_CLOUD_ASR,
    AI_AUDIO_STATE_GET_CLOD_ASR,
    AI_AUDIO_STATE_PLAYER_AI_RESP,
    AI_AUDIO_STATE_PLAYER_LAST_AI_RESP,
}AI_AUDIO_STATE_E;
/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_AUDIO_INFORM_CB sg_ai_agent_inform_cb = NULL;
static AI_AUDIO_WORK_MODE_E sg_ai_audio_work_mode = AI_AUDIO_MODE_MANUAL_SINGLE_TALK;
/***********************************************************
***********************function define**********************
***********************************************************/
static void __ai_audio_agent_msg_cb(AI_AGENT_MSG_T *msg)
{
    AI_AUDIO_EVENT_E event = AI_AUDIO_EVT_NONE;

    switch (msg->type) {
    case AI_AGENT_MSG_TP_TEXT_ASR: {
        if (msg->data_len > 0) {
            // Prepare to play mp3
            if (ai_audio_player_is_playing()) {
                PR_DEBUG("player is playing, stop it first");
                ai_audio_player_stop();
            }
            ai_audio_player_start();
        }

        ai_audio_cloud_stop_wait_asr();

        event = AI_AUDIO_EVT_HUMAN_ASR_TEXT;
    } 
    break;
    case AI_AGENT_MSG_TP_AUDIO_START: {
    }
    break;
    case AI_AGENT_MSG_TP_AUDIO_DATA: {
        ai_audio_player_data_write(msg->data, msg->data_len, 0);
    } 
    break;
    case AI_AGENT_MSG_TP_AUDIO_STOP: {
        ai_audio_player_data_write(msg->data, msg->data_len, 1);
    } 
    break;
    case AI_AGENT_MSG_TP_TEXT_NLG: {
        event = AI_AUDIO_EVT_AI_REPLIES_TEXT;
    } 
    break;
    case AI_AGENT_MSG_TP_EMOTION: {
        event = AI_AUDIO_EVT_AI_REPLIES_EMO;
    } 
    break;
    default:
        break;
    }

    if (sg_ai_agent_inform_cb) {
        sg_ai_agent_inform_cb(event, msg->data, msg->data_len, NULL);
    }
}

static void __ai_audio_input_inform_handle(AI_AUDIO_INPUT_EVENT_E event, void *arg)
{
    static AI_AUDIO_INPUT_EVENT_E last_evt = 0xFF;

    AI_AUDIO_INPUT_EVT_CHANGE(last_evt, event);

    last_evt = event;

    switch (event) {    
    case AI_AUDIO_INPUT_EVT_WAKEUP:{
        if(AI_CLOUD_ASR_STATE_IDLE == ai_audio_cloud_asr_get_state()) {
            ai_audio_cloud_asr_start();
    
            if (sg_ai_agent_inform_cb) {
                sg_ai_agent_inform_cb(AI_AUDIO_EVT_WAKEUP, NULL, 0, NULL);
            }
        }
    } 
    break;
    case AI_AUDIO_INPUT_EVT_AWAKE_STOP:{
        ai_audio_cloud_asr_stop();
     }
     break;
    }
}

static AI_AUDIO_INPUT_WAKEUP_TP_E __get_input_wakeup_type(AI_AUDIO_WORK_MODE_E work_mode)
{
    AI_AUDIO_INPUT_WAKEUP_TP_E wakeup_tp = 0;

    if(work_mode == AI_AUDIO_MODE_MANUAL_SINGLE_TALK) {
        wakeup_tp = AI_AUDIO_INPUT_WAKEUP_MANUAL;
    }else if(work_mode == AI_AUDIO_WORK_MANUAL_FREE_TALK){
        wakeup_tp = AI_AUDIO_INPUT_WAKEUP_VAD;
    }else {
        wakeup_tp = AI_AUDIO_INPUT_WAKEUP_VAD;
    }

    return wakeup_tp;
}

/**
 * @brief Initializes the audio module with the provided configuration.
 * @param cfg Pointer to the configuration structure for the audio module.
 * @return OPERATE_RET - OPRT_OK if initialization is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_init(AI_AUDIO_CONFIG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    AI_AUDIO_INPUT_CFG_T input_cfg;

    if (NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    input_cfg.wakeup_tp = __get_input_wakeup_type(cfg->work_mode);
    sg_ai_audio_work_mode = cfg->work_mode;

    TUYA_CALL_ERR_RETURN(ai_audio_input_init(&input_cfg, __ai_audio_input_inform_handle));

    TUYA_CALL_ERR_RETURN(tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, ai_audio_get_volume()));

    TUYA_CALL_ERR_RETURN(ai_audio_cloud_asr_init());

    TUYA_CALL_ERR_RETURN(ai_audio_player_init());

    TUYA_CALL_ERR_RETURN(ai_audio_agent_init(__ai_audio_agent_msg_cb));
    sg_ai_agent_inform_cb = cfg->inform_cb;

    return OPRT_OK;
}

/**
 * @brief Sets the volume for the audio module.
 * @param volume The volume level to set.
 * @return OPERATE_RET - OPRT_OK if the volume is set successfully, otherwise an error code.
 */
OPERATE_RET ai_audio_set_volume(uint8_t volume)
{
    OPERATE_RET rt = OPRT_OK;

    // kv storage
    TUYA_CALL_ERR_LOG(tal_kv_set(AI_AUDIO_SPEAK_VOLUME_KEY, &volume, sizeof(volume)));
    TUYA_CALL_ERR_LOG(tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, volume));

    return rt;
}

/**
 * @brief Retrieves the current volume setting for the audio module.
 * @param None
 * @return uint8_t - The current volume level.
 */
uint8_t ai_audio_get_volume(void)
{
    OPERATE_RET rt = OPRT_OK;

    uint8_t volume = 0;
    uint8_t *value = NULL;
    size_t read_len = 0;

    // kv read
    TUYA_CALL_ERR_LOG(tal_kv_get(AI_AUDIO_SPEAK_VOLUME_KEY, &value, &read_len));
    if (OPRT_OK != rt || NULL == value) {
        PR_ERR("read volume failed");
        volume = 50;
    } else {
        volume = *value;
    }

    PR_DEBUG("get spk volume: %d", volume);

    if (value) {
        tal_kv_free(value);
        value = NULL;
    }

    return volume;
}

OPERATE_RET ai_audio_set_open(bool is_open)
{
    if(true == is_open) {
        ai_audio_input_enable_wakeup(true);
    }else {
        ai_audio_input_enable_wakeup(false);

        if (ai_audio_player_is_playing()) {
            PR_DEBUG("player is playing, stop it first");
            ai_audio_player_stop();
        }

        ai_audio_cloud_asr_set_idle();
    }

    return OPRT_OK;
}

OPERATE_RET ai_audio_manual_start_single_talk(void)
{
    if(sg_ai_audio_work_mode != AI_AUDIO_MODE_MANUAL_SINGLE_TALK) {
        return OPRT_COM_ERROR;
    }

    ai_audio_input_manual_set_wakeup(true);

    return OPRT_OK;
}

OPERATE_RET ai_audio_manual_stop_single_talk(void)
{
    if(sg_ai_audio_work_mode != AI_AUDIO_MODE_MANUAL_SINGLE_TALK) {
        return OPRT_COM_ERROR;
    }

    ai_audio_input_manual_set_wakeup(false);

    return OPRT_OK;

}
