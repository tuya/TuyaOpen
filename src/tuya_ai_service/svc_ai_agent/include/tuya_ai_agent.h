/**
 * @file tuya_ai_agent.h
 * @brief
 * @version 0.1
 * @date 2025-04-17
 *
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */

#ifndef __TUYA_AI_AGENT_H__
#define __TUYA_AI_AGENT_H__

#include "tuya_ai_types.h"

#include "tuya_cloud_types.h"
#include "tuya_ai_client.h"
#include "tuya_ai_biz.h"
#include "tuya_ai_event.h"
#include "tuya_ai_input.h"
#include "tuya_ai_output.h"
#if defined(ENABLE_JOYINSIDE) && (ENABLE_JOYINSIDE == 1)
#include "joyinside_client.h"
#include "joyinside_biz.h"
#endif

#define AI_AGENT_SCODE_DEFAULT ""
#define AI_AGENT_SCODE_ALERT "device_alert"
#define AI_AGENT_SCODE_MULTIMODAL "multimodal_agent"

/* MCP tool callback */
typedef OPERATE_RET (*TY_AI_MCP_CB)(CONST ty_cJSON *json, VOID *user_data);

typedef union {
    /** video attr */
    AI_VIDEO_ATTR_BASE_T video;
    /** audio attr */
    AI_AUDIO_ATTR_BASE_T audio;
    /** image attr */
    AI_IMAGE_ATTR_BASE_T image;
    /** file attr */
    AI_FILE_ATTR_BASE_T file;
} AI_ATTR_BIZ_T;

typedef struct {
    uint32_t bit_rate;          // bit rate in bps, default is 16000, set 48000 or 64000 to reduce latency in poor network conditions
    uint32_t sample_rate;       // sample rate in Hz, default is 16000
    CONST char *format;       // default is "mp3", set to "opus" if your player supports opus decoding
} AI_AGENT_TTS_CFG_T;

typedef struct {
    AI_PACKET_PT type;
    uint16_t id;
    BOOL_T first_pkt;
} AI_AGENT_ID_T;
typedef struct {
    char scode[AI_SOLUTION_CODE_LEN];
    char sid[AI_UUID_V4_LEN + 1];
    char eid[AI_UUID_V4_LEN + 1];
    AI_AGENT_ID_T send[AI_BIZ_MAX_NUM];
    char token[AI_AGENT_TOKEN_LEN];
    BOOL_T is_active;
} AI_AGENT_SESSION_T;

typedef struct {
    AI_ATTR_BASE_T attr;
    char scode[AI_SOLUTION_CODE_LEN];
    AI_INPUT_SEND_T biz_get[AI_BIZ_MAX_NUM];
    AI_OUTPUT_CBS_T output;
    BOOL_T codec_enable; // whether to enable codec to encode audio before upload
    AI_AGENT_TTS_CFG_T tts_cfg;
    BOOL_T enable_crt_session_ext; // enable crt session external
    BOOL_T enable_internal_scode; // enable internal solution code
    BOOL_T enable_mcp; // enable mcp tools
#if defined(ENABLE_JOYINSIDE) && (ENABLE_JOYINSIDE == 1)
    JD_CHAT_CFG_S *jd_cfg; // joyinside chat config
#endif
} AI_AGENT_CFG_T;

/**
 * @brief ai agent init
 *
 * @param[in] cfg agent cfg
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_agent_init(AI_AGENT_CFG_T *cfg);

/**
 * @brief ai agent deinit
 *
 */
VOID tuya_ai_agent_deinit(VOID);

/**
 * @brief ai agent event
 *
 * @param[in] etype event type
 * @param[in] ptype packet type
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_agent_event(AI_EVENT_TYPE etype, AI_PACKET_PT ptype);

/**
 * @brief ai agent set attribute
 *
 * @param[in] ptype packet type
 * @param[in] attr attribute
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
VOID tuya_ai_agent_set_attr(AI_PACKET_PT ptype, AI_ATTR_BIZ_T *attr);

/**
 * @brief ai agent set tts config
 *
 * @param[in] cfg config for tts
 *
 * @return VOID
 */
VOID tuya_ai_agent_set_tts_cfg(AI_AGENT_TTS_CFG_T *cfg);

/**
 * @brief ai agent create session
 *
 * @param[in] scode solution code
 * @param[in] bizCode business code
 * @param[in] bizTag business tag
 * @param[in] attr user attribute data
 * @param[in] attr_len user attribute data length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_agent_crt_session(char *scode, uint32_t bizCode, uint64_t bizTag, uint8_t *attr, uint32_t attr_len);

/**
 * @brief ai agent del session
 *
 * @param[in] scode solution code
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_agent_del_session(char *scode);

/**
 * @brief ai agent set solution code
 *
 * @param[in] scode solution code
 *
 */
VOID tuya_ai_agent_set_scode(char *scode);

/**
 * @brief ai agent get active solution code
 *
 * @return active solution code or default solution code
 */
char *tuya_ai_agent_get_active_scode(VOID);

/**
 * @brief ai agent set event id
 *
 * @param[in] eid event id
 *
 */
VOID tuya_ai_agent_set_eid(AI_EVENT_ID eid);

/**
 * @brief ai agent get event id
 *
 * @return event id
 */
AI_EVENT_ID tuya_ai_agent_get_eid(VOID);

/**
 * @brief get recv callback by packet type
 *
 * @param[in] type packet type
 *
 * @return recv callback
 */
AI_BIZ_RECV_CB tuya_ai_agent_get_recv_cb(AI_PACKET_PT type);

/**
 * @brief set send callback
 *
 * @param[in] in_send send callback
 *
 * @return VOID
 */
VOID tuya_ai_agent_send_cb_set(AI_INPUT_SEND_T *in_send);

/**
 * @brief is internal solution code
 *
 * @return TRUE: internal solution code; FALSE: user solution code
 */
BOOL_T tuya_ai_agent_is_internal_scode(VOID);

/**
 * @brief control internal solution code
 *
 * @param[in] flag TRUE: enable internal solution code; FALSE: disable internal solution code
 *
 * @return VOID
 */
void tuya_ai_agent_internal_scode_ctrl(BOOL_T flag);

/**
 * @brief get session by solution code
 *
 * @param[in] scode solution code
 *
 * @return session pointer, NULL if not found
 */
AI_AGENT_SESSION_T* tuya_ai_agent_get_session(char *scode);

/**
 * @brief server vad control
 *
 * @param[in] flag TRUE: enable server vad; FALSE: disable server vad
 *
 */
VOID tuya_ai_agent_server_vad_ctrl(BOOL_T flag);

/**
 * @brief get down attribute
 *
 * @return down attribute pointer
 */
AI_ATTR_BASE_T *tuya_ai_agent_get_down_attr(VOID);

/**
 * @brief ai agent mcp set callback
 *
 * @param[in] cb callback function
 * @param[in] user_data user data for callback
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_agent_mcp_set_cb(TY_AI_MCP_CB cb, VOID *user_data);

/**
 * @brief ai agent mcp response
 *
 * @param[in] message response message
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_agent_mcp_response(char *message);

/**
 * @brief get event callback
 *
 * @return event callback
 */
AI_EVENT_CB tuya_ai_agent_get_evt_cb(void);

/**
 * @brief is ai agent ready
 *
 * @return TRUE is ready, FALSE is not ready
 */
BOOL_T tuya_ai_agent_is_ready(void);
#endif // __TUYA_AI_AGENT_H__