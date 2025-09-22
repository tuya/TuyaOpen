/**
 * @file tal_workqueue.c
 * @brief Implements the work queue functionality for Tuya IoT applications.
 *
 * This source file provides the implementation of the work queue functionality
 * within the Tuya Abstract Layer (TAL), enabling the scheduling and execution
 * of tasks in a multi-threaded environment. It defines the structure and
 * operations of work queues, including task submission, execution, and
 * synchronization mechanisms. The work queue system is designed to facilitate
 * efficient task management and execution, supporting the development of
 * responsive and scalable Tuya IoT applications.
 *
 * Key components include:
 * - Definition of the work queue structure with queue, thread, and semaphore
 * handles.
 * - Implementation of the work queue thread callback for task execution.
 * - Synchronization mechanisms to ensure thread-safe operation and task
 * execution.
 *
 * The implementation leverages Tuya's infrastructure components, such as
 * queues, threads, and semaphores, to provide a robust and efficient work queue
 * system. It is designed to handle tasks of varying priorities, ensuring that
 * critical tasks are processed in a timely manner while maintaining overall
 * system responsiveness.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_queue.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "tal_semaphore.h"
#include "tal_workqueue.h"
#include "tal_sw_timer.h"

typedef struct {
    TUYA_QUEUE_HANDLE queue;
    THREAD_HANDLE thread;
    SEM_HANDLE sem;
    WORKQUEUE_CB last_cb; // used to debug which cb is blocked
} TAL_WORKQUEUE_T;

static void __work_thread_cb(void *data)
{
    OPERATE_RET op_ret = OPRT_OK;
    TAL_WORKQUEUE_T *workqueue = (TAL_WORKQUEUE_T *)data;
    WORK_ITEM_T work_item = {0};

    while (THREAD_STATE_RUNNING == tal_thread_get_state(workqueue->thread)) {
        op_ret = tal_semaphore_wait(workqueue->sem, SEM_WAIT_FOREVER);
        if (OPRT_OK != op_ret) {
            tal_system_sleep(10);
            continue;
        }

        op_ret = tuya_queue_output(workqueue->queue, &work_item);
        if (OPRT_OK != op_ret) {
            tal_system_sleep(10);
            continue;
        }

        if (work_item.cb) {
            workqueue->last_cb = work_item.cb;
            work_item.cb(work_item.data);
            workqueue->last_cb = NULL;
        }
    }
}

static BOOL_T __work_cancel_traverse(void *item, void *ctx)
{
    BOOL_T is_same = FALSE;
    WORK_ITEM_T *src = (WORK_ITEM_T *)item;
    WORK_ITEM_T *dst = (WORK_ITEM_T *)ctx;

    if (src && dst) {
        if (dst->cb && (dst->cb == src->cb)) {
            is_same = TRUE;
        }

        if (dst->data && (dst->data == src->data)) {
            is_same = TRUE;
        }
    }

    if (is_same) {
        src->cb = NULL; // stop exe
    }

    return TRUE;
}

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
OPERATE_RET tal_workqueue_create(const uint16_t queue_len, THREAD_CFG_T *thread_cfg, WORKQUEUE_HANDLE *handle)
{
    OPERATE_RET op_ret = OPRT_OK;
    TAL_WORKQUEUE_T *workqueue = NULL;

    if ((0 == queue_len) || (NULL == thread_cfg) || (NULL == handle)) {
        return OPRT_INVALID_PARM;
    }

    workqueue = (TAL_WORKQUEUE_T *)tal_calloc(1, sizeof(TAL_WORKQUEUE_T));
    if (NULL == workqueue) {
        return OPRT_MALLOC_FAILED;
    }

    op_ret = tuya_queue_create(queue_len, sizeof(WORK_ITEM_T), &workqueue->queue);
    if (OPRT_OK != op_ret) {
        tal_free(workqueue);
        return op_ret;
    }

    op_ret = tal_semaphore_create_init(&workqueue->sem, 0, queue_len);
    if (OPRT_OK != op_ret) {
        tuya_queue_release(workqueue->queue);
        tal_free(workqueue);
        return op_ret;
    }

    op_ret = tal_thread_create_and_start(&workqueue->thread, NULL, NULL, __work_thread_cb, workqueue, thread_cfg);
    if (OPRT_OK != op_ret) {
        tal_semaphore_release(workqueue->sem);
        tuya_queue_release(workqueue->queue);
        tal_free(workqueue);
    } else {
        *handle = workqueue;
    }

    return op_ret;
}

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
OPERATE_RET tal_workqueue_schedule(WORKQUEUE_HANDLE handle, WORKQUEUE_CB cb, void *data)
{
    OPERATE_RET op_ret = OPRT_OK;

    if ((NULL == handle) || (NULL == cb)) {
        return OPRT_INVALID_PARM;
    }

    TAL_WORKQUEUE_T *workqueue = (TAL_WORKQUEUE_T *)handle;
    WORK_ITEM_T work_item = {.cb = cb, .data = data};

    op_ret = tuya_queue_input(workqueue->queue, &work_item);
    if (OPRT_OK == op_ret) {
        op_ret = tal_semaphore_post(workqueue->sem);
    }

    return op_ret;
}

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
OPERATE_RET tal_workqueue_schedule_instant(WORKQUEUE_HANDLE handle, WORKQUEUE_CB cb, void *data)
{
    OPERATE_RET op_ret = OPRT_OK;

    if ((NULL == handle) || (NULL == cb)) {
        return OPRT_INVALID_PARM;
    }

    TAL_WORKQUEUE_T *workqueue = (TAL_WORKQUEUE_T *)handle;
    WORK_ITEM_T work_item = {.cb = cb, .data = data};

    op_ret = tuya_queue_input_instant(workqueue->queue, &work_item);
    if (OPRT_OK == op_ret) {
        op_ret = tal_semaphore_post(workqueue->sem);
    }

    return op_ret;
}

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
OPERATE_RET tal_workqueue_cancel(WORKQUEUE_HANDLE handle, WORKQUEUE_CB cb, void *data)
{
    if ((NULL == handle) || ((NULL == cb) && (NULL == data))) {
        return OPRT_INVALID_PARM;
    }

    TAL_WORKQUEUE_T *workqueue = (TAL_WORKQUEUE_T *)handle;
    WORK_ITEM_T work_item = {.cb = cb, .data = data};

    return tuya_queue_traverse(workqueue->queue, __work_cancel_traverse, &work_item);
}

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
OPERATE_RET tal_workqueue_traverse(WORKQUEUE_HANDLE handle, WORKQUEUE_TRAVERSE_CB cb, void *ctx)
{
    if (NULL == handle || NULL == cb) {
        return OPRT_INVALID_PARM;
    }

    TAL_WORKQUEUE_T *workqueue = (TAL_WORKQUEUE_T *)handle;
    return tuya_queue_traverse(workqueue->queue, (TRAVERSE_CB)cb, ctx);
}

/**
 * @brief get the workqueue item number
 *
 * @param[in] handle the workqueue handle
 *
 * @return the current item counts
 */
uint16_t tal_workqueue_get_num(WORKQUEUE_HANDLE handle)
{
    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    TAL_WORKQUEUE_T *workqueue = (TAL_WORKQUEUE_T *)handle;

    if (workqueue->last_cb) {
        PR_NOTICE("%p:last_cb %p", workqueue->thread, workqueue->last_cb);
    }

    return tuya_queue_get_used_num(workqueue->queue);
}

/**
 * @brief release the workqueue
 *
 * @param[in] handle the workqueue handle
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workqueue_release(WORKQUEUE_HANDLE handle)
{
    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET op_ret = OPRT_OK;
    uint32_t count = 1;
    TAL_WORKQUEUE_T *workqueue = (TAL_WORKQUEUE_T *)handle;

    op_ret = tal_thread_delete(workqueue->thread);
    if (OPRT_OK != op_ret) {
        return op_ret;
    }

    tal_semaphore_post(workqueue->sem);

    while (THREAD_STATE_DELETE != tal_thread_get_state(workqueue->thread)) {
        tal_system_sleep(10);
        if ((count++) % 500 == 0) {
            PR_NOTICE("%p still running", workqueue->thread);
        }
    }

    tuya_queue_release(workqueue->queue);
    tal_semaphore_release(workqueue->sem);
    tal_free(workqueue);

    return OPRT_OK;
}

/**
 * @brief get thread handle of the workqueue
 *
 * @param[in] handle the workqueue handle
 *
 * @return thread handle
 */
THREAD_HANDLE tal_workqueue_get_thread(WORKQUEUE_HANDLE handle)
{
    if (NULL == handle) {
        return NULL;
    }

    TAL_WORKQUEUE_T *workqueue = (TAL_WORKQUEUE_T *)handle;
    return workqueue->thread;
}

typedef struct {
    TIMER_ID timer;
    WORKQUEUE_CB cb;
    void *data;
    WORKQUEUE_HANDLE handle;
} DELAYED_WORK_T;

void __delayed_work_cb(TIMER_ID timer_id, void *arg)
{
    DELAYED_WORK_T *p_delayed_work = (DELAYED_WORK_T *)arg;

    tal_workqueue_schedule(p_delayed_work->handle, p_delayed_work->cb, p_delayed_work->data);
}

/**
 * @brief init delayed work task in workqueue
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
                                       DELAYED_WORK_HANDLE *delayed_work)
{
    OPERATE_RET op_ret = OPRT_OK;

    if (NULL == handle || NULL == delayed_work) {
        return OPRT_INVALID_PARM;
    }

    DELAYED_WORK_T *p_delayed_work = (DELAYED_WORK_T *)tal_calloc(1, sizeof(DELAYED_WORK_T));
    if (NULL == p_delayed_work) {
        return OPRT_MALLOC_FAILED;
    }

    p_delayed_work->data = data;
    p_delayed_work->cb = cb;
    p_delayed_work->handle = handle;

    op_ret = tal_sw_timer_create(__delayed_work_cb, p_delayed_work, &p_delayed_work->timer);
    if (OPRT_OK != op_ret) {
        tal_free(p_delayed_work);
        return OPRT_COM_ERROR;
    }

    *delayed_work = (DELAYED_WORK_HANDLE)p_delayed_work;

    return OPRT_OK;
}

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
OPERATE_RET tal_workqueue_start_delayed(DELAYED_WORK_HANDLE delayed_work, TIME_MS interval, LOOP_TYPE type)
{
    if (NULL == delayed_work) {
        return OPRT_INVALID_PARM;
    }

    DELAYED_WORK_T *p_delayed_work = (DELAYED_WORK_T *)delayed_work;

    return tal_sw_timer_start(p_delayed_work->timer, interval, type);
}

/**
 * @brief stop delayed work
 *
 * @param[in] delayed_work handle of delayed work
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workqueue_stop_delayed(DELAYED_WORK_HANDLE delayed_work)
{
    if (NULL == delayed_work) {
        return OPRT_INVALID_PARM;
    }

    DELAYED_WORK_T *p_delayed_work = (DELAYED_WORK_T *)delayed_work;

    return tal_sw_timer_stop(p_delayed_work->timer);
}

/**
 * @brief cancel delay work task in workqueue
 *
 * @param[in] delayed_work handle of delayed work
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_workqueue_cancel_delayed(DELAYED_WORK_HANDLE delayed_work)
{
    if (NULL == delayed_work) {
        return OPRT_INVALID_PARM;
    }

    DELAYED_WORK_T *p_delayed_work = (DELAYED_WORK_T *)delayed_work;

    tal_sw_timer_delete(p_delayed_work->timer);
    tal_workqueue_cancel(p_delayed_work->handle, p_delayed_work->cb, p_delayed_work->data);

    tal_free(p_delayed_work);

    return OPRT_OK;
}
