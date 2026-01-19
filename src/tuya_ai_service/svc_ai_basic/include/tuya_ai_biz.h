/**
 * @file tuya_ai_biz.h
 * @author tuya
 * @brief ai protocol
 * @version 0.1
 * @date 2025-03-04
 *
 * @copyright Copyright (c) 2023 Tuya Inc. All Rights Reserved.
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
#ifndef __TUYA_AI_BIZ_H__
#define __TUYA_AI_BIZ_H__

#include "tuya_ai_types.h"

#include "tuya_cloud_com_defs.h"
#include "tuya_cloud_types.h"
#include "tuya_ai_protocol.h"
#include "tuya_ai_event.h"

#ifndef AI_BIZ_MAX_NUM
#define AI_BIZ_MAX_NUM 5
#endif

#define EVENT_AI_SESSION_NEW       "ai.session.new"
#define EVENT_AI_SESSION_CLOSE     "ai.session.close"

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
    /** session state change attr */
    AI_SESSION_STATE_ATTR_T state;
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
    uint64_t timestamp; //unit:ms
    /** pts */
    uint64_t pts; //unit:us
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

typedef uint8_t AI_BIZ_DATA_TYPE;
#define AI_BIZ_DATA_TYPE_BYTE 0
#define AI_BIZ_DATA_TYPE_JSON 1
typedef struct {
    /** stream type */
    AI_STREAM_TYPE stream_flag;
    /** head data */
    AI_BIZ_HD_T value;
    /** data type */
    AI_BIZ_DATA_TYPE data_type;
    /** data total length */
    uint32_t total_len;
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
typedef OPERATE_RET(*AI_BIZ_SEND_GET_CB)(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char **data);

/**
 * @brief send free
 *
 * @param[in] data data
 *
 */
typedef VOID(*AI_BIZ_SEND_FREE_CB)(char *data);

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
typedef OPERATE_RET(*AI_BIZ_RECV_CB)(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, VOID *data, VOID *usr_data);

/**
 * @brief monitor callback for biz data
 *
 * @param[in] id channel id
 * @param[in] attr attribute
 * @param[in] head data head
 * @param[in] payload data
 * @param[in] usr_data user data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET(*AI_BIZ_MONITOR_CB)(uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char *data, VOID *usr_data);

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
    /** recv packet type */
    AI_PACKET_PT type;
    /** recv channel id */
    uint16_t id;
    /** recv channel cb */
    AI_BIZ_RECV_CB cb;
    /** user data */
    VOID *usr_data;
} AI_BIZ_RECV_DATA_T;

typedef struct {
    /** send channel num */
    uint16_t send_num;
    /** send channel data */
    AI_BIZ_SEND_DATA_T send[AI_BIZ_MAX_NUM];
    /** recv channel num */
    uint16_t recv_num;
    /** recv channel data */
    AI_BIZ_RECV_DATA_T recv[AI_BIZ_MAX_NUM];
    /** event cb */
    AI_EVENT_CB event_cb;
} AI_SESSION_CFG_T;

/**
 * @brief create session
 *
 * @param[in] bizCode biz code
 * @param[in] bizTag biz tag
 * @param[in] cfg session cfg
 * @param[in] attr user attr
 * @param[in] attr_len user attr len
 * @param[in] token agent token
 * @param[out] id session id
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_biz_crt_session(uint32_t bizCode, uint64_t bizTag, AI_SESSION_CFG_T *cfg, uint8_t *attr, uint32_t attr_len, char *token, AI_SESSION_ID id);

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
OPERATE_RET tuya_ai_send_biz_pkt(uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_PACKET_PT type, AI_BIZ_HEAD_INFO_T *head, char *payload);

/**
 * @brief send ai biz packet with custom writer
 *
 * @param[in] id channel id
 * @param[in] attr attribute
 * @param[in] type packet type
 * @param[in] head data head
 * @param[in] payload data
 * @param[in] writer custom writer, can be NULL
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_send_biz_pkt_custom(uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_PACKET_PT type,
                                        AI_BIZ_HEAD_INFO_T *head, char *payload, AI_PACKET_WRITER_T *writer);

/**
 * @brief get send id
 *
 * @return send id
 */
int tuya_ai_biz_get_send_id(VOID);

/**
 * @brief get recv id
 *
 * @return recv id
 */
int tuya_ai_biz_get_recv_id(VOID);

/**
 * @brief init ai biz
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_biz_init(VOID);

/**
 * @brief deinit ai biz
 *
 */
VOID tuya_ai_biz_deinit(VOID);

/**
 * @brief get session cfg
 *
 * @param[in] id session id
 *
 * @return session cfg
 */
AI_SESSION_CFG_T* tuya_ai_biz_get_session_cfg(AI_SESSION_ID id);

/**
 * @brief parse video attribute
 *
 * @param[in] de_buf decrypted buffer
 * @param[in] attr_len length of the attribute
 * @param[out] video pointer to AI_VIDEO_ATTR_T structure to fill in
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_parse_video_attr(char *de_buf, uint32_t attr_len, AI_VIDEO_ATTR_T *video);

/**
 * @brief parse audio attribute
 *
 * @param[in] de_buf decrypted buffer
 * @param[in] attr_len length of the attribute
 * @param[out] audio pointer to AI_AUDIO_ATTR_T structure to fill in
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_parse_audio_attr(char *de_buf, uint32_t attr_len, AI_AUDIO_ATTR_T *audio);

/**
 * @brief parse image attribute
 *
 * @param[in] de_buf decrypted buffer
 * @param[in] attr_len length of the attribute
 * @param[out] image pointer to AI_IMAGE_ATTR_T structure to fill in
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_parse_image_attr(char *de_buf, uint32_t attr_len, AI_IMAGE_ATTR_T *image);

/**
 * @brief parse file attribute
 *
 * @param[in] de_buf decrypted buffer
 * @param[in] attr_len length of the attribute
 * @param[out] file pointer to AI_FILE_ATTR_T structure to fill in
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_parse_file_attr(char *de_buf, uint32_t attr_len, AI_FILE_ATTR_T *file);

/**
 * @brief parse text attribute
 *
 * @param[in] de_buf decrypted buffer
 * @param[in] attr_len length of the attribute
 * @param[out] text pointer to AI_TEXT_ATTR_T structure to fill in
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_parse_text_attr(char *de_buf, uint32_t attr_len, AI_TEXT_ATTR_T *text);

/**
 * @brief parse event attribute
 *
 * @param[in] de_buf decrypted buffer
 * @param[in] attr_len length of the attribute
 * @param[out] event pointer to AI_EVENT_ATTR_T structure to fill in
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_parse_event_attr(char *de_buf, uint32_t attr_len, AI_EVENT_ATTR_T *event);

/**
 * @brief set monitor callback
 *
 * @param[in] recv_cb receive callback
 * @param[in] send_cb send callback
 * @param[in] usr_data user data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_biz_monitor_register(AI_BIZ_MONITOR_CB recv_cb, AI_BIZ_MONITOR_CB send_cb, VOID *usr_data);

/**
 * @brief get reuse send id
 *
 * @param[in] type packet type
 *
 * @return send id
 */
int tuya_ai_biz_get_reuse_send_id(AI_PACKET_PT type);
#endif