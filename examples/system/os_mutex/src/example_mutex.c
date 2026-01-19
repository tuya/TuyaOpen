/**
 * @file example_mutex.c
 * @brief Demonstrates the use of mutexes for thread synchronization in Tuya SDK.
 *
 * This file contains examples of how to use mutexes to synchronize access to shared resources between multiple threads
 * in Tuya SDK applications. It includes the creation and deletion of mutexes, locking and unlocking mutexes within
 * thread tasks to safely update shared resources, and the creation and management of threads that access these
 * resources. The example aims to showcase best practices for thread synchronization using mutexes in the context of
 * Tuya's IoT platform.
 *
 * Key components demonstrated in this file:
 * - Initialization and release of mutexes.
 * - Creation of threads that perform operations requiring synchronized access to a shared resource.
 * - Use of mutexes to ensure thread-safe operations on a shared variable.
 * - Proper cleanup of threads and mutexes to avoid resource leaks.
 *
 * This example serves as a practical guide for developers working on multi-threaded applications in the Tuya IoT
 * ecosystem, emphasizing the importance of thread synchronization for the stability and reliability of IoT
 * applications.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tal_mutex.h"
#include "tkl_output.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE mutex1_thrd_hdl = NULL;
static THREAD_HANDLE mutex2_thrd_hdl = NULL;
static MUTEX_HANDLE mutex_hdl = NULL;

static int temp_value = 0;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief mutex task
 *
 * @param[in] :
 *
 * @return none
 */
static void __mutex_task1(void *args)
{
    for (;;) {
        tal_mutex_lock(mutex_hdl);
        temp_value = temp_value + 2;
        PR_NOTICE("task1 temp_value: %d", temp_value);
        tal_mutex_unlock(mutex_hdl);
        tal_system_sleep(2000);

        if (THREAD_STATE_STOP == tal_thread_get_state(mutex1_thrd_hdl)) {
            break;
        }
    }

    mutex1_thrd_hdl = NULL;
    PR_DEBUG("thread mutex1_thrd_hdl will delete");

    return;
}

/**
 * @brief mutex task
 *
 * @param[in] :
 *
 * @return none
 */
static void __mutex_task2(void *args)
{
    for (;;) {
        tal_mutex_lock(mutex_hdl);
        temp_value--;
        PR_NOTICE("task2 temp_value: %d", temp_value);
        tal_mutex_unlock(mutex_hdl);
        tal_system_sleep(2000);

        if (THREAD_STATE_STOP == tal_thread_get_state(mutex2_thrd_hdl)) {
            break;
        }
    }

    mutex2_thrd_hdl = NULL;
    PR_DEBUG("thread mutex2_thrd_hdl will delete");

    return;
}

/**
 * @brief mutex example
 *
 * @param[in] :
 *
 * @return none
 */
void example_mutex()
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == mutex_hdl) {
        TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&mutex_hdl), __EXIT);
    }

    THREAD_CFG_T thread_cfg = {
        .thrdname = "example_mutex1",
        .stackDepth = 4096,
        .priority = THREAD_PRIO_2,
    };
    if (NULL == mutex1_thrd_hdl) {
        TUYA_CALL_ERR_GOTO(tal_thread_create_and_start(&mutex1_thrd_hdl, NULL, NULL, __mutex_task1, NULL, &thread_cfg),
                           __EXIT);
    }

    thread_cfg.thrdname = "example_mutex2";
    if (NULL == mutex2_thrd_hdl) {
        TUYA_CALL_ERR_GOTO(tal_thread_create_and_start(&mutex2_thrd_hdl, NULL, NULL, __mutex_task2, NULL, &thread_cfg),
                           __EXIT);
    }
__EXIT:
    return;
}

/**
 * @brief stop mutex example, delete thread, mutex
 *
 * @param[in] :
 *
 * @return none
 */
void example_mutex_stop()
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL != mutex1_thrd_hdl) {
        TUYA_CALL_ERR_LOG(tal_thread_delete(mutex1_thrd_hdl));
    }

    if (NULL != mutex2_thrd_hdl) {
        TUYA_CALL_ERR_LOG(tal_thread_delete(mutex2_thrd_hdl));
    }

    /* Wait for the thread deletion to complete before releasing the queue resources. This prevents errors that may
     * occur if the queue is released while the thread is not yet deleted. */
    while (NULL != mutex1_thrd_hdl || NULL != mutex2_thrd_hdl) {
        tal_system_sleep(500);
    }

    if (NULL != mutex_hdl) {
        tal_mutex_release(mutex_hdl);
        mutex_hdl = NULL;
        PR_DEBUG("mutex_hdl free finish");
    }

    return;
}

/**
 * @brief user_main
 *
 * @return void
 */
void user_main(void)
{
    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    /* create mutex and create thread lock&unlock thread*/
    example_mutex();

    tal_system_sleep(2000);

    /* release thread & mutex */
    example_mutex_stop();

    return;
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif