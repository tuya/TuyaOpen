/**
 * @file tal_thread.h
 * @brief Provides thread management functions for Tuya IoT applications.
 *
 * This header file defines the interface for thread management in Tuya IoT
 * applications, including creating and starting threads, stopping and deleting
 * threads, checking thread context, and getting thread running status. It
 * offers functionalities to manage threads' lifecycle, prioritize tasks, and
 * ensure efficient execution of concurrent operations within Tuya-based IoT
 * applications. The API abstracts underlying threading mechanisms, providing a
 * portable and simplified interface for application development.
 *
 * Thread management is crucial for achieving multitasking and parallel
 * processing in embedded systems, enabling applications to perform multiple
 * operations simultaneously, thus improving responsiveness and operational
 * efficiency.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_THREAD_H__
#define __TAL_THREAD_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *THREAD_HANDLE;

/**
 * @brief max length of thread name
 *
 */
#define TAL_THREAD_MAX_NAME_LEN 16

/**
 * @brief thread process function
 *
 */
typedef void (*THREAD_FUNC_CB)(void *args);
/**
 * @brief thread enter function
 *
 */
typedef void (*THREAD_ENTER_CB)(void);

/**
 * @brief thread exut function
 *
 */
typedef void (*THREAD_EXIT_CB)(void); // thread extract
/**
 * @brief thread running status
 *
 */
typedef enum {
    THREAD_STATE_EMPTY = 0,
    THREAD_STATE_RUNNING,
    THREAD_STATE_STOP,
    THREAD_STATE_DELETE,
} THREAD_STATE_E;

/**
 * @brief thread priority
 *
 */
typedef enum {
    THREAD_PRIO_0 = 5,
    THREAD_PRIO_1 = 4,
    THREAD_PRIO_2 = 3,
    THREAD_PRIO_3 = 2,
    THREAD_PRIO_4 = 1,
    THREAD_PRIO_5 = 0,
    THREAD_PRIO_6 = 0,
} THREAD_PRIO_E;
/**
 * @brief thread parameters
 *
 */
typedef struct {
    uint32_t stackDepth; // stack size
    uint8_t priority;    // thread priority
    char *thrdname;      // thread name
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    uint8_t psram_mode : 1; // run in psram
#endif
} THREAD_CFG_T;

/**
 * @brief create and start a tuya sdk thread
 *
 * @param[in] enter: the function called before the thread process called.can be
 * null
 * @param[in] exit: the function called after the thread process called.can be
 * null
 * @param[in] func: the main thread process function
 * @param[in] func_args: the args of the pThrdFunc.can be null
 * @param[in] cfg: the param of creating a thread
 * @param[out] handle: the tuya sdk thread context
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_thread_create_and_start(THREAD_HANDLE *handle, const THREAD_ENTER_CB enter, const THREAD_EXIT_CB exit,
                                        const THREAD_FUNC_CB func, const void *func_args, const THREAD_CFG_T *cfg);
/**
 * @brief stop and free a tuya sdk thread
 *
 * @param[in] handle: the input thread context
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_thread_delete(const THREAD_HANDLE handle);

/**
 * @brief check the function caller is in the input thread context
 *
 * @param[in] handle: the input thread context
 * @param[in] bl: run in self space
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_thread_is_self(const THREAD_HANDLE handle, BOOL_T *bl);

/**
 * @brief get the thread context running status
 *
 * @param[in] thrdHandle: the input thread context
 * @return the thread status
 */
THREAD_STATE_E tal_thread_get_state(const THREAD_HANDLE handle);

/**
 * @brief diagnose the thread(dump task stack, etc.)
 *
 * @param[in] thrdHandle: the input thread context
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_thread_diagnose(const THREAD_HANDLE handle);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
