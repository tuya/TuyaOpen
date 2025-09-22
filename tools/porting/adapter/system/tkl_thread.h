/**
 * @file tkl_thread.h
 * @brief Common process - adapter the thread api provide by OS
 * @version 0.1
 * @date 2020-11-09
 *
 * @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_THREAD_H__
#define __TKL_THREAD_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *TKL_THREAD_HANDLE;
typedef void (*THREAD_FUNC_T)(void *);

/**
 * @brief Create thread
 *
 * @param[out] thread: thread handle
 * @param[in] name: thread name
 * @param[in] stack_size: stack size of thread
 * @param[in] priority: priority of thread,please ref to tkl thread priority define in tuya_cloud_types.h
 * @param[in] func: the main thread process function
 * @param[in] arg: the args of the func, can be null
 *
 * @note This API is used for creating thread.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_thread_create(TKL_THREAD_HANDLE *thread, const char *name, uint32_t stack_size, uint32_t priority,
                              const THREAD_FUNC_T func, void *const arg);

/**
 * @brief Terminal thread and release thread resources
 *
 * @param[in] thread: thread handle
 *
 * @note This API is used to terminal thread and release thread resources.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_thread_release(const TKL_THREAD_HANDLE thread);

/**
 * @brief Get the thread stack's watermark
 *
 * @param[in] thread: thread handle
 * @param[out] watermark: watermark in Bytes
 *
 * @note This API is used to get the thread stack's watermark.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_thread_get_watermark(const TKL_THREAD_HANDLE thread, uint32_t *watermark);

/**
 * @brief Get the thread thread handle
 *
 * @param[out] thread: thread handle
 *
 * @note This API is used to get the thread handle.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_thread_get_id(TKL_THREAD_HANDLE *thread);

/**
 * @brief Set name of self thread
 *
 * @param[in] name: thread name
 *
 * @note This API is used to set name of self thread.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_thread_set_self_name(const char *name);

/**
 * @brief Check thread is self thread
 *
 * @param[in] thread: thread handle
 * @param[out] is_self: is self thread or not
 *
 * @note This API is used to check thread is self thread.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_thread_is_self(TKL_THREAD_HANDLE thread, BOOL_T *is_self);

/**
 * @brief Get thread priority
 *
 * @param[in] thread: thread handle, If NULL indicates the current thread
 * @param[in] priority: thread priority
 *
 * @note This API is used to get thread priority.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_thread_get_priority(TKL_THREAD_HANDLE thread, int *priority);

/**
 * @brief Set thread priority
 *
 * @param[in] thread: thread handle, If NULL indicates the current thread
 * @param[in] priority: thread priority
 *
 * @note This API is used to Set thread priority.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_thread_set_priority(TKL_THREAD_HANDLE thread, int priority);

/**
 * @brief Diagnose the thread(dump task stack, etc.)
 *
 * @param[in] thread: thread handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_thread_diagnose(TKL_THREAD_HANDLE thread);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
