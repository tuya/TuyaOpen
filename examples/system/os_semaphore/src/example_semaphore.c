/**
 * @file example_semaphore.c
 * @brief Demonstrates semaphore usage for synchronization in Tuya SDK applications.
 *
 * This file provides an example of how to use semaphores for thread synchronization in applications developed with the
 * Tuya SDK. It covers the creation and initialization of a semaphore, semaphore posting and waiting tasks, and the
 * proper cleanup of semaphore and thread resources. The example aims to showcase effective techniques for managing
 * concurrent operations in Tuya's IoT applications, ensuring thread-safe interactions with shared resources.
 *
 * Key concepts demonstrated in this example:
 * - Initialization and management of a semaphore.
 * - Creation of threads for posting to and waiting on the semaphore.
 * - Synchronization of threads using the semaphore to ensure orderly access to resources.
 * - Cleanup and deletion of threads and semaphore to prevent resource leaks.
 *
 * This example is intended to guide developers in implementing thread synchronization mechanisms in their Tuya
 * SDK-based IoT applications, promoting the development of reliable and efficient multi-threaded applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tal_semaphore.h"
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
static THREAD_HANDLE post_thrd_hdl = NULL;
static THREAD_HANDLE wait_thrd_hdl = NULL;
static SEM_HANDLE example_sem_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief semaphore post task
 *
 * @param[in] :
 *
 * @return none
 */
static void __sema_post_task(void *args)
{
    for (;;) {
        tal_semaphore_post(example_sem_hdl);
        PR_NOTICE("post semaphore");

        if (THREAD_STATE_STOP == tal_thread_get_state(post_thrd_hdl)) {
            break;
        }
        tal_system_sleep(2000);
    }

    post_thrd_hdl = NULL;
    PR_DEBUG("thread post_thrd_hdl will delete");

    return;
}

/**
 * @brief semaphore wait task
 *
 * @param[in] :
 *
 * @return none
 */
static void __sema_wait_task(void *args)
{
    for (;;) {
        tal_semaphore_wait(example_sem_hdl, SEM_WAIT_FOREVER);
        PR_NOTICE("get semaphore");

        if (THREAD_STATE_STOP == tal_thread_get_state(wait_thrd_hdl)) {
            break;
        }
    }

    wait_thrd_hdl = NULL;
    PR_DEBUG("thread __sema_wait_task will delete");

    return;
}

/**
 * @brief semaphore example
 *
 * @param[in] :
 *
 * @return none
 */
void example_semaphore()
{
    OPERATE_RET rt = OPRT_OK;
    if (NULL == example_sem_hdl) {
        TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&example_sem_hdl, 0, 1), __EXIT);
    }

    THREAD_CFG_T thread_cfg = {0};
    thread_cfg.stackDepth = 1024 * 4;
    thread_cfg.priority = THREAD_PRIO_2;
    thread_cfg.thrdname = "sema_post";
    if (NULL == post_thrd_hdl) {
        TUYA_CALL_ERR_GOTO(tal_thread_create_and_start(&post_thrd_hdl, NULL, NULL, __sema_post_task, NULL, &thread_cfg),
                           __EXIT);
    }

    thread_cfg.thrdname = "sema_wait";
    if (NULL == wait_thrd_hdl) {
        TUYA_CALL_ERR_GOTO(tal_thread_create_and_start(&wait_thrd_hdl, NULL, NULL, __sema_wait_task, NULL, &thread_cfg),
                           __EXIT);
    }

__EXIT:
    return;
}

/**
 * @brief stop semaphore example, delete thread semaphore
 *
 * @param[in] :
 *
 * @return none
 */
void example_semaphore_stop()
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL != wait_thrd_hdl) {
        TUYA_CALL_ERR_LOG(tal_thread_delete(wait_thrd_hdl));
    }

    if (NULL != post_thrd_hdl) {
        TUYA_CALL_ERR_LOG(tal_thread_delete(post_thrd_hdl));
    }

    /* Wait for thread deletion to complete, then release queue resources. Avoid errors where threads are not yet
     * deleted but queue is already released */
    while (NULL != post_thrd_hdl || NULL != wait_thrd_hdl) {
        tal_system_sleep(500);
    }

    if (NULL != example_sem_hdl) {
        tal_semaphore_release(example_sem_hdl);
        example_sem_hdl = NULL;
        PR_DEBUG("example_sem_hdl free finish");
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

    /* create semaphore and create thread wait&post semaphore */
    example_semaphore();

    tal_system_sleep(2000);

    /* release thread and semaphore */
    example_semaphore_stop();

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