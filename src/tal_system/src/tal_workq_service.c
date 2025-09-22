/**
 * @file tal_workq_service.c
 * @brief Implements work queue services for Tuya IoT applications.
 *
 * This source file provides the implementation of work queue services within
 * the Tuya Abstract Layer (TAL), facilitating the management of asynchronous
 * tasks and operations. It defines and manages work queues for system and
 * high-priority tasks, allowing for efficient task scheduling and execution in
 * a multi-threaded environment. The work queue service supports dynamic task
 * allocation, prioritization, and execution, ensuring timely processing of
 * tasks with varying levels of importance.
 *
 * Key functionalities include:
 * - Creation and management of system and high-priority work queues.
 * - Dynamic allocation and scheduling of tasks within work queues.
 * - Thread-safe task submission and execution.
 * - Configurable task stack sizes and queue lengths.
 *
 * The implementation utilizes static handles for system and high-priority work
 * queues, and defines configurable parameters for stack sizes and maximum
 * number of nodes in work and message queues, ensuring flexibility and
 * scalability of task management.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_iot_config.h"
#include "tal_workq_service.h"
#include "tal_log.h"

#ifndef MAX_NODE_NUM_WORK_QUEUE
#define MAX_NODE_NUM_WORK_QUEUE 100
#endif

#ifndef MAX_NODE_NUM_MSG_QUEUE
#define MAX_NODE_NUM_MSG_QUEUE 100
#endif

#ifndef STACK_SIZE_WORK_QUEUE
#define STACK_SIZE_WORK_QUEUE (5 * 1024)
#endif

#ifndef STACK_SIZE_MSG_QUEUE
#define STACK_SIZE_MSG_QUEUE (4 * 1024)
#endif

static WORKQUEUE_HANDLE wq_system;
static WORKQUEUE_HANDLE wq_highpri;

/**
 * @brief init ty work queue
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workq_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    THREAD_CFG_T thread_cfg;

    if (wq_system) {
        return OPRT_OK;
    }

    thread_cfg.priority = THREAD_PRIO_2;
    thread_cfg.stackDepth = STACK_SIZE_WORK_QUEUE;
#if defined(TUYA_SECURITY_LEVEL) && (TUYA_SECURITY_LEVEL >= TUYA_SL_1)
    thread_cfg.stackDepth += 1024;
#endif
    thread_cfg.thrdname = "wq_system";
    TUYA_CALL_ERR_GOTO(tal_workqueue_create(MAX_NODE_NUM_WORK_QUEUE, &thread_cfg, &wq_system), ERR_EXIT);

    thread_cfg.priority = THREAD_PRIO_1;
    thread_cfg.stackDepth = STACK_SIZE_MSG_QUEUE;
#if defined(TUYA_SECURITY_LEVEL) && (TUYA_SECURITY_LEVEL >= TUYA_SL_1)
    thread_cfg.stackDepth += 1024;
#endif
    thread_cfg.thrdname = "wq_highpri";
    TUYA_CALL_ERR_GOTO(tal_workqueue_create(MAX_NODE_NUM_MSG_QUEUE, &thread_cfg, &wq_highpri), ERR_EXIT);

    return OPRT_OK;

ERR_EXIT:
    if (wq_system) {
        tal_workqueue_release(wq_system);
        wq_system = NULL;
    }

    if (wq_highpri) {
        tal_workqueue_release(wq_highpri);
        wq_highpri = NULL;
    }

    return rt;
}

/**
 * @brief get handle of workqueue service
 *
 * @param[in] service the workqueue service
 *
 * @return WORKQUEUE_HANDLE handle of workqueue service
 */
WORKQUEUE_HANDLE tal_workq_get_handle(WORKQ_SERVICE_E service)
{
    WORKQUEUE_HANDLE handle = NULL;

    if (WORKQ_SYSTEM == service) {
        handle = wq_system;
    } else if (WORKQ_HIGHTPRI == service) {
        handle = wq_highpri;
    }

    return handle;
}

/**
 * @brief add work to work queue
 *
 * @param[in] service the workqueue service
 * @param[in] cb, call back of work
 * @param[in] data, parameter of call back
 *
 * @note This API is used for add work to work queue
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workq_schedule(WORKQ_SERVICE_E service, WORKQUEUE_CB cb, void *data)
{
    return tal_workqueue_schedule(tal_workq_get_handle(service), cb, data);
}

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
OPERATE_RET tal_workq_schedule_instant(WORKQ_SERVICE_E service, WORKQUEUE_CB cb, void *data)
{
    return tal_workqueue_schedule_instant(tal_workq_get_handle(service), cb, data);
}

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
OPERATE_RET tal_workq_cancel(WORKQ_SERVICE_E service, WORKQUEUE_CB cb, void *data)
{
    return tal_workqueue_cancel(tal_workq_get_handle(service), cb, data);
}

/**
 * @brief get current work number in work queue.
 *
 * @param[in] NONE
 *
 * @note This API is used for get the current work number in work queue.
 *
 * @return current work number in the work queue
 */
uint16_t tal_workq_get_num(WORKQ_SERVICE_E service)
{
    return tal_workqueue_get_num(tal_workq_get_handle(service));
}

// used for debug
static BOOL_T _dump_cb(WORK_ITEM_T *item, void *ctx)
{
    PR_NOTICE("cb:%p", item->cb);
    return TRUE;
}

void tal_workq_dump(WORKQ_SERVICE_E service)
{
    PR_NOTICE("---------workq-%d dump begin---------", service);
    tal_workqueue_traverse(tal_workq_get_handle(service), _dump_cb, NULL);
    tal_thread_diagnose(tal_workqueue_get_thread(tal_workq_get_handle(service)));
    PR_NOTICE("---------workq-%d dump end---------", service);
}

/**
 * @brief init delayed work task in workqueue
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
                                   DELAYED_WORK_HANDLE *delayed_work)
{
    return tal_workqueue_init_delayed(tal_workq_get_handle(service), cb, data, delayed_work);
}

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
OPERATE_RET tal_workq_start_delayed(DELAYED_WORK_HANDLE delayed_work, TIME_MS interval, LOOP_TYPE type)
{
    return tal_workqueue_start_delayed(delayed_work, interval, type);
}

/**
 * @brief stop delayed work
 *
 * @param[in] delayed_work handle of delayed work
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workq_stop_delayed(DELAYED_WORK_HANDLE delayed_work)
{
    return tal_workqueue_stop_delayed(delayed_work);
}

/**
 * @brief cancel delayed work
 *
 * @param[in] delayed_work handle of delayed work
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workq_cancel_delayed(DELAYED_WORK_HANDLE delayed_work)
{
    return tal_workqueue_cancel_delayed(delayed_work);
}
