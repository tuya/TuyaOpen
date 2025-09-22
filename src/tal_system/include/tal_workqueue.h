/**
 * @file tal_workqueue.h
 * @brief Provides work queue management functions for Tuya IoT applications.
 *
 * This header file defines the interface for managing work queues in Tuya IoT
 * applications, facilitating the scheduling, execution, and cancellation of
 * tasks. It supports both immediate and delayed task execution, with the
 * ability to specify one-time or cyclic tasks. The API abstracts the complexity
 * of task management, providing a simplified and efficient mechanism for
 * executing background tasks and time-sensitive operations within Tuya-based
 * IoT applications.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_WORKQUEUE_H__
#define __TAL_WORKQUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tal_thread.h"

typedef enum { LOOP_ONCE, LOOP_CYCLE } LOOP_TYPE;

typedef void *WORKQUEUE_HANDLE;
typedef void (*WORKQUEUE_CB)(void *data);

typedef struct {
    WORKQUEUE_CB cb;
    void *data;
} WORK_ITEM_T;
typedef BOOL_T (*WORKQUEUE_TRAVERSE_CB)(WORK_ITEM_T *item, void *ctx);

/**
 * @brief create and initialize a workqueue which runs in thread context
 *
 * @param[in] queue_len the maximum number of items that the workqueue can
 * contain
 * @param[in] thread_cfg thread param
 * @param[out] handle the workqueue handle
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workqueue_create(const uint16_t queue_len, THREAD_CFG_T *thread_cfg, WORKQUEUE_HANDLE *handle);

/**
 * @brief put work task in workqueue
 *
 * @param[in] handle the workqueue handle
 * @param[in] cb the work callback
 * @param[in] data the work data
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workqueue_schedule(WORKQUEUE_HANDLE handle, WORKQUEUE_CB cb, void *data);

/**
 * @brief put work task in workqueue, instant will be dequeued first
 *
 * @param[in] handle the workqueue handle
 * @param[in] cb the work callback
 * @param[in] data the work data
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workqueue_schedule_instant(WORKQUEUE_HANDLE handle, WORKQUEUE_CB cb, void *data);

/**
 * @brief cancel work task in workqueue
 *
 * @param[in] handle the workqueue handle
 * @param[in] cb the work callback
 * @param[in] data the work data
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workqueue_cancel(WORKQUEUE_HANDLE handle, WORKQUEUE_CB cb, void *data);

/**
 * @brief traverse the queue with specific callback
 *
 * @param[in] handle the workqueue handle
 * @param[in] cb the traverse callback
 * @param[in] ctx the traverse context
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workqueue_traverse(WORKQUEUE_HANDLE handle, WORKQUEUE_TRAVERSE_CB cb, void *ctx);

/**
 * @brief get the workqueue item number
 *
 * @param[in] handle the workqueue handle
 *
 * @return the current item counts
 */
uint16_t tal_workqueue_get_num(WORKQUEUE_HANDLE handle);

/**
 * @brief release the workqueue
 *
 * @param[in] handle the workqueue handle
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workqueue_release(WORKQUEUE_HANDLE handle);

/**
 * @brief get thread handle of the workqueue
 *
 * @param[in] handle the workqueue handle
 *
 * @return thread handle
 */
THREAD_HANDLE tal_workqueue_get_thread(WORKQUEUE_HANDLE handle);

typedef void *DELAYED_WORK_HANDLE;

/**
 * @brief init delayed work in workqueue
 *
 * @param[in] handle the workqueue handle
 * @param[in] cb the work callback
 * @param[in] data the work data
 * @param[out] delayed_work handle of delayed work
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workqueue_init_delayed(WORKQUEUE_HANDLE handle, WORKQUEUE_CB cb, void *data,
                                       DELAYED_WORK_HANDLE *delayed_work);

/**
 * @brief put work in workqueue after delay
 *
 * @param[in] delayed_work handle of delayed work
 * @param[in] interval number of ms to wait or 0 for immediate execution
 * @param[in] type see @LOOP_TYPE
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workqueue_start_delayed(DELAYED_WORK_HANDLE delayed_work, TIME_MS interval, LOOP_TYPE type);

/**
 * @brief stop delayed work
 *
 * @param[in] delayed_work handle of delayed work
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workqueue_stop_delayed(DELAYED_WORK_HANDLE delayed_work);

/**
 * @brief cancel delayed work
 *
 * @param[in] delayed_work handle of delayed work
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workqueue_cancel_delayed(DELAYED_WORK_HANDLE delayed_work);

#ifdef __cplusplus
}
#endif
#endif
