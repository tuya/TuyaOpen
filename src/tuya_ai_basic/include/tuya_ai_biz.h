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

typedef OPERATE_RET (*AI_BIZ_SEND_GET_CB)(AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char **data);

typedef void (*AI_BIZ_SEND_FREE_CB)(char *data);

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
 * @brief Create a new AI session with the specified configuration
 *
 * @param[in] bizCode The business code identifying the type of AI session
 * @param[in] cfg Pointer to the AI session configuration structure
 * @param[in] attr Pointer to session attributes data (can be NULL)
 * @param[in] attr_len Length of the session attributes data
 * @param[out] id Output parameter for the generated session ID (must be pre-allocated)
 *
 * @return OPERATE_RET OPRT_OK on success, error code otherwise
 *
 * @note This function will:
 *       1. Generate a unique session ID
 *       2. Pack the session data
 *       3. Store the session in the session table
 *       4. Create a task if needed
 *
 * @warning The caller must ensure the id buffer has sufficient space (minimum UUID length)
 */
OPERATE_RET tuya_ai_biz_crt_session(uint32_t bizCode, AI_SESSION_CFG_T *cfg, uint8_t *attr, uint32_t attr_len,
                                    AI_SESSION_ID id);
/**
 * @brief Delete an existing AI session
 *
 * @param[in] id The session ID to delete
 * @param[in] code Status code indicating reason for deletion
 *
 * @return OPERATE_RET OPRT_OK on success, error code otherwise
 *
 * @note This is a wrapper for the internal session destruction function
 */
OPERATE_RET tuya_ai_biz_del_session(AI_SESSION_ID id, AI_STATUS_CODE code);

/**
 * @brief Send a business packet with specified attributes and payload
 *
 * @param[in] id The packet identifier
 * @param[in] attr Pointer to the attribute information structure (AI_BIZ_ATTR_INFO_T)
 * @param[in] type The packet type (AI_PACKET_PT enumeration)
 * @param[in] head Pointer to the packet header information (AI_BIZ_HEAD_INFO_T)
 * @param[in] payload Pointer to the payload data
 *
 * @return OPERATE_RET Returns OPRT_OK on success, error code otherwise
 *
 * @note This function handles different packet types (VIDEO/AUDIO/IMAGE/FILE/TEXT)
 *       by creating appropriate headers and managing memory allocation.
 *       It performs network byte order conversion (UNI_HTONS/UNI_HTONL/UNI_HTONLL)
 *       for cross-platform compatibility.
 *       Memory is allocated and freed for each packet type internally.
 */
OPERATE_RET tuya_ai_send_biz_pkt(uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_PACKET_PT type, AI_BIZ_HEAD_INFO_T *head,
                                 char *payload);

/**
 * @brief Initialize the AI business module by subscribing to client events
 *
 * @return OPERATE_RET Returns OPRT_OK on successful initialization
 *
 * @details This function performs the following initialization tasks:
 *          - Subscribes to AI client run event (EVENT_AI_CLIENT_RUN) with callback __ai_clt_run_evt
 *          - Subscribes to AI client close event (EVENT_AI_CLIENT_CLOSE) with callback __ai_clt_close_evt
 *          - Both subscriptions use normal priority (SUBSCRIBE_TYPE_NORMAL)
 *
 * @note The subscription names ("ai.biz") identify this module in event notifications.
 *       This initialization should be called once during system startup.
 *
 * @see EVENT_AI_CLIENT_RUN
 * @see EVENT_AI_CLIENT_CLOSE
 * @see SUBSCRIBE_TYPE_NORMAL
 */
OPERATE_RET tuya_ai_biz_init(void);

int tuya_ai_biz_get_send_id(void);

int tuya_ai_biz_get_recv_id(void);

#endif /* __TUYA_AI_BIZ_H__ */