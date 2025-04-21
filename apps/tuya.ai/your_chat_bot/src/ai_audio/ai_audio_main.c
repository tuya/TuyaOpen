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

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint32_t frame_len;
    uint8_t *frame;
} AI_AUDIO_FRAME_MSG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static bool sg_ai_audio_is_work = false;
static AI_AGENT_MSG_CB sg_ai_agent_msg_cb = NULL;
static AI_AUDIO_WORK_MODE_E sg_ai_audio_work_mode = AI_AUDIO_WORK_MODE_HOLD;
static bool sg_is_ai_speaking = false;
static bool sg_is_enable_intrrupt = false;
/***********************************************************
***********************function define**********************
***********************************************************/
static void __ai_audio_agent_msg_cb(AI_AGENT_MSG_T *msg)
{
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
    } break;
    case AI_AGENT_MSG_TP_AUDIO_DATA: {
        sg_is_ai_speaking = true;
        ai_audio_player_data_write(msg->data, msg->data_len, 0);
    } break;
    case AI_AGENT_MSG_TP_AUDIO_STOP: {
        ai_audio_player_data_write(msg->data, msg->data_len, 1);

        if (AI_AUDIO_WORK_MODE_WAKEUP == sg_ai_audio_work_mode) {
            ai_audio_input_restart_asr_detect_wakeup_word();
        }else if(AI_AUDIO_WORK_MODE_FREE == sg_ai_audio_work_mode) {
            ai_audio_input_trigger_asr_awake();
        }
        sg_is_ai_speaking = false;
    } break;
    default:
        break;
    }

    if (sg_ai_agent_msg_cb) {
        sg_ai_agent_msg_cb(msg);
    }
}

static void __ai_audio_input_inform_handle(AI_AUDIO_INPUT_EVENT_E event, uint8_t *data, uint32_t len, void *arg)
{
    static AI_AUDIO_INPUT_EVENT_E last_evt = 0xFF;

    AI_AUDIO_INPUT_EVT_CHANGE(last_evt, event);

    switch (event) {    
    case AI_AUDIO_INPUT_EVT_IDLE:
        ai_audio_cloud_asr_rb_reset();
        ai_audio_cloud_asr_stop();
        break;
    case AI_AUDIO_INPUT_EVT_ENTER_DETECT:
        ai_audio_cloud_asr_input(data, len);
        ai_audio_cloud_asr_stop();
        break;
    case AI_AUDIO_INPUT_EVT_DETECTING:
        ai_audio_cloud_asr_vad_input(data, len);
        break;
    case AI_AUDIO_INPUT_EVT_ASR_WORD:
        if (sg_is_ai_speaking) {
            PR_NOTICE("intrrupt");
            ai_audio_agent_upload_intrrupt();
        }

        ai_audio_input_stop_wakeup_feed(true);
        ai_audio_player_play_alert_syn(AI_AUDIO_ALERT_WAKEUP);
        ai_audio_input_stop_wakeup_feed(false);

#if defined(ENABLE_ECHO_CANCELLATION) && (ENABLE_ECHO_CANCELLATION == 1)
        ai_audio_input_rb_reset();
#endif
        ai_audio_cloud_asr_rb_reset();
        break;
    case AI_AUDIO_INPUT_EVT_WAKEUP: {
        if (sg_is_ai_speaking) {
            if(sg_ai_audio_work_mode == AI_AUDIO_WORK_MODE_FREE) {
                PR_NOTICE("awake intrrupt");
                ai_audio_agent_upload_intrrupt();
            }else {
                break;
            }
        }

#if defined(ENABLE_MIC2) && (ENABLE_MIC2 == 1)
#else 
        ai_audio_cloud_asr_input(data, len);
#endif
        ai_audio_cloud_asr_start();
    } break;
    case AI_AUDIO_INPUT_EVT_AWAKE:
        if (sg_is_ai_speaking) {
            if(sg_ai_audio_work_mode == AI_AUDIO_WORK_MODE_FREE) {
                PR_NOTICE("awake intrrupt");
                ai_audio_agent_upload_intrrupt();

                ai_audio_cloud_asr_input(data, len);
                ai_audio_cloud_asr_start();
            }else {
                break;
            }
        }else {
            ai_audio_cloud_asr_input(data, len);
        }

        break;
     }

     last_evt = event;
}

static AI_AUDIO_INPUT_WAKEUP_TP_E __get_input_wakeup_type(AI_AUDIO_WORK_MODE_E work_mode)
{
    AI_AUDIO_INPUT_WAKEUP_TP_E wakeup_tp = 0;

    if(work_mode == AI_AUDIO_WORK_MODE_HOLD) {
        wakeup_tp = AI_AUDIO_INPUT_WAKEUP_MANUAL;
    }else if(work_mode == AI_AUDIO_WORK_MODE_TRIGGER){
        wakeup_tp = AI_AUDIO_INPUT_WAKEUP_VAD;
    }else {
        wakeup_tp = AI_AUDIO_INPUT_WAKEUP_ASR;
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

    if(cfg->work_mode == AI_AUDIO_WORK_MODE_WAKEUP ||\
       cfg->work_mode == AI_AUDIO_WORK_MODE_FREE ) {
        sg_is_enable_intrrupt = true;
    }

    TUYA_CALL_ERR_RETURN(ai_audio_input_init(&input_cfg, __ai_audio_input_inform_handle));

    TUYA_CALL_ERR_RETURN(tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, ai_audio_get_volume()));

    TUYA_CALL_ERR_RETURN(ai_audio_cloud_asr_init(sg_is_enable_intrrupt));

    TUYA_CALL_ERR_RETURN(ai_audio_player_init());

    TUYA_CALL_ERR_RETURN(ai_audio_agent_init(__ai_audio_agent_msg_cb));
    sg_ai_agent_msg_cb = cfg->agent_msg_cb;

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

/**
 * @brief Sets the open state of the audio module.
 * @param is_open Boolean value indicating whether to open (true) or close (false) the audio module.
 * @return OPERATE_RET - OPRT_OK if the operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_set_open(bool is_open)
{
    if(sg_ai_audio_is_work == is_open){
        PR_NOTICE("ai audio is already %d", is_open);
        return OPRT_OK;
    }

    if(is_open) {
        ai_audio_input_enable_detect_wakeup();
    }else {
        if (ai_audio_player_is_playing()) {
            ai_audio_player_stop();
        }

        ai_audio_input_disable_detect_wakeup();
    }

    sg_is_ai_speaking = false;
    sg_ai_audio_is_work = is_open;

    return OPRT_OK;
}

/**
 * @brief Sets the work mode of the audio module.
 * @param work_mode The work mode to set for the audio module.
 * @return OPERATE_RET - OPRT_OK if the operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_set_work_mode(AI_AUDIO_WORK_MODE_E work_mode)
{
    if(true == sg_ai_audio_is_work) {
        PR_ERR("audio module is open please close it first");
        return OPRT_COM_ERROR;
    }

    ai_audio_cloud_asr_idle();
    ai_audio_cloud_asr_rb_reset();

    ai_audio_input_set_wakeup_tp(__get_input_wakeup_type(work_mode));

    if(work_mode == AI_AUDIO_WORK_MODE_WAKEUP ||\
       work_mode == AI_AUDIO_WORK_MODE_FREE ) {
        sg_is_enable_intrrupt = true;
    }else {
        sg_is_enable_intrrupt = false;
    }
    ai_audio_cloud_asr_enable_intrrupt(sg_is_enable_intrrupt);

    sg_ai_audio_work_mode = work_mode;

    return OPRT_OK;
}
