/**
 * @file tal_queue.h
 * @brief Provides message queue management functions for Tuya IoT applications.
 *
 * This header file defines the interface for creating, posting to, fetching
 * from, and freeing message queues in Tuya IoT applications. Message queues
 * facilitate communication between different parts of an application or between
 * different tasks in a multitasking environment, allowing for asynchronous
 * message passing and synchronization. The functions provided in this file
 * abstract the underlying implementation details, offering a simple and
 * efficient way to manage message queues.
 *
 * The message queue management functions are essential for developing
 * responsive and modular IoT applications on the Tuya platform, enabling tasks
 * to communicate and synchronize their operations effectively.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_QUEUE_H__
#define __TAL_QUEUE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *QUEUE_HANDLE;

/**
 * @brief Create message queue
 *
 * @param[in] msgsize message size
 * @param[in] msgcount message number
 * @param[out] queue the queue handle created
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_queue_create_init(QUEUE_HANDLE *queue, int msgsize, int msgcount);

/**
 * @brief post a message to the message queue
 *
 * @param[in] queue the handle of the queue
 * @param[in] data the data of the message
 * @param[in] timeout timeout time
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_queue_post(QUEUE_HANDLE queue, void *data, uint32_t timeout);

/**
 * @brief fetch message from the message queue
 *
 * @param[in] queue the message queue handle
 * @param[inout] msg the message fetch form the message queue
 * @param[in] timeout timeout time
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_queue_fetch(QUEUE_HANDLE queue, void *msg, uint32_t timeout);

/**
 * @brief free the message queue
 *
 * @param[in] queue the message queue handle
 *
 * @return void
 */
void tal_queue_free(QUEUE_HANDLE queue);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
