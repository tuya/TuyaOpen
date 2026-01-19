/**
 * @file tuya_ai_client.h
 * @author tuya
 * @brief ai client
 * @version 0.1
 * @date 2025-03-02
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
#ifndef __TUYA_AI_CLIENT_H__
#define __TUYA_AI_CLIENT_H__

#include "tuya_ai_types.h"

#include "tuya_cloud_com_defs.h"
#include "tuya_cloud_types.h"
#include "tuya_ai_protocol.h"
#include "tuya_ai_mqtt.h"

#define EVENT_AI_CLIENT_RUN      "ai.client.run"
#define EVENT_AI_CLIENT_CLOSE    "ai.client.close"

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
VOID tuya_ai_client_reg_cb(AI_BASIC_DATA_HANDLE cb);

/**
 * @brief ai client init
 *
 * @param[in] cb mqtt recv callback
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_client_init(AI_MQTT_RECV_CB cb);

/**
 * @brief ai client deinit
 *
 */
VOID tuya_ai_client_deinit(VOID);

/**
 * @brief is ai client ready
 *
 * @return TRUE is ready, FALSE is not ready
 */
BOOL_T tuya_ai_client_is_ready(VOID);

/**
 * @brief start ai client ping
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
VOID tuya_ai_client_start_ping(VOID);

/**
 * @brief stop ai client ping
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
VOID tuya_ai_client_stop_ping(VOID);
#endif