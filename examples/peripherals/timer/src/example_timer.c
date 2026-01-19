/**
 * @file example_timer.c
 * @brief Timer driver example for SDK.
 *
 * This file demonstrates the implementation of a timer driver using Tuya SDK APIs.
 * It includes the initialization of a timer, setting up a timer callback function, and a user main function that
 * initializes and starts the timer. The timer is configured to trigger a callback function periodically, which
 * increments a counter and stops and deinitializes the timer after a certain number of ticks. This example is intended
 * to illustrate how to use timers in Tuya IoT projects for scheduling tasks or creating delays.
 *
 * The example also includes conditional compilation to support different operating systems, showcasing a basic approach
 * to creating portable IoT applications with Tuya SDK.
 *
 * @note This example is designed for educational purposes and may need to be adapted for production environments.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_timer.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define DELAY_TIME 500 * 1000 // us

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static char sg_count = 0;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Timer callback function
 *
 * @param[in] arg:parameters
 * @return none
 */
static void __timer_callback(void *args)
{
    /* TAL_PR_ , PR_ , these two types of prints have locks inside, do not use them in interrupts */
    tkl_log_output("\r\n------------- Timer Callback --------------\r\n");
    sg_count++;

    if (sg_count >= 5) {
        sg_count = 0;
        tkl_timer_stop(EXAMPLE_TIMER_ID);
        tkl_timer_deinit(EXAMPLE_TIMER_ID);
        tkl_log_output("\r\ntimer %d is stop\r\n", EXAMPLE_TIMER_ID);
    }

    return;
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

    /* timer init */
    TUYA_TIMER_BASE_CFG_T sg_timer_cfg = {.mode = TUYA_TIMER_MODE_PERIOD, .args = NULL, .cb = __timer_callback};
    TUYA_CALL_ERR_GOTO(tkl_timer_init(EXAMPLE_TIMER_ID, &sg_timer_cfg), __EXIT);

    /*start timer*/
    TUYA_CALL_ERR_GOTO(tkl_timer_start(EXAMPLE_TIMER_ID, DELAY_TIME), __EXIT);
    PR_NOTICE("timer %d is start", EXAMPLE_TIMER_ID);

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