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

typedef OPERATE_RET (*AI_BASIC_DATA_HANDLE)(char *data, uint32_t len);

/**
 * @brief Register a callback function for AI client data handling
 *
 * @param[in] cb Callback function pointer of type AI_BASIC_DATA_HANDLE
 *
 * @note This function registers the callback that will be invoked when AI data is received.
 *       The callback will only be registered if the AI client is initialized.
 */
void tuya_ai_client_reg_cb(AI_BASIC_DATA_HANDLE cb);

/**
 * @brief Check if the AI client is ready and running
 *
 * @return uint8_t Returns true (1) if client is initialized and in RUNNING state,
 *                 false (0) otherwise
 */
uint8_t tuya_ai_client_is_ready(void);

/**
 * @brief Initialize the AI client subsystem
 *
 * @return OPERATE_RET OPRT_OK on success, error code otherwise
 *
 * @note This function performs the following initialization steps:
 *       1. Allocates memory for client structure
 *       2. Sets default heartbeat interval (30s)
 *       3. Configures reconnection timing parameters
 *       4. Initializes AI business layer
 *       5. Creates various timers and work queues
 *
 * @warning If initialization fails at any point, all resources will be cleaned up
 *          and an error code will be returned.
 *
 * @see AI_RECONN_TIME_T for reconnection timing structure
 */
OPERATE_RET tuya_ai_client_init(void);

#endif /* __TUYA_AI_CLIENT_H__ */