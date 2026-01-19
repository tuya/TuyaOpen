/**
 * @file tuya_ai_output.h
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

#ifndef __TUYA_AI_OUTPUT_H__
#define __TUYA_AI_OUTPUT_H__

#include "tuya_ai_types.h"

#include "tuya_cloud_types.h"
#include "tuya_ai_protocol.h"
#include "tuya_ai_biz.h"
#include "cJSON.h"

typedef enum {
    /* Power-on boot prompt */
    AT_POWER_ON,
    /* Not yet configured for network, please configure network first */
    AT_NOT_ACTIVE,
    /* Entering network configuration state, starting network configuration */
    AT_NETWORK_CFG,
    /* Network connection successful */
    AT_NETWORK_CONNECTED,
    /* Network connection failed, retrying */
    AT_NETWORK_FAIL,
    /* Network disconnected */
    AT_NETWORK_DISCONNECT,
    /* Low battery */
    AT_BATTERY_LOW,
    /* Please say again */
    AT_PLEASE_AGAIN,
    /* Long press key to talk */
    AT_LONG_KEY_TALK,
    /* Press key to talk */
    AT_KEY_TALK,
    /* Wake up to talk */
    AT_WAKEUP_TALK,
    /* Random talk */
    AT_RANDOM_TALK,
    /* Wake up response */
    AT_WAKEUP,
    /* Volume adjustment */
    AT_VOLUME,
    AT_MAX
} AI_ALERT_TYPE_E;

typedef enum {
    AI_OUTPUT_CBS_MODE_NORMAL = 0,
    AI_OUTPUT_CBS_MODE_MONITOR = AI_OUTPUT_CBS_MODE_NORMAL, // ext cbs callback does not affect default cbs
    AI_OUTPUT_CBS_MODE_FILTER,      // after ext cbs callback, default cbs will not be called
    AI_OUTPUT_CBS_MODE_MAX
} AI_OUTPUT_CBS_MODE_E;

typedef uint8_t AI_TEXT_TYPE_E;
#define AI_TEXT_ASR 0x00
#define AI_TEXT_NLG 0x01
#define AI_TEXT_SKILL 0x02
#define AI_TEXT_OTHER 0x03
#define AI_TEXT_CLOUD_EVENT 0x04

typedef struct {
    /** recv event */
    OPERATE_RET(*event_cb)(AI_EVENT_TYPE etype, AI_PACKET_PT ptype, AI_EVENT_ID eid);
    /** recv media attr */
    OPERATE_RET(*media_attr_cb)(AI_BIZ_ATTR_INFO_T *attr);
    /** recv media data */
    OPERATE_RET(*media_data_cb)(AI_PACKET_PT type, char *data, uint32_t len, uint32_t total_len);
    /** recv text stream */
    OPERATE_RET(*text_cb)(AI_TEXT_TYPE_E type, cJSON *root, bool eof);
    /** recv alert */
    OPERATE_RET(*alert_cb)(AI_ALERT_TYPE_E type);
} AI_OUTPUT_CBS_T;

/**
 * @brief ai output event
 *
 * @param[in] type event type
 * @param[in] ptype packet type
 * @param[in] eid event id
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_output_event(AI_EVENT_TYPE etype, AI_PACKET_PT ptype, AI_EVENT_ID eid);

/**
 * @brief ai output start
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_output_start(void);

/**
 * @brief ai output stop
 *
 * @param[in] force force stop
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_output_stop(bool force);

/**
 * @brief ai output write
 *
 * @param[in] type packet type
 * @param[in] data data buffer
 * @param[in] len data length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_output_write(AI_PACKET_PT type, uint8_t *data, uint32_t len);

/**
 * @brief is ai output palying
 *
 * @return TRUE if playing, FALSE if not
 */
bool tuya_ai_output_is_playing(void);

/**
 * @brief register external cbs with scode
 *
 * @param[in] scode solution code
 * @param[in] cbs callback functions
 * @param[in] mode callback mode
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 *
 * @note should call this interface after tuya_ai_output_init
 */
OPERATE_RET tuya_ai_output_register_cbs(char *scode, AI_OUTPUT_CBS_T *cbs, AI_OUTPUT_CBS_MODE_E mode);

/**
 * @brief unregister external cbs with scode
 *
 * @param[in] scode solution code
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 *
 */
OPERATE_RET tuya_ai_output_unregister_cbs(char *scode);

/**
 * @brief ai output attr
 *
 * @param[in] attr attribute
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_output_attr(char *scode, AI_BIZ_ATTR_INFO_T *attr);
#endif // __TUYA_AI_OUTPUT_H__
