/**
 * @file example_queue.c
 * @brief Demonstrates queue operations in Tuya SDK.
 *
 * This file provides an example of how to use queues for inter-thread communication in applications developed using the
 * Tuya SDK. It includes creating and initializing a queue, posting messages to the queue, fetching messages from the
 * queue, and cleaning up the queue and threads upon completion. The example aims to showcase the use of queues for
 * synchronizing data exchange between producer and consumer threads in a multi-threaded environment.
 *
 * Key operations demonstrated:
 * - Creation and initialization of a queue.
 * - Posting data to the queue from a producer thread.
 * - Fetching data from the queue in a consumer thread.
 * - Proper deletion of threads and freeing of queue resources to prevent memory leaks.
 *
 * This example is intended for developers looking to understand the basics of queue management in Tuya's IoT
 * applications, providing a foundation for building robust and efficient multi-threaded applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tal_queue.h"
#include "tkl_output.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define MSG_SIZE 16

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
static THREAD_HANDLE fetch_thrd_hdl = NULL;
static QUEUE_HANDLE queue_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief queue post task
 *
 * @param[in] :
 *
 * @return none
 */
static void __queue_post_task(void *args)
{
    OPERATE_RET rt = OPRT_OK;
    char post_data[MSG_SIZE] = {"hello tuya\r\n"};

    for (;;) {
        TUYA_CALL_ERR_LOG(tal_queue_post(queue_hdl, post_data, 0xFFFFFFFF));
        PR_DEBUG("post queue");

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
 * @brief queue fetch task
 *
 * @param[in] :
 *
 * @return none
 */
static void __queue_fetch_task(void *args)
{
    OPERATE_RET rt = OPRT_OK;
    char fetch_data[MSG_SIZE] = {0};

    for (;;) {
        TUYA_CALL_ERR_LOG(tal_queue_fetch(queue_hdl, fetch_data, 0xFFFFFFFF));
        PR_DEBUG("get queue, data: %s", fetch_data);

        if (THREAD_STATE_STOP == tal_thread_get_state(fetch_thrd_hdl)) {
            break;
        }
    }

    fetch_thrd_hdl = NULL;
    PR_DEBUG("thread fetch_thrd_hdl will delete");

    return;
}

/**
 * @brief queue example
 *
 * @param[in] :
 *
 * @return none
 */
void example_queue()
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == queue_hdl) {
        TUYA_CALL_ERR_GOTO(tal_queue_create_init(&queue_hdl, MSG_SIZE * sizeof(char), 2), __EXIT);
    }

    THREAD_CFG_T thread_cfg = {0};
    thread_cfg.stackDepth = 1024 * 4;
    thread_cfg.priority = THREAD_PRIO_2;
    thread_cfg.thrdname = "queue_post";
    if (NULL == post_thrd_hdl) {
        TUYA_CALL_ERR_GOTO(
            tal_thread_create_and_start(&post_thrd_hdl, NULL, NULL, __queue_post_task, NULL, &thread_cfg), __EXIT);
    }

    thread_cfg.thrdname = "queue_fetch";
    if (NULL == fetch_thrd_hdl) {
        TUYA_CALL_ERR_GOTO(
            tal_thread_create_and_start(&fetch_thrd_hdl, NULL, NULL, __queue_fetch_task, NULL, &thread_cfg), __EXIT);
    }

__EXIT:
    return;
}

/**
 * @brief stop queue example, delete thread, queue
 *
 * @param[in] :
 *
 * @return none
 */
void example_queue_stop()
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL != fetch_thrd_hdl) {
        TUYA_CALL_ERR_LOG(tal_thread_delete(fetch_thrd_hdl));
    }

    if (NULL != post_thrd_hdl) {
        TUYA_CALL_ERR_LOG(tal_thread_delete(post_thrd_hdl));
    }

    /* Wait for thread deletion to complete, then release queue resources. Avoid errors where threads are not yet
     * deleted but queue is already released */
    while (NULL != post_thrd_hdl || NULL != fetch_thrd_hdl) {
        tal_system_sleep(500);
    }

    if (NULL != queue_hdl) {
        tal_queue_free(queue_hdl);
        queue_hdl = NULL;
        PR_DEBUG("queue_hdl free finish");
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

    /* create and post&fetch queue */
    example_queue();

    tal_system_sleep(2000);

    /* release queue */
    example_queue_stop();

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