/**
 * @file example_sw_timer.c
 * @brief Demonstrates the usage of software timers in Tuya SDK applications.
 *
 * This file contains examples of how to initialize, create, start, and manage software timers using the Tuya SDK.
 * It showcases the creation of a software timer, setting a callback function for timer events, and using the timer to
 * perform periodic tasks within an application. Additionally, it demonstrates stopping and deleting a timer after a
 * certain number of events. This example is useful for developers looking to implement time-based operations in their
 * Tuya IoT applications.
 *
 * Key operations demonstrated in this file:
 * - Initialization of the Tuya SDK logging system.
 * - Creation and management of a software timer.
 * - Use of callback functions to handle timer events.
 * - Conditional stopping and deletion of timers based on application logic.
 *
 * This example serves as a practical guide for developers to understand and utilize software timers in their Tuya
 * SDK-based applications, enabling efficient and effective time-based task management.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tal_sw_timer.h"
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
static TIMER_ID sw_timer_id = NULL;
static uint8_t timer_run_cnt = 0;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief software timer callback
 *
 * @param[in] :
 *
 * @return none
 */
static void __timer_cb(TIMER_ID timer_id, void *arg)
{
    PR_NOTICE("--- tal sw timer callback");
    timer_run_cnt++;
    if (timer_run_cnt >= 5) {
        PR_NOTICE("stop and delete software timer");
        timer_run_cnt = 0;
        tal_sw_timer_stop(sw_timer_id);
        tal_sw_timer_delete(sw_timer_id);
    }
}

/**
 * @brief user_main
 *
 * @return none
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

    /* sw timer init & start */
    PR_DEBUG("sw timer init");
    TUYA_CALL_ERR_GOTO(tal_sw_timer_init(), __EXIT);

    PR_DEBUG("sw timer create");
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__timer_cb, NULL, &sw_timer_id), __EXIT);

    PR_DEBUG("sw timer start");
    TUYA_CALL_ERR_LOG(tal_sw_timer_start(sw_timer_id, 3000, TAL_TIMER_CYCLE));

__EXIT:
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