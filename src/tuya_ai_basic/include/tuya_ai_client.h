/**
 * @file tuya_ai_client.h
 * @brief This file contains the implementation of Tuya AI client core functionality,
 * including connection management, network communication and protocol handling.
 *
 * The Tuya AI client module provides the underlying communication framework for AI services,
 * handling network connections, data transmission and protocol processing. It implements
 * automatic reconnection mechanisms and ping-pong keepalive for stable connections.
 *
 * Key features include:
 * - Configurable reconnection attempts (AI_RECONN_TIME_NUM)
 * - Customizable ping timeout settings (AT_PING_TIMEOUT)
 * - Thread stack size configuration (AI_CLIENT_STACK_SIZE)
 * - Secure communication through tal_security integration
 * - Asynchronous task processing via work queue service
 * - Network state management with netmgr integration
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_AI_CLIENT_H__
#define __TUYA_AI_CLIENT_H__

#include "tuya_cloud_com_defs.h"
#include "tuya_cloud_types.h"
#include "tuya_ai_protocol.h"

#define EVENT_AI_CLIENT_RUN   "ai.client.run"
#define EVENT_AI_CLIENT_CLOSE "ai.client.close"

/**
 * @brief data handle cb
 *
 * @param[in] data data
 * @param[in] len data length
 * @param[in] frag data fragment flag
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET (*AI_BASIC_DATA_HANDLE)(char *data, uint32_t len, AI_FRAG_FLAG frag);

/**
 * @brief register data handle cb
 *
 * @param[in] cb data handle cb
 */
void tuya_ai_client_reg_cb(AI_BASIC_DATA_HANDLE cb);

/**
 * @brief ai client init
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_client_init(void);

/**
 * @brief is ai client ready
 *
 * @return true is ready, false is not ready
 */
uint8_t tuya_ai_client_is_ready(void);

/**
 * @brief start ai client ping
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
void tuya_ai_client_start_ping(void);

/**
 * @brief stop ai client ping
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
void tuya_ai_client_stop_ping(void);
#endif