/**
 * @file example_wifi_sta.c
 * @brief WiFi Station (STA) mode example for SDK.
 *
 * This file demonstrates the setup and management of a device in WiFi Station mode using the Tuya IoT SDK.
 * It includes initializing the WiFi module, connecting to a specified WiFi network (SSID and password), handling WiFi
 * events (connection success, connection failure, disconnection), and maintaining a connection to the WiFi network. The
 * example covers the implementation of a WiFi event callback function, a main user task for initiating WiFi connection,
 * and conditional compilation for different operating systems to showcase portability across platforms supported by the
 * Tuya IoT SDK.
 *
 * The code aims to provide a clear example of managing WiFi connectivity in IoT devices, which is a fundamental
 * requirement for most IoT applications. It demonstrates how to use Tuya's APIs to connect to WiFi networks, handle
 * connection events, and ensure the device operates correctly in WiFi Station mode.
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
 * @brief wifi Related event callback function
 *
 * @param[in] event:event
 * @param[in] arg:parameter
 * @return none
 */
static void wifi_event_callback(WF_EVENT_E event, void *arg)
{
    OPERATE_RET op_ret = OPRT_OK;
    NW_IP_S sta_info;
    memset(&sta_info, 0, sizeof(NW_IP_S));

    PR_DEBUG("-------------event callback-------------");
    switch (event) {
    case WFE_CONNECTED: {
        PR_DEBUG("connection succeeded!");

        /* output ip information */
        op_ret = tal_wifi_get_ip(WF_STATION, &sta_info);
        if (OPRT_OK != op_ret) {
            PR_ERR("get station ip error");
            return;
        }
        PR_NOTICE("gw: %s", sta_info.gw);
        PR_NOTICE("ip: %s", sta_info.ip);
        PR_NOTICE("mask: %s", sta_info.mask);
        break;
    }

    case WFE_CONNECT_FAILED: {
        PR_DEBUG("connection fail!");
        break;
    }

    case WFE_DISCONNECTED: {
        PR_DEBUG("disconnect!");
        break;
    }
    }
}

/**
 * @brief WiFi STA task
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main()
{
    OPERATE_RET rt = OPRT_OK;
    char connect_ssid[] = "your-ssid-xxx"; // connect wifi ssid
    char connect_pswd[] = "your-pswd-xxx"; // connect wifi password

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

    PR_NOTICE("------ wifi station example start ------");

    /*WiFi init*/
    TUYA_CALL_ERR_GOTO(tal_wifi_init(wifi_event_callback), __EXIT);

    /*Set WiFi mode to station*/
    TUYA_CALL_ERR_GOTO(tal_wifi_set_work_mode(WWM_STATION), __EXIT);

    /*STA mode, connect to WiFi*/
    PR_NOTICE("\r\nconnect wifi ssid: %s, password: %s\r\n", connect_ssid, connect_pswd);
    TUYA_CALL_ERR_LOG(tal_wifi_station_connect((int8_t *)connect_ssid, (int8_t *)connect_pswd));

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
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
