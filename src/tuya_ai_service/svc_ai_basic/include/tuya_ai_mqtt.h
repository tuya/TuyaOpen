/**
 * @file tuya_ai_mqtt.h
 * @author tuya
 * @brief ai mqtt
 * @version 0.1
 * @date 2025-05-17
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
#ifndef __TUYA_AI_MQTT_H__
#define __TUYA_AI_MQTT_H__

#include "tuya_ai_types.h"
#include "ty_cJSON.h"

#include "tuya_cloud_com_defs.h"
#include "tuya_cloud_types.h"
#include "tuya_ai_protocol.h"

/**
 * @brief get server cfg info
 *
 * @param[out] cfg cfg info
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_mq_ser_cfg_req(VOID);

/**
 * @brief get server cfg info
 *
 * @return server cfg info
 */
AI_SERVER_CFG_INFO_T* tuya_ai_mq_ser_cfg_get(VOID);

/**
 * @brief get agent token info
 *
 * @param[in] solution_code solution code
 * @param[out] agent agent token info
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_mq_token_req(char *solution_code, AI_AGENT_TOKEN_INFO_T *agent);

/**
 * @brief recv mqtt 9000 protocol message
 *
 * @param[in] root json root
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET(*AI_MQTT_RECV_CB)(ty_cJSON *root);

/**
 * @brief mqtt init
 *
 * @param[in] cb callback function for receiving mqtt messages
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_mq_init(AI_MQTT_RECV_CB cb);

/**
 * @brief deinit mqtt
 *
 */
VOID tuya_ai_mq_deinit(VOID);
#endif