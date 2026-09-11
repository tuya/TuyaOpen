/**
 * @file example_thread.c
 * @brief Demonstrates thread creation and management in Tuya SDK applications.
 *
 * This file provides an example of creating and managing threads using the Tuya SDK.
 * It includes the creation of a simple thread that executes a task a specified number of times before terminating.
 * The example demonstrates how to properly initialize resources, create a thread, execute a task within the thread, and
 * clean up resources upon completion. It also showcases conditional compilation techniques for different operating
 * systems, ensuring broad compatibility.
 *
 * Key operations demonstrated in this file:
 * - Initialization of the Tuya SDK logging system for debugging.
 * - Creation and starting of a thread to perform a specific task.
 * - Use of a loop within the thread function to perform repetitive tasks.
 * - Proper termination and cleanup of the thread and associated resources.
 *
 * This example is intended for developers looking to understand thread management in Tuya SDK-based IoT applications,
 * providing a foundation for building multi-threaded applications.
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
static THREAD_HANDLE example_thrd_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief thread example task
 *
 * @param[in] :
 *
 * @return none
 */
static void example_task(void *args)
{
    uint8_t cnt = 0;
    PR_NOTICE("example task is run...");
    for (;;) {
        PR_NOTICE("this is example task");
        tal_system_sleep(2000);

        cnt++;
        if (cnt >= 5) {
            break;
        }
    }

    /* Do not delete threads asynchronously, as it may not immediately remove the thread */
    PR_NOTICE("example task will delete");
    tal_thread_delete(example_thrd_hdl);

    return;
}

/* PSRAM-stack probe: mirrors how the lvgl_v9 thread runs (psram_mode=1).
 * Touch the stack heavily each loop; if PSRAM stacks misbehave on this SoC
 * the thread stalls/faults here instead of inside LVGL. */
static THREAD_HANDLE sg_psram_probe_hdl = NULL;

static void psram_stack_probe_task(void *args)
{
    volatile uint32_t buf[256]; /* ~1KB on the PSRAM stack */
    uint32_t loop = 0;
    PR_NOTICE("PSRAM-stack probe start, buf@%p", buf);
    for (;;) {
        uint32_t sum = 0;
        for (int i = 0; i < 256; i++) {
            buf[i] = loop + i;
        }
        for (int i = 0; i < 256; i++) {
            sum += buf[i];
        }
        PR_NOTICE("PSRAM-stack probe loop=%u sum=%u", loop, sum);
        loop++;
        tal_system_sleep(1000);
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

    /* thread create and start */
    THREAD_CFG_T thread_cfg = {0};
    thread_cfg.stackDepth = 1024 * 4;
    thread_cfg.priority = THREAD_PRIO_2;
    thread_cfg.thrdname = "example_task";
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&example_thrd_hdl, NULL, NULL, example_task, NULL, &thread_cfg));

    /* PSRAM-stack probe: same psram_mode=1 + 8KB stack as the lvgl_v9 thread */
    THREAD_CFG_T psram_cfg = {0};
    psram_cfg.stackDepth = 1024 * 8;
    psram_cfg.priority   = THREAD_PRIO_2;
    psram_cfg.thrdname   = "psram_probe";
    psram_cfg.psram_mode = 1;
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_psram_probe_hdl, NULL, NULL,
                                                  psram_stack_probe_task, NULL, &psram_cfg));

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