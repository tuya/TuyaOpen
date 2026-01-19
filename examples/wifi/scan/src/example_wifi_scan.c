/**
 * @file example_wifi_scan.c
 * @brief WiFi scan example for SDK.
 *
 * This file demonstrates how to perform a WiFi scan using the Tuya IoT SDK.
 * It includes initializing the WiFi module, triggering a scan for available WiFi networks, and processing the scan
 * results. The example covers setting up a task for WiFi scanning, handling the scan results to list available
 * networks, and cleaning up resources upon completion. This example is useful for IoT devices that need to list or
 * connect to WiFi networks based on signal strength or other criteria.
 *
 * The code is structured to provide a clear example of using Tuya's WiFi APIs for scanning nearby WiFi networks, which
 * can be integrated into IoT solutions that require WiFi connectivity setup or network management functionalities.
 *
 * @note This example is designed for educational purposes and may need to be adapted for production environments.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tal_wifi.h"
#include "tkl_output.h"

/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief WiFi scanf task
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;
    AP_IF_S *ap_info;
    uint32_t ap_info_nums;
    int i = 0;
    char info_ssid[50];

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

    PR_NOTICE("------ wifi scan example start ------");

    /*Scan WiFi information in the current environment*/
    TUYA_CALL_ERR_GOTO(tal_wifi_all_ap_scan(&ap_info, &ap_info_nums), __EXIT);
    PR_DEBUG("Scanf to %d wifi signals", ap_info_nums);
    for (i = 0; i < ap_info_nums; i++) {
        strcpy((char *)info_ssid, (const char *)ap_info[i].ssid);
        PR_DEBUG("channel:%d, ssid:%s", ap_info[i].channel, info_ssid);
    }

    /*Release the acquired WiFi information in the current environment*/
    TUYA_CALL_ERR_LOG(tal_wifi_release_ap(ap_info));

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
