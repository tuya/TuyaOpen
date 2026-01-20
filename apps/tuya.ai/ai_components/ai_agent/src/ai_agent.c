/**
 * @file ai_agent.c
 * @brief AI agent module implementation
 *
 * This module implements the AI agent functionality, which handles communication
 * with the Tuya AI cloud service. It processes ASR, NLG, and skill data,
 * manages audio playback, and handles various AI events.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "cJSON.h"
#include "atop_service.h"
#include "tuya_ai_agent.h"
#include "tuya_ai_output.h"

#include "ai_user_event.h"
#include "ai_skill.h"

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
#include "tdl_audio_manage.h"
#include "ai_audio_player.h"
#endif

#if defined(ENABLE_AI_MONITOR) && (ENABLE_AI_MONITOR == 1)
#include "tuya_ai_monitor.h"
#endif
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
static uint16_t __s_audio_codec_type = AI_AUDIO_CODEC_MP3;
#endif
/***********************************************************
***********************function define**********************
***********************************************************/
/**
@brief Callback function for AI agent events
@param type Event type
@param ptype Packet type
@param eid Event ID
@return OPERATE_RET Operation result
*/
OPERATE_RET __ai_agent_event_cb(AI_EVENT_TYPE type, AI_PACKET_PT ptype, AI_EVENT_ID eid)
{
    PR_DEBUG("ai agent -> recv event type: %d", type);

    if (AI_EVENT_START == type) {
        if (AI_PT_AUDIO == ptype) {
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
            /* Start audio player */
            ai_audio_play_tts_stream(AI_AUDIO_PLAYER_TTS_START, __s_audio_codec_type, (char*)eid, strlen(eid));
#endif
        }
    } else if ((AI_EVENT_CHAT_BREAK == type)) {
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
        /* Cloud break, stop audio player */
        ai_audio_player_stop(AI_AUDIO_PLAYER_FG);
#endif
        ai_user_event_notify(AI_USER_EVT_CHAT_BREAK, NULL);
    }else if(AI_EVENT_SERVER_VAD == type) {
		ai_user_event_notify(AI_USER_EVT_SERVER_VAD, NULL);
	} else if ((AI_EVENT_END == type)) {
        if (AI_PT_AUDIO == ptype) {
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
            /* Stop audio player */
            ai_audio_play_tts_stream(AI_AUDIO_PLAYER_TTS_STOP, __s_audio_codec_type, (char*)eid, strlen(eid));
#endif
        }
    } else if (AI_EVENT_CHAT_EXIT == type) {
        /* Stop audio player */
        ai_user_event_notify(AI_USER_EVT_EXIT, NULL);
    }

    return OPRT_OK;
}

/**
@brief Callback function for receiving media attributes
@param attr Pointer to media attribute information
@return OPERATE_RET Operation result
*/
OPERATE_RET __ai_agent_media_attr_cb(AI_BIZ_ATTR_INFO_T *attr)
{
    /* PR_DEBUG(" ai agent -> recv media attr type: %d", attr->type); */
    if (attr->type == AI_PT_AUDIO && attr->flag & AI_HAS_ATTR) {
        PR_DEBUG("ai agent -> audio codec type: %d", attr->value.audio.base.codec_type);
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
        switch (attr->value.audio.base.codec_type) {
            case AUDIO_CODEC_MP3:
                __s_audio_codec_type = AI_AUDIO_CODEC_MP3;
                break;
            case AUDIO_CODEC_OPUS:
                __s_audio_codec_type = AI_AUDIO_CODEC_OPUS;
                break;
            default:
                __s_audio_codec_type = AI_AUDIO_CODEC_MAX;
                break;
        }
#endif
    }
    return OPRT_OK;
}

/**
@brief Callback function for receiving media data
@param type Packet type
@param data Pointer to media data
@param len Data length
@param total_len Total data length
@return OPERATE_RET Operation result
*/
OPERATE_RET __ai_agent_media_data_cb(AI_PACKET_PT type, char *data, uint32_t len, uint32_t total_len)
{
    /* TAL_PR_NOTICE(" ai agent -> recv media type %d", type); */
    OPERATE_RET rt = OPRT_OK;
    if(type == AI_PT_AUDIO) {
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
        rt = ai_audio_play_tts_stream(AI_AUDIO_PLAYER_TTS_DATA, __s_audio_codec_type, (char*)data, len);
#endif
    } else if(type == AI_PT_VIDEO) {
        /* TBD */
    } else if(type == AI_PT_IMAGE) {
        /* TBD */
    } else if(type == AI_PT_FILE) {
        /* TBD */
    }
    return rt;
}

/**
@brief Callback function for receiving text data
@param type Text type (ASR, NLG, SKILL, etc.)
@param root JSON root object containing text data
@param eof End of file flag
@return OPERATE_RET Operation result
*/
OPERATE_RET __ai_agent_text_cb(AI_TEXT_TYPE_E type, cJSON *root, bool eof)
{    
    return ai_text_process(type, root, eof);
}

/**
@brief Callback function for receiving alert notifications
@param type Alert type
@return OPERATE_RET Operation result
*/
OPERATE_RET __ai_agent_alert_cb(AI_ALERT_TYPE_E type)
{
    /* PR_DEBUG(" ai agent -> alert type: %d", type); */
    if (type == AT_PLEASE_AGAIN) {
        PR_DEBUG("ignored alert: %d", type);
        return OPRT_OK;
    }

    ai_user_event_notify(AI_USER_EVT_PLAY_ALERT, (void*)type);   

    return OPRT_OK;
}

/**
@brief Initialize the AI agent module
@return OPERATE_RET Operation result
*/OPERATE_RET ai_agent_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_AGENT_CFG_T ai_agent_cfg = {0};

    memset(&ai_agent_cfg, 0x00, sizeof(AI_AGENT_CFG_T));

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)

#if defined(ENABLE_COMP_AI_AUDIO_CODEC_OPUS) && (ENABLE_COMP_AI_AUDIO_CODEC_OPUS == 1)
    ai_agent_cfg.attr.audio.codec_type = AUDIO_CODEC_OPUS;
#elif defined(ENABLE_COMP_AI_AUDIO_CODEC_SPEEX) && (ENABLE_COMP_AI_AUDIO_CODEC_SPEEX == 1)
    ai_agent_cfg.attr.audio.codec_type = AUDIO_CODEC_SPEEX;
#else
    ai_agent_cfg.attr.audio.codec_type = AUDIO_CODEC_PCM;
#endif

    TDL_AUDIO_HANDLE_T audio_hdl = NULL;
    TDL_AUDIO_INFO_T audio_info = {0};
    TUYA_CALL_ERR_RETURN(tdl_audio_find(AUDIO_CODEC_NAME, &audio_hdl));
    TUYA_CALL_ERR_RETURN(tdl_audio_get_info(audio_hdl, &audio_info));

    ai_agent_cfg.attr.audio.sample_rate = audio_info.sample_rate;
    ai_agent_cfg.attr.audio.channels = audio_info.sample_ch_num;
    ai_agent_cfg.attr.audio.bit_depth = audio_info.sample_bits;

    ai_agent_cfg.codec_enable         = TRUE;
#endif

    ai_agent_cfg.output.alert_cb      = __ai_agent_alert_cb;
    ai_agent_cfg.output.text_cb       = __ai_agent_text_cb;
    ai_agent_cfg.output.media_data_cb = __ai_agent_media_data_cb;
    ai_agent_cfg.output.media_attr_cb = __ai_agent_media_attr_cb;
    ai_agent_cfg.output.event_cb      = __ai_agent_event_cb;

    TUYA_CALL_ERR_RETURN(tuya_ai_agent_init(&ai_agent_cfg));
    
#if defined(ENABLE_AI_MONITOR) && (ENABLE_AI_MONITOR == 1)
    ai_monitor_config_t monitor_cfg = AI_MONITOR_CFG_DEFAULT;
    TUYA_CALL_ERR_RETURN(tuya_ai_monitor_init(&monitor_cfg));
#endif

    return rt;
}

/**
@brief Deinitialize the AI agent module
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_agent_deinit(void)
{
    tuya_ai_agent_deinit();

    return OPRT_OK;
}

/**
@brief Send text input to AI agent
@param content Text content to send
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_agent_send_text(char *content)
{
    OPERATE_RET rt = OPRT_OK;

    tuya_ai_input_start(true);
    TUYA_CALL_ERR_RETURN(tuya_ai_text_input((uint8_t *)content, strlen(content), strlen(content)));
    tuya_ai_input_stop();

    return rt;
}

/**
@brief Send file data to AI agent
@param data Pointer to file data
@param len File data length
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_agent_send_file(uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    tuya_ai_input_start(true);
    TUYA_CALL_ERR_RETURN(tuya_ai_file_input((uint8_t *)data, len, len));
    tuya_ai_input_stop();

    return rt;
}

/**
@brief Send image data to AI agent
@param data Pointer to image data
@param len Image data length
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_agent_send_image(uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    uint64_t   timestamp = tal_system_get_millisecond();

    tuya_ai_input_start(true);
    TUYA_CALL_ERR_RETURN(tuya_ai_image_input(timestamp, (uint8_t *)data, len, len));
    tuya_ai_input_stop();

    return rt;
}

/**
@brief Request cloud alert from AI agent
@param type Alert type
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_agent_cloud_alert(AI_ALERT_TYPE_E type)
{    
    char *alert_prompt = NULL;

    PR_NOTICE("ai agent -> request cloud request for %d", type);

    switch (type) {
    case AT_NETWORK_CONNECTED:
        alert_prompt = "cmd:0";
        break;
    // case AT_PLEASE_AGAIN:
    //     alert_prompt = "提示音：联网";
    //     // ty_cJSON_AddStringToObject(json, "eventType", "pleaseAgain");
    //     break;
    case AT_WAKEUP:
        alert_prompt = "cmd:1";
        break;
    case AT_LONG_KEY_TALK:
        alert_prompt = "cmd:2";
        break;
    case AT_KEY_TALK:
        alert_prompt = "cmd:3";
        break;
    case AT_WAKEUP_TALK:
        alert_prompt = "cmd:4";
        break;
    case AT_RANDOM_TALK:
        alert_prompt = "cmd:5";
        break;
    default:
        return OPRT_NOT_SUPPORTED;
    }

    return ai_agent_send_text(alert_prompt);
}

/**
@brief Switch AI agent role
@param role Role name to switch to
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_agent_role_switch(char *role)
{
    OPERATE_RET rt = OPRT_OK;
    cJSON* result = NULL;

    /* char *print_data = NULL; */
    char post_content[128] = {0};
    snprintf(post_content, sizeof(post_content), "{\"commandInfo\": \"%s\"}", role);
	
    TUYA_CALL_ERR_LOG(atop_service_comm_post_simple("thing.ai.agent.switch.role", "1.0",  post_content, NULL, &result));
    TUYA_CHECK_NULL_RETURN(result, OPRT_MID_HTTP_GET_RESP_ERROR);

    /* Free resources */
    cJSON_Delete(result);

    return OPRT_OK;    
}