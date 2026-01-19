/**
 * @file example_wifi_ap.c
 * @brief WiFi AP (Access Point) example for SDK
 *
 * This file demonstrates the setup and management of a WiFi Access Point using the Tuya IoT SDK.
 * It includes initializing the WiFi system, configuring the Access Point with SSID and password, starting the AP mode,
 * and handling WiFi events. Additionally, it showcases how to implement a UDP broadcast over the WiFi network, allowing
 * devices to discover each other or communicate simple messages. This example is particularly useful for creating local
 * networks for IoT devices to interact without requiring an external router.
 *
 * The code is structured to support conditional compilation, making it adaptable for different operating systems
 * supported by the Tuya IoT SDK. It provides a practical example of using Tuya's APIs to manage network connectivity
 * and perform basic network communication tasks.
 *
 * @note This example is designed for educational purposes and may need to be adapted for production environments.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
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
#define DEFAULT_WIFI_CHANNEL 5

#define AP_SSID   "my-wifi"
#define AP_PASSWD "12345678"

#define AP_IP   "192.168.1.123"
#define AP_MASK "255.255.255.0"
#define AP_GW   "192.168.1.1"

// udp broadcast
#define UDP_BROADCAST_PORT 6666

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
TIMER_ID timer_id = NULL;
int udp_broadcast_fd = 0;

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
    PR_DEBUG("-------------event callback-------------");
}

void udp_broadcast_func(TIMER_ID timer_id, void *arg)
{
    char send_data[] = "hello world";
    TUYA_IP_ADDR_T broadcast_addr = 0xFFFFFFFF;
    tal_net_send_to(udp_broadcast_fd, send_data, strlen(send_data), broadcast_addr, UDP_BROADCAST_PORT);
    PR_DEBUG("send data:%s", send_data);

    return;
}

/**
 * @brief WiFi AP task
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

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

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    TUYA_LwIP_Init();
#endif

    const char ap_ssid[] = AP_SSID;       // ssid
    const char ap_password[] = AP_PASSWD; // password
    NW_IP_S nw_ip = {
        .ip = AP_IP,
        .mask = AP_MASK,
        .gw = AP_GW,
    };

    PR_NOTICE("------ wifi ap example start ------");

    /*wifi init*/
    TUYA_CALL_ERR_GOTO(tal_wifi_init(wifi_event_callback), __EXIT);

    /*Set WiFi mode to AP*/
    TUYA_CALL_ERR_LOG(tal_wifi_set_work_mode(WWM_SOFTAP));

    WF_AP_CFG_IF_S wifi_cfg = {
        .s_len = strlen(ap_ssid),     // ssid length
        .p_len = strlen(ap_password), // password length
        .chan = DEFAULT_WIFI_CHANNEL, // wifi channel
        .md = WAAM_WPA2_PSK,          // encryption type
        .ip = nw_ip,                  // ip information
        .ms_interval = 100,           // broadcast interval,Unit(ms)
        .max_conn = 3                 // max sta connect numbers
    };

    strcpy((char *)wifi_cfg.ssid, ap_ssid);       // ssid
    strcpy((char *)wifi_cfg.passwd, ap_password); // password
    TUYA_CALL_ERR_LOG(tal_wifi_ap_start(&wifi_cfg));

    /* udp broadcast */
#if 1
    int status = 0;
    udp_broadcast_fd = tal_net_socket_create(PROTOCOL_UDP);
    if (udp_broadcast_fd < 0) {
        PR_ERR("create udp socket failed");
        goto __EXIT;
    }

    TUYA_IP_ADDR_T ip_addr = tal_net_str2addr(AP_IP);
    PR_DEBUG("ip_addr:%d", ip_addr);

    status = tal_net_bind(udp_broadcast_fd, ip_addr, UDP_BROADCAST_PORT);
    if (status < 0) {
        PR_ERR("bind fail:%d", tal_net_get_errno());
        goto __EXIT;
    }

    tal_net_set_broadcast(udp_broadcast_fd);

    tal_sw_timer_create(udp_broadcast_func, NULL, &timer_id);
    tal_sw_timer_start(timer_id, 5 * 1000, TAL_TIMER_CYCLE);
#endif

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
