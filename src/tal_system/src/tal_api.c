/**
 * @file tal_api.c
 * @brief Provides Tuya Abstract Layer (TAL) API implementations for semaphores,
 * mutexes, OTA, and queues.
 *
 * This source file implements the Tuya Abstract Layer (TAL) APIs for managing
 * semaphores, mutexes, Over-The-Air (OTA) updates, and message queues. It
 * serves as an abstraction layer that interfaces with the lower-level Tuya
 * Kernel Layer (TKL) to provide simplified and unified API calls for
 * application development. The TAL APIs facilitate the development of Tuya IoT
 * applications by offering standardized operations for synchronization
 * primitives (semaphores and mutexes), handling OTA updates, and managing
 * inter-thread communication through queues.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_semaphore.h"
#include "tkl_mutex.h"
#include "tkl_ota.h"
#include "tkl_queue.h"
#include "tal_mutex.h"
#include "tal_semaphore.h"
#include "tal_queue.h"
#include "tal_ota.h"

//! sem
OPERATE_RET tal_semaphore_create_init(SEM_HANDLE *handle, uint32_t sem_cnt, uint32_t sem_max)
{
    return tkl_semaphore_create_init(handle, sem_cnt, sem_max);
}

OPERATE_RET tal_semaphore_wait(SEM_HANDLE handle, uint32_t timeout)
{
    return tkl_semaphore_wait(handle, timeout);
}

OPERATE_RET tal_semaphore_post(SEM_HANDLE handle)
{
    return tkl_semaphore_post(handle);
}

OPERATE_RET tal_semaphore_release(SEM_HANDLE handle)
{
    return tkl_semaphore_release(handle);
}

//! mutex
OPERATE_RET tal_mutex_create_init(MUTEX_HANDLE *handle)
{
    return tkl_mutex_create_init(handle);
}

OPERATE_RET tal_mutex_lock(const MUTEX_HANDLE handle)
{
    return tkl_mutex_lock(handle);
}

OPERATE_RET tal_mutex_unlock(const MUTEX_HANDLE handle)
{
    return tkl_mutex_unlock(handle);
}

OPERATE_RET tal_mutex_release(const MUTEX_HANDLE handle)
{
    return tkl_mutex_release(handle);
}

//! ota
OPERATE_RET tal_ota_get_ability(uint32_t *image_size, TUYA_OTA_TYPE_E *type)
{
    return tkl_ota_get_ability(image_size, type);
}

OPERATE_RET tal_ota_start_notify(uint32_t image_size, TUYA_OTA_TYPE_E type, TUYA_OTA_PATH_E path)
{
    return tkl_ota_start_notify(image_size, type, path);
}

OPERATE_RET tal_ota_data_process(TUYA_OTA_DATA_T *pack, uint32_t *remain_len)
{
    return tkl_ota_data_process(pack, remain_len);
}

OPERATE_RET tal_ota_end_notify(BOOL_T reset)
{
    return tkl_ota_end_notify(reset);
}

//! queue
OPERATE_RET tal_queue_create_init(QUEUE_HANDLE *queue, int msgsize, int msgcount)
{
    return tkl_queue_create_init(queue, msgsize, msgcount);
}

OPERATE_RET tal_queue_post(QUEUE_HANDLE queue, void *data, uint32_t timeout)
{
    return tkl_queue_post(queue, data, timeout);
}

OPERATE_RET tal_queue_fetch(QUEUE_HANDLE queue, void *msg, uint32_t timeout)
{
    return tkl_queue_fetch(queue, msg, timeout);
}

void tal_queue_free(QUEUE_HANDLE queue)
{
    tkl_queue_free(queue);
}
