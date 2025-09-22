/**
 * @file tal_workq_service.h
 * @brief Provides work queue services for Tuya IoT applications.
 *
 * This header file defines the interface for work queue services in Tuya IoT
 * applications, offering functionalities to manage and execute tasks in an
 * organized and efficient manner. It supports the creation of both low and high
 * priority work queues, allowing tasks to be scheduled, executed immediately,
 * delayed, or cancelled based on application requirements. The API facilitates
 * the management of task execution sequences and priorities, enhancing the
 * responsiveness and performance of Tuya-based IoT applications.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_WORKQ_SERVICE_H__
#define __TAL_WORKQ_SERVICE_H__

#include "tal_workqueue.h"

/**
 * @brief TuyaOS provides developers with two workqueue service for convenience.
 */
typedef enum {
    /**
     * low priority workqueue(block operations are allowed)
     */
    WORKQ_SYSTEM,
    /**
     * high priority workqueue (block operations are not allowed)
     */
    WORKQ_HIGHTPRI
} WORKQ_SERVICE_E;

/**
 * @brief init ty work queue service
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workq_init(void);

/**
 * @brief get handle of workqueue service
 *
 * @param[in] service the workqueue service
 *
 * @return WORKQUEUE_HANDLE handle of workqueue service
 */
WORKQUEUE_HANDLE tal_workq_get_handle(WORKQ_SERVICE_E service);

/**
 * @brief add work to work queue
 *
 * @param[in] service the workqueue service
 * @param[in] cb, call back of work
 * @param[in] data, parameter of call back
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workq_schedule(WORKQ_SERVICE_E service, WORKQUEUE_CB cb, void *data);

/**
 * @brief put work task in workqueue, instant will be dequeued first
 *
 * @param[in] service the workqueue service
 * @param[in] cb the work callback
 * @param[in] data the work data
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workq_schedule_instant(WORKQ_SERVICE_E service, WORKQUEUE_CB cb, void *data);

/**
 * @brief cancel work task in workqueue
 *
 * @param[in] service the workqueue service
 * @param[in] cb the work callback
 * @param[in] data the work data
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workq_cancel(WORKQ_SERVICE_E service, WORKQUEUE_CB cb, void *data);

/**
 * @brief get current work number in work queue.
 *
 * @param[in] service the workqueue service
 *
 * @note This API is used for get the current work number in work queue.
 *
 * @return current work number in the work queue
 */
uint16_t tal_workq_get_num(WORKQ_SERVICE_E service);

/**
 * @brief dump all work in work queue.
 *
 * @param[in] service the workqueue service
 */
void tal_workq_dump(WORKQ_SERVICE_E service);

/**
 * @brief delayed work entry definition
 */
typedef struct {
    DELAYED_WORK_HANDLE *delayed_work; // delayed work handle
    WORKQUEUE_CB delayed_work_cb;      // delayed work callback
} DELAYED_WORK_ENTRY_S;

/**
 * @brief init delayed work task
 *
 * @param[in] service the workqueue service
 * @param[in] cb the work callback
 * @param[in] data the work data
 * @param[out] delayed_work handle of delayed work
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workq_init_delayed(WORKQ_SERVICE_E service, WORKQUEUE_CB cb, void *data,
                                   DELAYED_WORK_HANDLE *delayed_work);

/**
 * @brief put work task in workqueue after delay
 *
 * @param[in] delayed_work handle of delayed work
 * @param[in] interval number of ms to wait or 0 for immediate execution
 * @param[in] type see @LOOP_TYPE
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workq_start_delayed(DELAYED_WORK_HANDLE delayed_work, TIME_MS interval, LOOP_TYPE type);

/**
 * @brief stop delayed work
 *
 * @param[in] delayed_work handle of delayed work
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workq_stop_delayed(DELAYED_WORK_HANDLE delayed_work);

/**
 * @brief cancel delay work
 *
 * @param[in] delayed_work handle of delayed work
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workq_cancel_delayed(DELAYED_WORK_HANDLE delayed_work);

#endif //__TAL_WORKQ_SERVICE_H__