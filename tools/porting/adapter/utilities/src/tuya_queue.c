/**
 * @file tuya_queue.h
 * @brief tuya common queue module
 * @version 1.0
 * @date 2019-10-30
 *
 * @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_system.h"
#include "tkl_memory.h"

#include "tuya_list.h"
#include "tuya_queue.h"

#if defined(OPERATING_SYSTEM) && (SYSTEM_NON_OS == OPERATING_SYSTEM)
#define QUEUE_CREATE_LOCK(queue)  OPRT_OK
#define QUEUE_RELEASE_LOCK(queue) OPRT_OK
#define QUEUE_LOCK(queue)         TKL_ENTER_CRITICAL()
#define QUEUE_UNLOCK(queue)       TKL_EXIT_CRITICAL()
#else
#include "tkl_mutex.h"

#define QUEUE_CREATE_LOCK(queue)  tkl_mutex_create_init(&queue->mutex)
#define QUEUE_RELEASE_LOCK(queue) tkl_mutex_release(queue->mutex)
#define QUEUE_LOCK(queue)         tkl_mutex_lock(queue->mutex)
#define QUEUE_UNLOCK(queue)       tkl_mutex_unlock(queue->mutex)
#endif

typedef enum { POLICY_SEND_TO_BACK, POLICY_SEND_TO_FRONT, POLICY_MAX } ENQUEUE_POLICY_E;

typedef struct {
    LIST_HEAD node;
    uint8_t data[];
} QUEUE_ITEM_T;

typedef struct {
#if defined(OPERATING_SYSTEM) && (SYSTEM_NON_OS != OPERATING_SYSTEM)
    TKL_MUTEX_HANDLE mutex;
#endif

    uint32_t item_size;
    uint32_t queue_len;
    uint32_t queue_free;

    LIST_HEAD head;
} TUYA_QUEUE_T;

static OPERATE_RET __enqueue(TUYA_QUEUE_HANDLE handle, const void *item, ENQUEUE_POLICY_E policy)
{
    OPERATE_RET op_ret = OPRT_OK;

    if (NULL == handle || NULL == item || policy >= POLICY_MAX) {
        return OPRT_INVALID_PARM;
    }

    TUYA_QUEUE_T *queue = (TUYA_QUEUE_T *)handle;

    QUEUE_ITEM_T *queue_item = (QUEUE_ITEM_T *)tkl_system_malloc(sizeof(QUEUE_ITEM_T) + queue->item_size);
    if (NULL == queue_item) {
        return OPRT_MALLOC_FAILED;
    }

    INIT_LIST_HEAD(&(queue_item->node));
    memcpy(queue_item->data, (void *)item, queue->item_size);

    QUEUE_LOCK(queue);
    if (queue->queue_free > 0) {
        if (POLICY_SEND_TO_BACK == policy) {
            tuya_list_add_tail(&(queue_item->node), &(queue->head));
        } else if (POLICY_SEND_TO_FRONT == policy) {
            tuya_list_add(&(queue_item->node), &(queue->head));
        }
        queue->queue_free--;
    } else {
        tkl_system_free(queue_item);
        op_ret = OPRT_EXCEED_UPPER_LIMIT;
    }
    QUEUE_UNLOCK(queue);

    return op_ret;
}

/**
 * @brief create and initialize a queue (FIFO)
 *
 * @param[in] queue_len the maximum number of items that the queue can contain.
 * @param[in] item_size the number of bytes each item in the queue will require.
 * @param[out] handle the queue handle
 *
 * @note items are queued by copy, not by reference. Each item on the queue must be the same size.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_create(const uint32_t queue_len, const uint32_t item_size, TUYA_QUEUE_HANDLE *handle)
{
    OPERATE_RET op_ret = OPRT_OK;
    TUYA_QUEUE_T *queue = NULL;

    if ((NULL == handle) || (0 == queue_len) || (0 == item_size)) {
        return OPRT_INVALID_PARM;
    }

    queue = (TUYA_QUEUE_T *)tkl_system_malloc(sizeof(TUYA_QUEUE_T));
    if (!queue) {
        return OPRT_MALLOC_FAILED;
    }

    op_ret = QUEUE_CREATE_LOCK(queue);
    if (OPRT_OK != op_ret) {
        tkl_system_free(queue);
        return OPRT_COM_ERROR;
    }

    queue->item_size = item_size;
    queue->queue_len = queue_len;
    queue->queue_free = queue_len;
    INIT_LIST_HEAD(&(queue->head));

    *handle = (TUYA_QUEUE_HANDLE)queue;

    return OPRT_OK;
}

/**
 * @brief enqueue
 *
 * @param[in] handle the queue handle
 * @param[in] item pointer to the item that is to be placed on the queue.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_input(TUYA_QUEUE_HANDLE handle, const void *item)
{
    return __enqueue(handle, item, POLICY_SEND_TO_BACK);
}

/**
 * @brief enqueue, instant will be dequeued first
 *
 * @param[in] handle the queue handle
 * @param[in] item pointer to the item that is to be placed on the queue.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_input_instant(TUYA_QUEUE_HANDLE handle, const void *item)
{
    return __enqueue(handle, item, POLICY_SEND_TO_FRONT);
}

/**
 * @brief dequeue
 *
 * @param[in] handle the queue handle
 * @param[in] item the dequeue item buffer, NULL indicates discard the item
 *
 * @return OPRT_OK on success, others on failed, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_output(TUYA_QUEUE_HANDLE handle, const void *item)
{
    OPERATE_RET op_ret = OPRT_OK;

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    TUYA_QUEUE_T *queue = (TUYA_QUEUE_T *)handle;

    QUEUE_LOCK(queue);
    if (queue->queue_free < queue->queue_len) {
        QUEUE_ITEM_T *queue_item = tuya_list_entry(queue->head.next, QUEUE_ITEM_T, node);
        if (item) {
            memcpy((void *)item, queue_item->data, queue->item_size);
        }
        tuya_list_del(&(queue_item->node));
        tkl_system_free(queue_item);
        queue->queue_free++;
    } else {
        op_ret = OPRT_NOT_FOUND;
    }
    QUEUE_UNLOCK(queue);

    return op_ret;
}

/**
 * @brief get the peek item,  not dequeue
 *
 * @param[in] handle the queue handle
 * @param[out] item pointer to the buffer into which the received item will be copied.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_peek(TUYA_QUEUE_HANDLE handle, const void *item)
{
    OPERATE_RET op_ret = OPRT_OK;

    if (NULL == handle || NULL == item) {
        return OPRT_INVALID_PARM;
    }

    TUYA_QUEUE_T *queue = (TUYA_QUEUE_T *)handle;

    QUEUE_LOCK(queue);
    if (queue->queue_free < queue->queue_len) {
        QUEUE_ITEM_T *queue_item = tuya_list_entry(queue->head.next, QUEUE_ITEM_T, node);
        memcpy((void *)item, queue_item->data, queue->item_size);
    } else {
        op_ret = OPRT_NOT_FOUND;
    }
    QUEUE_UNLOCK(queue);

    return op_ret;
}

/**
 * @brief traverse the queue with specific callback
 *
 * @param[in] handle the queue handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_traverse(TUYA_QUEUE_HANDLE handle, TRAVERSE_CB cb, void *ctx)
{
    if (NULL == handle || NULL == cb) {
        return OPRT_INVALID_PARM;
    }

    TUYA_QUEUE_T *queue = (TUYA_QUEUE_T *)handle;
    struct tuya_list_head *p = NULL;
    QUEUE_ITEM_T *queue_item = NULL;

    QUEUE_LOCK(queue);
    tuya_list_for_each(p, &(queue->head))
    {
        queue_item = tuya_list_entry(p, QUEUE_ITEM_T, node);
        if (!cb(queue_item->data, ctx)) {
            break;
        }
    }
    QUEUE_UNLOCK(queue);

    return OPRT_OK;
}

/**
 * @brief clear all items of the queue
 *
 * @param[in] handle the queue handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_clear(TUYA_QUEUE_HANDLE handle)
{
    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    TUYA_QUEUE_T *queue = (TUYA_QUEUE_T *)handle;
    struct tuya_list_head *p = NULL;
    struct tuya_list_head *n = NULL;
    QUEUE_ITEM_T *queue_item = NULL;

    QUEUE_LOCK(queue);
    tuya_list_for_each_safe(p, n, &(queue->head))
    {
        queue_item = tuya_list_entry(p, QUEUE_ITEM_T, node);
        tuya_list_del(&queue_item->node);
        tkl_system_free(queue_item);
    }
    queue->queue_free = queue->queue_len;
    QUEUE_UNLOCK(queue);

    return OPRT_OK;
}

/**
 * @brief get items from start postion, not dequeue
 *
 * @param[in] handle the queue handle
 * @param[in] start the start postion
 * @param[in] items the item buffer
 * @param[in] num the item counts
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_get_batch(TUYA_QUEUE_HANDLE handle, const uint32_t start, void *items, const uint32_t num)
{
    if (NULL == handle || NULL == items || 0 == num) {
        return OPRT_INVALID_PARM;
    }

    TUYA_QUEUE_T *queue = (TUYA_QUEUE_T *)handle;
    struct tuya_list_head *p = NULL;
    QUEUE_ITEM_T *queue_item = NULL;
    uint32_t index = 0;
    uint32_t count = 0;

    QUEUE_LOCK(queue);
    tuya_list_for_each(p, &(queue->head))
    {
        if (index < start) {
            index++;
            continue;
        }

        if (count >= num) {
            break;
        }

        queue_item = tuya_list_entry(p, QUEUE_ITEM_T, node);
        memcpy((uint8_t *)items + count * queue->item_size, queue_item->data, queue->item_size);
        count++;
    }
    QUEUE_UNLOCK(queue);

    if (index != start || count != num) {
        return OPRT_NOT_FOUND;
    }

    return OPRT_OK;
}

/**
 * @brief delete the item from the queue position
 *
 * @param[in] handle the queue handle
 * @param[in] num the item count to be deleted from the queue
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_delete_batch(TUYA_QUEUE_HANDLE handle, const uint32_t num)
{
    OPERATE_RET op_ret = OPRT_OK;
    uint32_t count = num;

    if (NULL == handle || 0 == num) {
        return OPRT_INVALID_PARM;
    }

    while ((count-- > 0) && (OPRT_OK == op_ret)) {
        op_ret = tuya_queue_output(handle, NULL);
    }

    return op_ret;
}

/**
 * @brief get the free queue item number
 *
 * @param[in] handle the queue handle
 *
 * @return the current free item counts
 */
uint32_t tuya_queue_get_free_num(TUYA_QUEUE_HANDLE handle)
{
    if (NULL == handle) {
        return 0;
    }

    TUYA_QUEUE_T *queue = (TUYA_QUEUE_T *)handle;

    return queue->queue_free;
}

/**
 * @brief get the queue item number
 *
 * @param[in] handle the queue handle
 *
 * @return the current item counts
 */
uint32_t tuya_queue_get_used_num(TUYA_QUEUE_HANDLE handle)
{
    if (NULL == handle) {
        return 0;
    }

    TUYA_QUEUE_T *queue = (TUYA_QUEUE_T *)handle;
    uint32_t used_num = 0;

    QUEUE_LOCK(queue);
    used_num = queue->queue_len - queue->queue_free;
    QUEUE_UNLOCK(queue);

    return used_num;
}

/**
 * @brief get the queue item number
 *
 * @param[in] handle the queue handle
 *
 * @return the current item counts
 */
uint32_t tuya_queue_get_max_num(TUYA_QUEUE_HANDLE handle)
{
    if (NULL == handle) {
        return 0;
    }

    TUYA_QUEUE_T *queue = (TUYA_QUEUE_T *)handle;

    return queue->queue_len;
}

/**
 * @brief release the queue
 *
 * @param[in] handle the queue handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_release(TUYA_QUEUE_HANDLE handle)
{
    OPERATE_RET op_ret = OPRT_OK;

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    TUYA_QUEUE_T *queue = (TUYA_QUEUE_T *)handle;

    tuya_queue_clear(handle);

    op_ret = QUEUE_RELEASE_LOCK(queue);
    tkl_system_free(queue);

    return op_ret;
}
