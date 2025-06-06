/**
 * @file example_watchdog.c
 * @brief Watchdog example for SDK.
 *
 * This file demonstrates the implementation of a watchdog timer using the Tuya SDK.
 * It includes initializing the watchdog, setting the watchdog interval, and periodically refreshing the watchdog to
 * prevent system resets. The example covers basic watchdog operations such as initialization, configuration, and
 * refresh in a loop, with a maximum refresh count to eventually allow the watchdog to reset the system. This serves as
 * a basic template for implementing watchdog functionality in Tuya IoT projects to enhance system reliability.
 *
 * The example also includes conditional compilation to support different operating systems, showcasing a basic approach
 * to creating portable IoT applications with Tuya SDK.
 *
 * @note This example is designed for educational purposes and may need to be adapted for production environments.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_watchdog.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define MAX_WD_REFRESH_CNT 10

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static uint32_t g_wd_refresh_cnt = 0;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief user_main
 *
 * @return none
 */
void user_main()
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

    /* watchdog init */
    PR_DEBUG("init watchdog, interval: 60 sec");
    TUYA_WDOG_BASE_CFG_T cfg;
    cfg.interval_ms = 60 * 1000;
    uint32_t refreh_intv = tkl_watchdog_init(&cfg);
    refreh_intv = refreh_intv >= 60000 ? refreh_intv : 60000;

    /* Feed watchdog */
    while (1) {
        tal_system_sleep(refreh_intv / 3);
        TUYA_CALL_ERR_LOG(tkl_watchdog_refresh());
        if (++g_wd_refresh_cnt > MAX_WD_REFRESH_CNT) {
            break;
        } else {
            PR_DEBUG("refresh watchdog, interval: %d sec", refreh_intv / (3 * 1000));
        }
    }

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
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif