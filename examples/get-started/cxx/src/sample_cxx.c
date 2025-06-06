
/**
 * @file sample_cxx.c
 * @brief Simple demonstration of a c++ program for SDK.
 *
 * This file contains the implementation of a basic c++ program
 * designed for Tuya IoT projects. It demonstrates the initialization of the
 * Tuya Abstract Layer (TAL) logging system, a simple loop to print debug
 * messages, and the use of a task thread for executing the main logic. The
 * program showcases fundamental concepts such as logging, task creation, and
 * conditional compilation based on the operating system. It serves as a
 * starting point for developers new to Tuya's IoT platform, illustrating how to
 * structure a simple application and utilize Tuya's SDK for basic operations.
 *
 * The code is structured to support different execution paths based on the
 * operating system, demonstrating the portability and flexibility of Tuya's IoT
 * SDK across various platforms.
 *
 * @note This example is designed for educational purposes and may need to be
 * adapted for production environments.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include "tal_api.h"
#include "tal_cli.h"
#include "tkl_output.h"

/**
 * @brief user_main
 *
 * @return int
 */
int user_main()
{
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

    PR_DEBUG("Start cxx sample test\r\n");

    int cnt = 0;

    while (1) {
        func_adder(cnt);
        cnt++;
        tal_system_sleep(1000);
    }
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
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif