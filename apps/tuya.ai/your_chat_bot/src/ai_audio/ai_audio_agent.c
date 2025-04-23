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
#include "tkl_memory.h"
#include "tal_api.h"

#include "tuya_ai_biz.h"
#include "tuya_ai_protocol.h"
#include "tuya_ai_client.h"
#include "tuya_ai_event.h"

#include "ai_audio.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define AI_AGENT_NLG_TEXT_MAX_LEN (4 * 1024)

#define TY_BIZCODE_AI_CHAT     0x00010001 // 聊天场景可支持打断
#define TY_AI_CHAT_ID_DS_CNT   4
#define TY_AI_CHAT_ID_DS_AUDIO 1
#define TY_AI_CHAT_ID_DS_VIDEO 3
#define TY_AI_CHAT_ID_DS_TEXT  5
#define TY_AI_CHAT_ID_DS_IMAGE 7
#define TY_AI_CHAT_ID_US_CNT   2
#define TY_AI_CHAT_ID_US_AUDIO 2
#define TY_AI_CHAT_ID_US_TEXT  4

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t is_online;
    char session_id[AI_UUID_V4_LEN];
    char event_id[AI_UUID_V4_LEN];
    char stream_event_id[AI_UUID_V4_LEN];
    AI_AGENT_MSG_CB msg_cb;

    char *nlg_text;
    uint32_t nlg_text_offset;
} AI_AGENT_SESSION_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_AGENT_SESSION_T sg_ai = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __ai_agent_audio_recv(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char *data, void *usr_data)
{

    if (!head || !attr || !data) {
        PR_ERR("invalid param");
        return OPRT_COM_ERROR;
    }

    if (NULL == sg_ai.msg_cb) {
        PR_ERR("msg_cb is NULL");
        return OPRT_COM_ERROR;
    }

    uint32_t len = head->len;

    switch (head->stream_flag) {
    case AI_STREAM_START: {
        AI_AGENT_MSG_T ai_msg = {
            .type = AI_AGENT_MSG_TP_AUDIO_START,
            .data_len = len,
            .data = (uint8_t *)data,
        };
        sg_ai.msg_cb(&ai_msg);
    } break;
    case AI_STREAM_ING: {
        AI_AGENT_MSG_T ai_msg = {
            .type = AI_AGENT_MSG_TP_AUDIO_DATA,
            .data_len = len,
            .data = (uint8_t *)data,
        };
        sg_ai.msg_cb(&ai_msg);
    } break;
    case AI_STREAM_END: {
        AI_AGENT_MSG_T ai_msg = {
            .type = AI_AGENT_MSG_TP_AUDIO_STOP,
            .data_len = len,
            .data = (uint8_t *)data,
        };
        sg_ai.msg_cb(&ai_msg);
    } break;
    default:
        PR_ERR("unknown stream flag: %d", head->stream_flag);
        break;
    }

    return OPRT_OK;
}

static OPERATE_RET __ai_agent_txt_recv(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char *data, void *usr_data)
{
    cJSON *json, *node;

    if ((json = cJSON_Parse(data)) == NULL) {
        return OPRT_OK;
    }

    // PR_DEBUG("content: %s", data);

    // parse bizType
    node = cJSON_GetObjectItem(json, "bizType");
    const char *bizType = cJSON_GetStringValue(node);
    // parse eof
    node = cJSON_GetObjectItem(json, "eof");
    uint8_t eof = cJSON_GetNumberValue(node);
    // PR_DEBUG("bizType: %s, eof: %d", bizType, eof);

    if (eof && strcmp(bizType, "ASR") == 0) {
        // parse data.text
        node = cJSON_GetObjectItem(json, "data");
        node = cJSON_GetObjectItem(node, "text");
        const char *text = cJSON_GetStringValue(node);
        PR_DEBUG("ASR text: %s", text);

        AI_AGENT_MSG_T ai_msg = {0};
        ai_msg.type = AI_AGENT_MSG_TP_TEXT_ASR;

        if (text == NULL || text[0] == '\0') {
            PR_DEBUG("ASR empty");
            ai_msg.data = NULL;
            ai_msg.data_len = 0;
        } else {
            ai_msg.data = (uint8_t *)text;
            ai_msg.data_len = strlen(text);
        }
        sg_ai.msg_cb(&ai_msg);
    } else if (strcmp(bizType, "NLG") == 0) {
        node = cJSON_GetObjectItem(json, "data");
        node = cJSON_GetObjectItem(node, "content");
        const char *content = cJSON_GetStringValue(node);
        // PR_DEBUG("NLG eof: %d, content: %s", eof, content);

        if (NULL == sg_ai.nlg_text) {
            sg_ai.nlg_text = (char *)tkl_system_psram_malloc(AI_AGENT_NLG_TEXT_MAX_LEN);
            if (NULL == sg_ai.nlg_text) {
                PR_ERR("malloc nlg text failed");
            } else {
                memset(sg_ai.nlg_text, 0, AI_AGENT_NLG_TEXT_MAX_LEN);
                sg_ai.nlg_text_offset = 0;
            }
        }

        if (NULL != sg_ai.nlg_text && content != NULL) {
            uint32_t len = strlen(content);
            if (sg_ai.nlg_text_offset + len < AI_AGENT_NLG_TEXT_MAX_LEN - 1) { // -1 for '\0'
                memcpy(sg_ai.nlg_text + sg_ai.nlg_text_offset, content, len);
                sg_ai.nlg_text_offset += len;
            } else {
                PR_ERR("nlg text overflow");
            }
        }
        if (eof) {
            // send nlg text
            if (sg_ai.nlg_text != NULL && sg_ai.nlg_text_offset > 0) {
                PR_DEBUG("NLG text: %s", sg_ai.nlg_text);

                AI_AGENT_MSG_T ai_msg = {
                    .type = AI_AGENT_MSG_TP_TEXT_NLG,
                    .data_len = sg_ai.nlg_text_offset,
                    .data = (uint8_t *)sg_ai.nlg_text,
                };
                sg_ai.msg_cb(&ai_msg);

                tkl_system_psram_free(sg_ai.nlg_text);
                sg_ai.nlg_text = NULL;
                sg_ai.nlg_text_offset = 0;
            }
        }

    } else if (eof && strcmp(bizType, "SKILL") == 0) {
        AI_AUDIO_EMOTION_T ai_emotion = {
            .name = NULL,
            .text = NULL,
        };

        // {"bizId":"xxx","bizType":"SKILL","eof":1,"data":{"code":"emo","skillContent":{"emotion":["NEUTRAL"],"text":["😐"]}}}
        node = cJSON_GetObjectItem(json, "data");
        cJSON *skillContent = cJSON_GetObjectItem(node, "skillContent");
        cJSON *emotion = cJSON_GetObjectItem(skillContent, "emotion");
        cJSON *emotion_name = cJSON_GetArrayItem(emotion, 0);
        if (NULL == emotion_name) {
            PR_ERR("emotion is NULL");
            ai_emotion.name = NULL;
        } else {
            PR_DEBUG("emotion name: %s", emotion_name->valuestring);
            ai_emotion.name = emotion_name->valuestring;
        }

        cJSON *emo_text = cJSON_GetObjectItem(skillContent, "text");
        emo_text = cJSON_GetArrayItem(emo_text, 0);
        if (NULL == emo_text) {
            PR_ERR("emo text is NULL");
            ai_emotion.text = NULL;
        } else {
            PR_DEBUG("emo text: %s", emo_text->valuestring);
            ai_emotion.text = emo_text->valuestring;
        }

        AI_AGENT_MSG_T ai_msg = {
            .type = AI_AGENT_MSG_TP_EMOTION,
            .data_len = sizeof(AI_AUDIO_EMOTION_T),
            .data = (uint8_t *)&ai_emotion,
        };
        sg_ai.msg_cb(&ai_msg);
    }

    cJSON_Delete(json);
    return OPRT_OK;
}

static OPERATE_RET __ai_agent_event_recv(AI_EVENT_TYPE type, AI_SESSION_ID session_id, AI_EVENT_ID event_id,
                                         uint8_t *attr, uint32_t attr_len)
{
    PR_DEBUG("recv event type:%d, session_id:%s, event_id:%s, attr: %s", type, session_id, event_id, attr);

    // event type: 0-chat start, 1-chat stop, 2-data finish
    if (type == 0) {
        // update stream event id
        strncpy(sg_ai.event_id, event_id, AI_UUID_V4_LEN);
        // s_stream_status = TUYA_VOICE_STREAM_START;
    } else if (type == 1) {
        // clear stream event id
        memset(sg_ai.event_id, 0, AI_UUID_V4_LEN);
    } else if (type == 2) {
        // stream end
        memset(sg_ai.event_id, 0, AI_UUID_V4_LEN);
    }

    return OPRT_OK;
}

static OPERATE_RET __ai_agent_session_create(void)
{
    OPERATE_RET rt = OPRT_OK;

    AI_SESSION_CFG_T cfg;

    memset(&cfg, 0, sizeof(AI_SESSION_CFG_T));
    cfg.send_num = TY_AI_CHAT_ID_DS_CNT;
    cfg.send[0].type = AI_PT_AUDIO;
    cfg.send[0].id = tuya_ai_biz_get_send_id();
    cfg.send[0].get_cb = NULL;
    cfg.send[0].free_cb = NULL;
    cfg.send[1].type = AI_PT_VIDEO;
    cfg.send[1].id = tuya_ai_biz_get_send_id();
    cfg.send[1].get_cb = NULL;
    cfg.send[1].free_cb = NULL;
    cfg.send[2].type = AI_PT_TEXT;
    cfg.send[2].id = tuya_ai_biz_get_send_id();
    cfg.send[2].get_cb = NULL;
    cfg.send[2].free_cb = NULL;
    cfg.send[3].type = AI_PT_IMAGE;
    cfg.send[3].id = tuya_ai_biz_get_send_id();
    cfg.send[3].get_cb = NULL;
    cfg.send[3].free_cb = NULL;

    cfg.recv_num = TY_AI_CHAT_ID_US_CNT;
    cfg.recv[1].id = tuya_ai_biz_get_recv_id();
    cfg.recv[1].cb = __ai_agent_audio_recv;
    cfg.recv[1].usr_data = NULL;
    cfg.recv[0].id = tuya_ai_biz_get_recv_id();
    cfg.recv[0].cb = __ai_agent_txt_recv;
    cfg.recv[0].usr_data = NULL;

    cfg.event_cb = __ai_agent_event_recv;

    // 支持的tts格式
    char attr_tts_order[128] = {0};
    snprintf(attr_tts_order, 128,
             "{\"tts.order.supports\":[{\"format\":\"mp3\",\"container\":\"\",\"sampleRate\":16000,\"bitDepth\":\"16\","
             "\"channels\":1}]}");

    AI_ATTRIBUTE_T attr[2] = {{
                                  .type = 1003,
                                  .payload_type = ATTR_PT_U8,
                                  .length = 1,
                                  .value.u8 = 2 // 2表示设备
                              },
                              {
                                  .type = 1004,
                                  .payload_type = ATTR_PT_STR,
                                  .length = strlen(attr_tts_order),
                                  .value.str = attr_tts_order, // tts.order.supports
                              }};
    uint8_t *out = NULL;
    uint32_t out_len = 0;
    tuya_pack_user_attrs(attr, CNTSOF(attr), &out, &out_len);

    memset(sg_ai.session_id, 0, AI_UUID_V4_LEN);
    rt = tuya_ai_biz_crt_session(TY_BIZCODE_AI_CHAT, &cfg, out, out_len, sg_ai.session_id);
    tal_free(out);
    if (rt) {
        PR_ERR("create session failed, rt:%d", rt);
        return rt;
    }
    PR_DEBUG("create session id:%s", sg_ai.session_id);
    return OPRT_OK;
}

static int __ai_agent_session_new(void *data)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("ai session is ready...");

    TUYA_CALL_ERR_RETURN(__ai_agent_session_create());

    sg_ai.is_online = TRUE;

    return OPRT_OK;
}

static int __ai_agent_session_close(void *data)
{
    PR_DEBUG("ai session close...session id = %s", sg_ai.session_id);

    sg_ai.is_online = FALSE;

    return OPRT_OK;
}

static OPERATE_RET __ai_agent_init(void *data)
{
    PR_DEBUG("%s...", __func__);

#if defined(AI_AUDIO_DEBUG) && (AI_AUDIO_DEBUG == 1)
    ai_audio_debug_init();
#endif

    tal_event_subscribe(EVENT_AI_SESSION_NEW, "ai_session_new", __ai_agent_session_new, SUBSCRIBE_TYPE_NORMAL);
    tal_event_subscribe(EVENT_AI_SESSION_CLOSE, "ai_session_close", __ai_agent_session_close, SUBSCRIBE_TYPE_NORMAL);

    return tuya_ai_client_init();
}

/**
 * @brief Initializes the AI service module.
 * @param msg_cb Callback function for handling AI messages.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_init(AI_AGENT_MSG_CB msg_cb)
{
    OPERATE_RET rt = OPRT_OK;

    memset(&sg_ai, 0, sizeof(AI_AGENT_SESSION_T));

    if (NULL == msg_cb) {
        PR_ERR("msg_cb is NULL");
        return OPRT_INVALID_PARM;
    }
    sg_ai.msg_cb = msg_cb;

    PR_DEBUG("ai session wait for mqtt connected...");

    tal_event_subscribe(EVENT_MQTT_CONNECTED, "ai_agent_init", __ai_agent_init, SUBSCRIBE_TYPE_ONETIME);

    return rt;
}

/**
 * @brief Starts the AI upload process.
 * @param int_enable_interrupt Flag to enable interrupt processing.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_start(uint8_t int_enable_interrupt)
{
    OPERATE_RET rt = OPRT_OK;

    if (FALSE == sg_ai.is_online) {
        PR_ERR("ai is not online");
        return OPRT_COM_ERROR;
    }

#if defined(AI_AUDIO_DEBUG) && (AI_AUDIO_DEBUG == 1)
    ai_audio_debug_start();
#endif

    PR_DEBUG("tuya ai upload start...");

    // send start event
    memset(sg_ai.event_id, 0, AI_UUID_V4_LEN);

    char attr_asr_enable_vad[64] = {0};
    if (int_enable_interrupt) {
        snprintf(attr_asr_enable_vad, sizeof(attr_asr_enable_vad),
                 "{\"asr.enableVad\":true,\"processing.interrupt\":true}");
    } else {
        snprintf(attr_asr_enable_vad, sizeof(attr_asr_enable_vad), "{\"asr.enableVad\":true}");
    }

    AI_ATTRIBUTE_T attr[] = {{
        .type = 1003,
        .payload_type = ATTR_PT_STR,
        .length = strlen(attr_asr_enable_vad),
        .value.str = attr_asr_enable_vad, // tts.order.supports
    }};
    uint8_t *out = NULL;
    uint32_t out_len = 0;
    tuya_pack_user_attrs(attr, CNTSOF(attr), &out, &out_len);
    rt = tuya_ai_event_start(sg_ai.session_id, sg_ai.event_id, out, out_len);
    tal_free(out);
    if (rt) {
        PR_ERR("start event failed, rt:%d", rt);
        return rt;
    }
    PR_DEBUG("upload start event_id:%s", sg_ai.event_id);

    return rt;
}
/**
 * @brief Uploads audio data to the AI service.
 * @param is_first Flag indicating if this is the first chunk of data.
 * @param data Pointer to the audio data buffer.
 * @param len Length of the audio data in bytes.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_data(uint8_t is_first, uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == data || 0 == len) {
        PR_ERR("invalid data");
        return OPRT_INVALID_PARM;
    }

    if (FALSE == sg_ai.is_online) {
        PR_ERR("ai is not online");
        return OPRT_COM_ERROR;
    }

#if defined(AI_AUDIO_DEBUG) && (AI_AUDIO_DEBUG == 1)
    ai_audio_debug_data((char *)data, len);
#endif

    // send data use tuya_ai_send_biz_pkt, pcm
    AI_BIZ_ATTR_INFO_T attr = {
        .flag = AI_HAS_ATTR,
        .type = AI_PT_AUDIO,
        .value.audio =
            {
                .base.codec_type = AUDIO_CODEC_PCM,
                .base.sample_rate = 16000,
                .base.channels = AUDIO_CHANNELS_MONO,
                .base.bit_depth = 16,
                .option.user_len = 0,
                .option.user_data = NULL,
                .option.session_id_list = NULL,
            },
    };
    AI_BIZ_HEAD_INFO_T head = {
        .value.audio =
            {
                .timestamp = tal_system_get_millisecond(),
                .pts = 0,
            },
        .len = len,
    };

    if (is_first) {
        head.stream_flag = AI_STREAM_START;
    } else {
        head.stream_flag = AI_STREAM_ING;
    }

    PR_DEBUG("tuya ai upload data[%d][%d]...", head.stream_flag, len);

    TUYA_CALL_ERR_RETURN(tuya_ai_send_biz_pkt(TY_AI_CHAT_ID_DS_AUDIO, &attr, AI_PT_AUDIO, &head, (char *)data));

    return rt;
}

/**
 * @brief Stops the AI upload process.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_stop(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (FALSE == sg_ai.is_online) {
        PR_ERR("ai is not online");
        return OPRT_COM_ERROR;
    }

    PR_DEBUG("tuya ai upload stop...");

#if defined(AI_AUDIO_DEBUG) && (AI_AUDIO_DEBUG == 1)
    ai_audio_debug_stop();
#endif

    AI_BIZ_ATTR_INFO_T biz_attr = {
        .flag = AI_HAS_ATTR,
        .type = AI_PT_AUDIO,
        .value.audio =
            {
                .base.codec_type = AUDIO_CODEC_PCM,
                .base.sample_rate = 16000,
                .base.channels = AUDIO_CHANNELS_MONO,
                .base.bit_depth = 16,
                .option.user_len = 0,
                .option.user_data = NULL,
                .option.session_id_list = NULL,
            },
    };

    AI_BIZ_HEAD_INFO_T biz_head = {
        .value.audio =
            {
                .timestamp = tal_system_get_millisecond(),
                .pts = 0,
            },
        .len = 0,
    };
    biz_head.stream_flag = AI_STREAM_END;

    TUYA_CALL_ERR_RETURN(tuya_ai_send_biz_pkt(TY_AI_CHAT_ID_DS_AUDIO, &biz_attr, AI_PT_AUDIO, &biz_head, NULL));

    AI_ATTRIBUTE_T attr[] = {{
        .type = 1002,
        .payload_type = ATTR_PT_U16,
        .length = 2,
        .value.u16 = TY_AI_CHAT_ID_DS_AUDIO,
    }};
    uint8_t *out = NULL;
    uint32_t out_len = 0;
    tuya_pack_user_attrs(attr, CNTSOF(attr), &out, &out_len);
    rt = tuya_ai_event_payloads_end(sg_ai.session_id, sg_ai.event_id, out, out_len);
    tal_free(out);
    if (rt != OPRT_OK) {
        PR_ERR("upload stop failed, rt:%d", rt);
        return rt;
    }

    TUYA_CALL_ERR_RETURN(tuya_ai_event_end(sg_ai.session_id, sg_ai.event_id, NULL, 0));

    return rt;
}

/**
 * @brief Intrrupt the AI upload process.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_agent_upload_intrrupt(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tuya_ai_event_chat_break(sg_ai.session_id, sg_ai.event_id, NULL, 0));

    return rt;
}
