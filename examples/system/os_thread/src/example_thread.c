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

#if defined(BOARD_ENABLE_GD32H77X) && (BOARD_ENABLE_GD32H77X == 1)
#include "gd32h77x_task_mem.h"

#define GD32H77X_DTCM_START UINT32_C(0x20000000)
#define GD32H77X_DTCM_END   UINT32_C(0x20040000)

/**
 * @brief Verify that TAL-created task stacks use the H77X DTCM task arena.
 *
 * The address of a volatile local variable must lie in the current task's
 * stack. On H77X that stack is allocated by the transparent DTCM-first task
 * allocator, so this provides a small end-to-end check without exposing the
 * allocator to application code.
 */
static bool gd32h77x_thread_memory_self_test(void)
{
    volatile uint32_t marker = UINT32_C(0x5A5A5A5A);
    uintptr_t stack_address = (uintptr_t)&marker;
    gd32h77x_task_mem_stats_t stats = {0};

    gd32h77x_task_mem_get_stats(&stats);

    bool in_dtcm = stack_address >= GD32H77X_DTCM_START &&
                   stack_address < GD32H77X_DTCM_END;
    bool has_dtcm_task_allocation = stats.dtcm_allocations > 0U;

    PR_NOTICE("H77X thread memory test: stack=0x%08lx total=%u free=%u used=%u dtcm_alloc=%u axisram_fallback=%u",
              (unsigned long)stack_address,
              (unsigned)stats.total_bytes,
              (unsigned)stats.free_bytes,
              (unsigned)stats.used_bytes,
              (unsigned)stats.dtcm_allocations,
              (unsigned)stats.axisram_fallbacks);

    if (!in_dtcm || !has_dtcm_task_allocation) {
        PR_ERR("H77X thread memory test FAIL: stack is not in DTCM or task allocator was not used");
        return false;
    }

    PR_NOTICE("H77X thread memory test PASS");
    return true;
}
#endif

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

#if defined(BOARD_ENABLE_GD32H77X) && (BOARD_ENABLE_GD32H77X == 1)
    (void)gd32h77x_thread_memory_self_test();
#endif

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