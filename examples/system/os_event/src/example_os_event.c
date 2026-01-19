/**
 * @file example_os_event.c
 * @brief Demonstrates the usage of Tuya's OS event handling mechanisms.
 *
 * This file contains the implementation of a simple event-driven system using Tuya's OS abstraction layer.
 * It showcases how to subscribe to events, publish events, and handle them in a multi-threaded environment.
 * The example includes the creation of threads, publishing of "thread start" and "thread finish" events, and the
 * subscription to these events with callback functions. It serves as a practical guide for developers to understand and
 * implement event-driven programming in Tuya IoT applications.
 *
 * The code demonstrates:
 * - Initialization of the logging system.
 * - Subscription to events with callback functions.
 * - Publishing events from thread start and finish callbacks.
 * - Creation and deletion of threads to trigger events.
 *
 * This example is designed to run on Tuya's IoT platform, utilizing its OS abstraction layer for thread management and
 * event handling, making it portable across different hardware platforms supported by Tuya.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
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
#define EVENT_THREAD_ENTER "enter_thread"
#define EVENT_THREAD_EXIT  "exit_thread"

/***********************************************************
***********************function define**********************
***********************************************************/

static OPERATE_RET subscribe_thread_start_cb(void *data)
{
    PR_DEBUG("---> thread start");
    return OPRT_OK;
}

static OPERATE_RET subscribe_thread_finish_cb(void *data)
{
    PR_DEBUG("---> thread finish");
    return OPRT_OK;
}

static void thread_start_cb()
{
    tal_event_publish(EVENT_THREAD_ENTER, NULL);
}

static void thread_finish_cb()
{
    tal_event_publish(EVENT_THREAD_EXIT, NULL);
}

static THREAD_HANDLE example_thrd_hdl = NULL;
static void example_task(void *args)
{
    PR_NOTICE("thread running!");
}

/**
 * @brief user_main
 *
 * @return void
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

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

    /* event subscribe */
    tal_event_subscribe(EVENT_THREAD_ENTER, "thread_start_cb", subscribe_thread_start_cb, SUBSCRIBE_TYPE_NORMAL);
    tal_event_subscribe(EVENT_THREAD_EXIT, "thread_finish_cb", subscribe_thread_finish_cb, SUBSCRIBE_TYPE_NORMAL);

    /* a thread will publish event */
    THREAD_CFG_T thread_cfg = {0};
    thread_cfg.stackDepth = 1024 * 4;
    thread_cfg.priority = THREAD_PRIO_2;
    thread_cfg.thrdname = "example_task";
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&example_thrd_hdl, thread_start_cb, thread_finish_cb, example_task,
                                                  NULL, &thread_cfg));

    /* delete thread */
    tal_thread_delete(example_thrd_hdl);
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