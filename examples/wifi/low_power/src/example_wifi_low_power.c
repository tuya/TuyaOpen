/**
 * @file example_wifi_low_power.c
 * @brief WiFi low power management example for SDK.
 *
 * This file demonstrates the implementation of low power management for WiFi modules in Tuya IoT projects.
 * It includes initializing the system for low power operation, managing WiFi low power states,
 * and creating tasks that periodically enable and disable WiFi low power mode to simulate real-world IoT device
 * behavior. The example showcases how to use the Tuya IoT SDK's APIs to reduce power consumption of IoT devices by
 * leveraging WiFi low power modes.
 *
 * The code is structured to support conditional compilation, making it adaptable for different operating systems
 * supported by the Tuya IoT SDK. This example provides a practical approach to implementing energy-efficient operations
 * in IoT devices, which is crucial for battery-powered applications.
 *
 * @note This example is designed for educational purposes and may need to be adapted for production environments.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tal_wifi.h"
#include "tal_network.h"
#include "tkl_output.h"
#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
#include "lwip_init.h"
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE __wifi_lp_hdl1 = NULL;
static THREAD_HANDLE __wifi_lp_hdl2 = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

static void __wifi_low_power_task1(void *arg)
{
    for (;;) {
        tal_wifi_lp_disable();
        /* exit low-power do something */
        tal_system_sleep(3000);
        tal_wifi_lp_enable();

        tal_system_sleep(5000);
    }
    tal_thread_delete(__wifi_lp_hdl1);
}

static void __wifi_low_power_task2(void *arg)
{
    for (;;) {
        tal_wifi_lp_disable();
        /* exit low-power do something */
        tal_system_sleep(3000);
        tal_wifi_lp_enable();

        tal_system_sleep(7000);
    }
    tal_thread_delete(__wifi_lp_hdl2);
}

void user_main(void)
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

    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();

    /*enable low power*/
    tal_cpu_set_lp_mode(TRUE);

    /* The higher the dtim, the lower the power consumption */
    tal_wifi_set_lps_dtim(2);
    /* enter tickless&WiFi low-power mode */
    tal_wifi_lp_enable();

    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_2;
    thrd_param.thrdname = "low_power_1";
    tal_thread_create_and_start(&__wifi_lp_hdl1, NULL, NULL, __wifi_low_power_task1, NULL, &thrd_param);

    thrd_param.thrdname = "low_power_2";
    tal_thread_create_and_start(&__wifi_lp_hdl2, NULL, NULL, __wifi_low_power_task2, NULL, &thrd_param);

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
