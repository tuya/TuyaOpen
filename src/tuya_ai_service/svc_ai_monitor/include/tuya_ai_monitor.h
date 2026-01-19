/**
 * @file tuya_ai_monitor.h
 * @author tuya
 * @brief TUYA AI monitor service
 * @version 0.1
 * @date 2025-06-09
 *
 * @copyright Copyright 2014-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_AI_MONITOR_H__
#define __TUYA_AI_MONITOR_H__

#include "tuya_ai_types.h"

#include "tuya_cloud_types.h"
#include "tuya_ai_biz.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AI_PT_CUSTOM_LOG  60        // Custom pt type for log messages

#define AI_EVENT_MONITOR_FILTER     0xF000  // Filter for AI event monitor type filtering
#define AI_EVENT_MONITOR_ALG_CTRL   0xF001  // Filter for AI event monitor algorithm control
#define AI_EVENT_MONITOR_INVALID    0xFFFF  // Invalid event monitor type

/**
 * @brief AI monitor message types
 */
typedef enum {
    AI_MSG_TYPE_PING            = 4,     // ping message
    AI_MSG_TYPE_PONG            = 5,     // pong message
    AI_MSG_TYPE_VIDEO_STREAM    = 30,    // video stream
    AI_MSG_TYPE_AUDIO_STREAM    = 31,    // audio stream
    AI_MSG_TYPE_IMAGE_STREAM    = 32,    // image stream
    AI_MSG_TYPE_FILE_STREAM     = 33,    // file stream
    AI_MSG_TYPE_TEXT_STREAM     = 34,    // text stream
    AI_MSG_TYPE_EVENT           = 35,    // event message
    AI_MSG_TYPE_ERROR           = 0xFF   // error message
} ai_monitor_msg_type_e;

/**
 * @brief AI monitor server configuration
 */
typedef struct {
    uint32_t port;                    // TCP server port
    uint32_t max_clients;             // maximum client connections
    uint32_t recv_buf_size;           // receive buffer size
    uint32_t send_buf_size;           // send buffer size
    uint32_t heartbeat_interval;      // heartbeat interval in seconds
    uint32_t heartbeat_timeout;       // heartbeat timeout in seconds
    BOOL_T enable_broadcast;        // enable broadcast to all clients
} ai_monitor_config_t;

#define AI_MONITOR_PORT_DEFAULT          5055
#define AI_MONITOR_MAX_CLIENTS_DEFAULT   3
#define AI_MONITOR_CFG_DEFAULT {\
    .port = AI_MONITOR_PORT_DEFAULT, \
    .max_clients = AI_MONITOR_MAX_CLIENTS_DEFAULT, \
    .recv_buf_size = 1024, \
    .send_buf_size = 1024, \
    .heartbeat_interval = 30, \
    .heartbeat_timeout = 60, \
    .enable_broadcast = TRUE \
}

/**
 * @brief initialize AI monitor TCP server
 *
 * @param[in] config server configuration
 * @param[in] callbacks callback functions
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_monitor_init(CONST ai_monitor_config_t *config);

/**
 * @brief deinitialize AI monitor TCP server
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_monitor_deinit(VOID);

/**
 * @brief check if server is running
 *
 * @return TRUE if running, FALSE otherwise
 */
BOOL_T tuya_ai_monitor_is_running(VOID);

/**
 * @brief broadcast message to all connected clients
 *
 * @param[in] id channel ID
 * @param[in] attr attribute information
 * @param[in] head header information
 * @param[in] data payload data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_monitor_broadcast(uint16_t id, AI_BIZ_ATTR_INFO_T *attr, AI_BIZ_HEAD_INFO_T *head, char *data);

/**
 * @brief broadcast text data to all connected clients
 *
 * @param[in] data text data to broadcast
 * @param[in] len length of the text data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_monitor_broadcast_text(char *data, uint32_t len);

/**
 * @brief broadcast log data to all connected clients
 * 
 * @param[in] data log data to broadcast
 * @param[in] len length of the log data
 * 
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_monitor_broadcast_log(char *data, uint32_t len);

/**
 * @brief broadcast audio data to all connected clients
 * 
 * @param[in] data_id ID for the audio data
 * @param[in] stype stream type(Start/Ing/End)
 * @param[in] data audio data to broadcast
 * @param[in] len length of the audio data
 * 
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_monitor_broadcast_audio(uint16_t data_id, AI_STREAM_TYPE stype, AI_AUDIO_CODEC_TYPE codec_type, char *data, uint32_t len);

OPERATE_RET tuya_ai_monitor_broadcast_audio_mic(AI_STREAM_TYPE stype, char *data, uint32_t len);

OPERATE_RET tuya_ai_monitor_broadcast_audio_ref(AI_STREAM_TYPE stype, char *data, uint32_t len);

OPERATE_RET tuya_ai_monitor_broadcast_audio_aec(AI_STREAM_TYPE stype, char *data, uint32_t len);

/**
 * @brief dump server status information
 *
 */
VOID tuya_ai_monitor_dump_status(VOID);

#ifdef __cplusplus
}
#endif
#endif //__TUYA_AI_MONITOR_H__