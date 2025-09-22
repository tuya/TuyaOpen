/**
 * @file tuya_queue.h
 * @brief tuya common queue module
 * @version 1.0
 * @date 2019-10-30
 *
 * @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef _TUYA_QUEUE_H_
#define _TUYA_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

typedef void *TUYA_QUEUE_HANDLE;
typedef BOOL_T (*TRAVERSE_CB)(void *item, void *ctx);

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
OPERATE_RET tuya_queue_create(const uint32_t queue_len, const uint32_t item_size, TUYA_QUEUE_HANDLE *handle);

/**
 * @brief enqueue, append to the tail
 *
 * @param[in] handle the queue handle
 * @param[in] item pointer to the item that is to be placed on the queue.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_input(TUYA_QUEUE_HANDLE handle, const void *item);

/**
 * @brief enqueue, insert to the head
 *
 * @param[in] handle the queue handle
 * @param[in] item pointer to the item that is to be placed on the queue.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_input_instant(TUYA_QUEUE_HANDLE handle, const void *item);

/**
 * @brief dequeue
 *
 * @param[in] handle the queue handle
 * @param[in] item the dequeue item buffer, NULL indicates discard the item
 *
 * @return OPRT_OK on success, others on failed, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_output(TUYA_QUEUE_HANDLE handle, const void *item);

/**
 * @brief get the peek item(not dequeue)
 *
 * @param[in] handle the queue handle
 * @param[out] item pointer to the buffer into which the received item will be copied.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_peek(TUYA_QUEUE_HANDLE handle, const void *item);

/**
 * @brief traverse the queue with specific callback
 *
 * @param[in] handle the queue handle
 * @param[in] cb the callback
 * @param[in] ctx the callback context
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_traverse(TUYA_QUEUE_HANDLE handle, TRAVERSE_CB cb, void *ctx);

/**
 * @brief clear all items in the queue
 *
 * @param[in] handle the queue handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_clear(TUYA_QUEUE_HANDLE handle);

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
OPERATE_RET tuya_queue_get_batch(TUYA_QUEUE_HANDLE handle, const uint32_t start, void *items, const uint32_t num);

/**
 * @brief delete the item from the queue position
 *
 * @param[in] handle the queue handle
 * @param[in] num the item count to be deleted from the queue
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_delete_batch(TUYA_QUEUE_HANDLE handle, const uint32_t num);

/**
 * @brief get the free queue item number
 *
 * @param[in] handle the queue handle
 *
 * @return the current free item counts
 */
uint32_t tuya_queue_get_free_num(TUYA_QUEUE_HANDLE handle);

/**
 * @brief get the queue item number
 *
 * @param[in] handle the queue handle
 *
 * @return the current item counts
 */
uint32_t tuya_queue_get_used_num(TUYA_QUEUE_HANDLE handle);

/**
 * @brief get the queue item number
 *
 * @param[in] handle the queue handle
 *
 * @return the current item counts
 */
uint32_t tuya_queue_get_max_num(TUYA_QUEUE_HANDLE handle);

/**
 * @brief release the queue
 *
 * @param[in] handle the queue handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_queue_release(TUYA_QUEUE_HANDLE handle);

#ifdef __cplusplus
}
#endif
#endif
