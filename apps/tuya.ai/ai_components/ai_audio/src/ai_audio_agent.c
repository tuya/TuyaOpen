/**
 * @file app_ai_service.c
 * @brief Provides functions for initializing, starting, uploading, and stopping AI services.
 *
 * This file contains the implementation of functions that manage the AI service module,
 * including initialization, starting the upload process, uploading audio data, and stopping
 * the upload process. It handles AI sessions, event subscriptions, and data transmission
 * to the AI server.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tkl_memory.h"
#include "tal_api.h"

#include "tuya_iot.h"
#include "tuya_iot_dp.h"
#include "cJSON.h"

#include "ai_audio.h"

#include "tuya_ai_agent.h"
#include "tuya_ai_monitor.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
// clang-format off
typedef struct {
    AI_AGENT_CBS_T           cbs;
} AI_AGENT_T;
// clang-format on

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_AGENT_T sg_ai = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

/***********************************************************
 *                  AI Agent Callback Functions
 ***********************************************************/
/**
 * @brief AI agent alert callback
 *
 * @param[in] type Alert type
 * @return OPERATE_RET Operation result
 */
static OPERATE_RET __ai_agent_alert_cb(AI_ALERT_TYPE_E type)
{
    PR_DEBUG("AI agent alert callback, type: %d", type);
    if (sg_ai.cbs.ai_agent_alert_cb) {
        sg_ai.cbs.ai_agent_alert_cb(type);
    }
    return OPRT_OK;
}

/**
 * @brief Process AI ASR (Automatic Speech Recognition) response
 *
 * @param[in] type Text type
 * @param[in] root JSON root object
 * @param[in] eof End of frame flag
 * @return OPERATE_RET Operation result
 */
static OPERATE_RET __ai_asr_process(cJSON *root, BOOL_T eof)
{
    AI_AGENT_MSG_T ai_msg = {0};

    const char *text = cJSON_GetStringValue(root);

    if (text == NULL || text[0] == '\0') {
        PR_DEBUG("ASR empty");
        ai_msg.data = NULL;
        ai_msg.data_len = 0;
    } else {
        PR_DEBUG("ASR text: %s", text);
        ai_msg.data = (uint8_t *)text;
        ai_msg.data_len = strlen(text);
    }
    ai_msg.type = AI_AGENT_MSG_TP_TEXT_ASR;

    if (sg_ai.cbs.ai_agent_msg_cb) {
        sg_ai.cbs.ai_agent_msg_cb(&ai_msg);
    }

    return OPRT_OK;
}

/**
 * @brief Process AI skill response
 *
 * @param[in] type Text type
 * @param[in] root JSON root object
 * @param[in] eof End of frame flag
 * @return OPERATE_RET Operation result
 */
static OPERATE_RET __ai_skill_process(cJSON *root, BOOL_T eof)
{
    AI_AUDIO_EMOTION_T ai_emotion = {
        .name = NULL,
        .text = NULL,
    };

    const cJSON *node = cJSON_GetObjectItem(root, "code");
    const char *code = cJSON_GetStringValue(node);

    if (!code) {
        return OPRT_OK;
    }

    PR_DEBUG("Skill code: %s", code);

    // Process emotion skill
    // Example: {"code":"emo","skillContent":{"emotion":["NEUTRAL"],"text":["😐"]}}
    if (strcmp(code, "emo") == 0) {
        cJSON *skillContent = cJSON_GetObjectItem(root, "skillContent");
        if (!skillContent) {
            return OPRT_OK;
        }

        cJSON *text = cJSON_GetObjectItem(skillContent, "text");
        if (text) {
            char *text_str = cJSON_GetStringValue(cJSON_GetArrayItem(text, 0));
            if (text_str) {
                ai_emotion.text = text_str;
                PR_DEBUG("Emotion text: %s", text_str);
            }
        }

        cJSON *emotion = cJSON_GetObjectItem(skillContent, "emotion");
        if (emotion) {
            char *emotion_str = cJSON_GetStringValue(cJSON_GetArrayItem(emotion, 0));
            if (emotion_str) {
                ai_emotion.name = emotion_str;
                PR_DEBUG("Emotion: %s", emotion_str);
            }
        }

        AI_AGENT_MSG_T ai_msg = {
            .type = AI_AGENT_MSG_TP_EMOTION,
            .data_len = sizeof(AI_AUDIO_EMOTION_T),
            .data = (uint8_t *)&ai_emotion,
        };

        if (sg_ai.cbs.ai_agent_msg_cb) {
            sg_ai.cbs.ai_agent_msg_cb(&ai_msg);
        }
    }

    return OPRT_OK;
}

/**
 * @brief Process AI NLG (Natural Language Generation) response
 *
 * @param[in] type Text type
 * @param[in] root JSON root object
 * @param[in] eof End of frame flag
 * @return OPERATE_RET Operation result
 */
static OPERATE_RET __ai_nlg_process(cJSON *root, BOOL_T eof)
{
    // Example: {"content":"Hello!","appendMode":"append","timeIndex":1000,"finish":false,"tags":""}

    static uint8_t is_first_nlg = TRUE;

    AI_AGENT_MSG_T ai_msg = {0};

    // get content string
    const char *content = cJSON_GetStringValue(cJSON_GetObjectItem(root, "content"));

    // PR_DEBUG("NLG text: %s", content);

    if (is_first_nlg) {
        is_first_nlg = FALSE;
        ai_msg.type = AI_AGENT_MSG_TP_TEXT_NLG_START;
        ai_msg.data_len = 0;
        ai_msg.data = NULL;
        // Compatible with old version
        if (sg_ai.cbs.ai_agent_msg_cb) {
            sg_ai.cbs.ai_agent_msg_cb(&ai_msg);
        }
    }

    if (eof) {
        is_first_nlg = TRUE;
        ai_msg.type = AI_AGENT_MSG_TP_TEXT_NLG_STOP;
        ai_msg.data_len = 0;
        ai_msg.data = NULL;
        // Compatible with old version
        if (sg_ai.cbs.ai_agent_msg_cb) {
            sg_ai.cbs.ai_agent_msg_cb(&ai_msg);
        }
    }

    if (content) {
        ai_msg.type = AI_AGENT_MSG_TP_TEXT_NLG_DATA;
        ai_msg.data_len = strlen(content);
        ai_msg.data = (uint8_t *)content;
        if (sg_ai.cbs.ai_agent_msg_cb) {
            sg_ai.cbs.ai_agent_msg_cb(&ai_msg);
        }
    }

    return OPRT_OK;
}

/**
 * @brief AI agent text callback
 *
 * @param[in] type Text type (skill or NLG)
 * @param[in] root JSON root object
 * @param[in] eof End of frame flag
 * @return OPERATE_RET Operation result
 */
static OPERATE_RET __ai_agent_text_cb(AI_TEXT_TYPE_E type, cJSON *root, BOOL_T eof)
{
    switch (type) {
    case AI_TEXT_ASR:
        return __ai_asr_process(root, eof);

    case AI_TEXT_SKILL:
        return __ai_skill_process(root, eof);

    case AI_TEXT_NLG:
        return __ai_nlg_process(root, eof);

    default:
        return OPRT_OK;
    }
}

/**
 * @brief AI agent media data callback
 *
 * @param[in] type Packet type
 * @param[in] data Media data
 * @param[in] len Data length
 * @param[in] total_len Total data length
 * @return OPERATE_RET Operation result
 */
static OPERATE_RET __ai_agent_media_data_cb(AI_PACKET_PT type, CHAR_T *data, UINT_T len, UINT_T total_len)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("Media data callback, type: %d, len: %d, total_len: %d", type, len, total_len);

    if (type == AI_PT_AUDIO) {
        if (len > 0) {
            AI_AGENT_MSG_T ai_msg = {0};
            ai_msg.type = AI_AGENT_MSG_TP_AUDIO_DATA;
            ai_msg.data_len = len;
            ai_msg.data = (uint8_t *)data;

            sg_ai.cbs.ai_agent_msg_cb(&ai_msg);
        }
    }

    return rt;
}

/**
 * @brief AI agent media attribute callback
 *
 * @param[in] attr Media attribute info
 * @return OPERATE_RET Operation result
 */
static OPERATE_RET __ai_agent_media_attr_cb(AI_BIZ_ATTR_INFO_T *attr)
{
    PR_DEBUG("Media attribute type: %d", attr->type);
    return OPRT_OK;
}

/**
 * @brief AI agent event callback
 *
 * @param[in] etype Event type
 * @param[in] ptype Packet type
 * @param[in] eid Event ID
 * @return OPERATE_RET Operation result
 */
static OPERATE_RET __ai_agent_event_cb(AI_EVENT_TYPE etype, AI_PACKET_PT ptype, AI_EVENT_ID eid)
{
    PR_DEBUG("Event type: %d", etype);

    if (AI_EVENT_START == etype) {
        if (AI_PT_AUDIO == ptype) {
            AI_AGENT_MSG_T ai_msg = {
                .type = AI_AGENT_MSG_TP_AUDIO_START,
                .data_len = 0,
                .data = NULL,
            };
            sg_ai.cbs.ai_agent_msg_cb(&ai_msg);
        }
    } else if ((AI_EVENT_CHAT_BREAK == etype)) {
        // TODO: cloud break, stop audio player
    } else if (AI_EVENT_END == etype) {
        if (AI_PT_AUDIO == ptype) {
            AI_AGENT_MSG_T ai_msg = {0};
            ai_msg.type = AI_AGENT_MSG_TP_AUDIO_STOP;
            ai_msg.data_len = 0;
            ai_msg.data = NULL;

            sg_ai.cbs.ai_agent_msg_cb(&ai_msg);
        }
    }

    return OPRT_OK;
}

/***********************************************************
 *               AI Server Initialization Functions
 ***********************************************************/
static OPERATE_RET __ai_agent_init(void *data)
{
    (void)data;

    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("%s...", __func__);

    AI_AGENT_CFG_T agent_cfg = {0};

    // Configure callback functions
    agent_cfg.output.alert_cb = __ai_agent_alert_cb;
    agent_cfg.output.text_cb = __ai_agent_text_cb;
    agent_cfg.output.media_data_cb = __ai_agent_media_data_cb;
    agent_cfg.output.media_attr_cb = __ai_agent_media_attr_cb;
    agent_cfg.output.event_cb = __ai_agent_event_cb;

    // Configure audio codec settings
    agent_cfg.codec_enable = TRUE;
    agent_cfg.attr.audio.codec_type = AUDIO_CODEC_PCM;
    agent_cfg.attr.audio.sample_rate = 16000;
    agent_cfg.attr.audio.channels = AUDIO_CHANNELS_MONO;
    agent_cfg.attr.audio.bit_depth = 16;

    // video
    // agent_cfg.attr.video.codec_type  = VIDEO_CODEC_H264;
    // agent_cfg.attr.video.sample_rate = 90000;
    // agent_cfg.attr.video.fps         = 20;
    // agent_cfg.attr.video.width       = 480;
    // agent_cfg.attr.video.height      = 480;

    // MCP
    agent_cfg.enable_mcp = TRUE;

    TUYA_CALL_ERR_RETURN(tuya_ai_agent_init(&agent_cfg));

    ai_monitor_config_t monitor_cfg = AI_MONITOR_CFG_DEFAULT;
    TUYA_CALL_ERR_RETURN(tuya_ai_monitor_init(&monitor_cfg));

    return rt;
}

/**
 * @brief Initializes the AI service module.
 * @param msg_cb Callback function for handling AI messages.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_init(AI_AGENT_CBS_T *cbs)
{
    OPERATE_RET rt = OPRT_OK;

    memset(&sg_ai, 0, sizeof(AI_AGENT_T));

    if (cbs) {
        memcpy(&sg_ai.cbs, cbs, sizeof(AI_AGENT_CBS_T));
    }

    PR_DEBUG("ai session wait for mqtt connected...");

    tal_event_subscribe(EVENT_MQTT_CONNECTED, "ai_agent_init", __ai_agent_init, SUBSCRIBE_TYPE_ONETIME);

    return rt;
}

/**
 * @brief Starts the AI audio upload process.
 * @param enable_vad Flag to enable cloud vad.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_start(uint8_t enable_vad)
{
    tuya_ai_agent_server_vad_ctrl(enable_vad);
    tuya_ai_agent_set_scode(AI_AGENT_SCODE_DEFAULT);
    tuya_ai_input_start(FALSE);

    return OPRT_OK;
}

/**
 * @brief Uploads audio data to the AI service.
 * @param data Pointer to the audio data buffer.
 * @param len Length of the audio data in bytes.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_data(uint8_t *data, uint32_t len)
{
    UINT64_T pts = 0;
    UINT64_T timestamp = 0;

    timestamp = pts = tal_system_get_millisecond();
    return tuya_ai_audio_input(timestamp, pts, data, len, len);
}

/**
 * @brief Stops the AI audio upload process.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_stop(void)
{
    PR_DEBUG("tuya ai upload stop...");
    tuya_ai_input_stop();
    return OPRT_OK;
}

/**
 * @brief Intrrupt the AI upload process.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_chat_intrrupt(void)
{
    return tuya_ai_agent_event(AI_EVENT_CHAT_BREAK, 0);
}

/**
 * @brief Send cloud alert to the AI service.
 * @param type Alert type
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_cloud_alert(int type)
{
    OPERATE_RET rt = OPRT_OK;
    PR_DEBUG("ai audio agent cloud alert, type: %d", type);
    TUYA_CALL_ERR_RETURN(tuya_ai_input_alert(type, NULL));
    return OPRT_OK;
}

OPERATE_RET ai_text_agent_upload_stop(void)
{
    tuya_ai_input_stop();

    return OPRT_OK;
}

OPERATE_RET ai_text_agent_upload(uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(data, OPRT_INVALID_PARM);
    if (len == 0) {
        PR_ERR("text data length is zero");
        return OPRT_INVALID_PARM;
    }

    tuya_ai_agent_set_scode(AI_AGENT_SCODE_DEFAULT);
    tuya_ai_input_start(FALSE);
    tuya_ai_text_input((BYTE_T *)data, len, len);
    tuya_ai_input_stop();

    return rt;
}