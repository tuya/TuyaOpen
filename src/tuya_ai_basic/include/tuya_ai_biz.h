/**
 * @file tuya_ai_biz.h
 * @brief This file contains the implementation of Tuya AI business logic,
 * including AI session management, task scheduling and AI protocol processing.
 *
 * The Tuya AI business module provides core functionalities for AI session
 * lifecycle management, including session creation, configuration and resource
 * allocation. It implements the AI protocol handlers and manages concurrent
 * AI sessions with thread-safe operations.
 *
 * Key features include:
 * - AI session management with configurable maximum session limit
 * - Asynchronous task scheduling with configurable delay
 * - Thread-safe operations using mutex and event mechanisms
 * - Integration with Tuya AI client and protocol layers
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_AI_BIZ_H__
#define __TUYA_AI_BIZ_H__

#include "tuya_cloud_com_defs.h"
#include "tuya_cloud_types.h"
#include "tuya_ai_protocol.h"
#include "tuya_ai_event.h"

#ifndef AI_MAX_SESSION_ID_NUM
#define AI_MAX_SESSION_ID_NUM 5
#endif

#define EVENT_AI_SESSION_NEW   "ai.session.new"
#define EVENT_AI_SESSION_CLOSE "ai.session.close"

typedef union {
    /** video attr */
    AI_VIDEO_ATTR_T video;
    /** audio attr */
    AI_AUDIO_ATTR_T audio;
    /** image attr */
    AI_IMAGE_ATTR_T image;
    /** file attr */
    AI_FILE_ATTR_T file;
    /** text attr */
    AI_TEXT_ATTR_T text;
    /** event attr */
    AI_EVENT_ATTR_T event;
    /** session close attr */
    AI_SESSION_CLOSE_ATTR_T close;
} AI_BIZ_ATTR_VALUE_T;

typedef struct {
    /** attr flag */
    AI_ATTR_FLAG flag;
    /** packet type*/
    AI_PACKET_PT type;
    /** attr value*/
    AI_BIZ_ATTR_VALUE_T value;
} AI_BIZ_ATTR_INFO_T;

typedef struct {
    /** timestamp */
    uint64_t timestamp; // unit:ms
    /** pts */
    uint64_t pts; // unit:us
} AI_VIDEO_BIZ_HEAD_T, AI_AUDIO_BIZ_HEAD_T;

typedef struct {
    /** timestamp */
    uint64_t timestamp;
} AI_IMAGE_BIZ_HEAD_T;

typedef union {
    /** video head */
    AI_VIDEO_BIZ_HEAD_T video;
    /** audio head */
    AI_AUDIO_BIZ_HEAD_T audio;
    /** image head */
    AI_IMAGE_BIZ_HEAD_T image;
} AI_BIZ_HD_T;

typedef struct {
    /** stream type */
    AI_STREAM_TYPE stream_flag;
    /** head data */
    AI_BIZ_HD_T value;
    /** data length */
    uint32_t len;
} AI_BIZ_HEAD_INFO_T;

/**
 * @brief get biz data to send
 *
 * @param[out] attr attribute
 * @param[out] head data head
 * @param[out] data data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET (*AI_BIZ_SEND_GET_CB)(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char **data);

/**
 * @brief send free
 *
 * @param[in] data data
 *
 */
typedef void (*AI_BIZ_SEND_FREE_CB)(char *data);

/**
 * @brief recv biz data
 *
 * @param[in] attr attribute
 * @param[in] head data head
 * @param[in] data data
 * @param[in] usr_data user data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET (*AI_BIZ_RECV_CB)(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char *data, void *usr_data);

typedef struct {
    /** send packet type */
    AI_PACKET_PT type;
    /** send channel id */
    uint16_t id;
    /** send channel get cb */
    AI_BIZ_SEND_GET_CB get_cb;
    /** send channel free cb */
    AI_BIZ_SEND_FREE_CB free_cb;
} AI_BIZ_SEND_DATA_T;

typedef struct {
    /** recv channel id */
    uint16_t id;
    /** recv channel cb */
    AI_BIZ_RECV_CB cb;
    /** user data */
    void *usr_data;
} AI_BIZ_RECV_DATA_T;

typedef struct {
    /** send channel num */
    uint16_t send_num;
    /** send channel data */
    AI_BIZ_SEND_DATA_T send[AI_MAX_SESSION_ID_NUM];
    /** recv channel num */
    uint16_t recv_num;
    /** recv channel data */
    AI_BIZ_RECV_DATA_T recv[AI_MAX_SESSION_ID_NUM];
    /** event cb */
    AI_EVENT_CB event_cb;
} AI_SESSION_CFG_T;

/**
 * @brief create session
 *
 * @param[in] bizCode biz code
 * @param[in] cfg session cfg
 * @param[in] attr user attr
 * @param[in] attr_len user attr len
 * @param[out] id session id
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_biz_crt_session(uint32_t bizCode, AI_SESSION_CFG_T *cfg, uint8_t *attr, uint32_t attr_len,
                                    AI_SESSION_ID id);

/**
 * @brief delete session
 *
 * @param[in] id session id
 * @param[in] code close code
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_biz_del_session(AI_SESSION_ID id, AI_STATUS_CODE code);

/**
 * @brief send ai biz packet
 *
 * @param[in] id channel id
 * @param[in] attr attribute
 * @param[in] type packet type
 * @param[in] head data head
 * @param[in] payload data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_send_biz_pkt(uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_PACKET_PT type, AI_BIZ_HEAD_INFO_T *head,
                                 char *payload);

/**
 * @brief get send id
 *
 * @return send id
 */
int tuya_ai_biz_get_send_id(void);

/**
 * @brief get recv id
 *
 * @return recv id
 */
int tuya_ai_biz_get_recv_id(void);

/**
 * @brief init ai biz
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_biz_init(void);
#endif