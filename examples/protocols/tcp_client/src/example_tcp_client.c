/**
 * @file example_tcp_client.c
 * @brief Demonstrates the implementation of a TCP client using Tuya SDK.
 *
 * This file contains the implementation of a simple TCP client that connects to a specified TCP server, sends a
 * predefined message multiple times, and then closes the connection. It showcases the use of Tuya's application layer
 * (TAL) APIs for network communication, including socket creation, connection, data transmission, and socket closure.
 * Additionally, it demonstrates handling network link status changes and initializing the network manager for different
 * connection types (WiFi, Wired).
 *
 * Key operations demonstrated in this file:
 * - Initialization of network manager and connection to a network.
 * - Creation of a TCP socket and connection to a TCP server.
 * - Sending data over the TCP connection.
 * - Handling network link status changes.
 * - Cleanup and resource management upon task completion.
 *
 * This example serves as a practical guide for developers looking to understand how to implement network communication
 * in their Tuya SDK-based IoT applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tal_network.h"
#include "netmgr.h"
#include "tkl_output.h"
#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
#include "lwip_init.h"
#endif
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "netconn_wired.h"
#endif
/***********************************************************
************************macro define************************
***********************************************************/
#define TCP_SERVER_IP   "127.0.0.1"
#define TCP_SERVER_PORT 7

#ifdef ENABLE_WIFI
#define DEFAULT_WIFI_SSID "your-ssid-****"
#define DEFAULT_WIFI_PSWD "your-pswd-****"
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE tcp_client = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief tcp client task, close the tcp client after sending 5 times.
 *
 * @param[in] :
 *
 * @return none
 */
static void __tcp_client_task(void *args)
{
    // OPERATE_RET rt = OPRT_OK;
    int sock_fd;
    TUYA_IP_ADDR_T server_ip;
    TUYA_ERRNO net_errno = 0;
    uint8_t cnt = 0;
    char send_buf[] = {"Hello Tuya\n"};

    /* wait connect network */
    PR_NOTICE("--- start connect the tcp server %s port %d", TCP_SERVER_IP, TCP_SERVER_PORT);

    /* TCP server */
    sock_fd = tal_net_socket_create(PROTOCOL_TCP);

    server_ip = tal_net_str2addr(TCP_SERVER_IP);
    PR_DEBUG("connect tcp server ip: %s, port: %d", TCP_SERVER_IP, TCP_SERVER_PORT);
    net_errno = tal_net_connect(sock_fd, server_ip, TCP_SERVER_PORT);
    if (net_errno < 0) {
        PR_ERR("connect fail, exit");
        goto __EXIT;
    }

    for (;;) {
        net_errno = tal_net_send(sock_fd, send_buf, strlen(send_buf));
        if (net_errno < 0) {
            PR_ERR("send fail, exit");
            break;
        }

        cnt++;
        if (cnt >= 5) {
            break;
        }
        tal_system_sleep(2000);
    }

__EXIT:
    tal_net_close(sock_fd);

    tal_thread_delete(tcp_client);
    tcp_client = NULL;

    return;
}

/**
 * @brief
 *
 * @param data
 * @return OPERATE_RET
 */
OPERATE_RET __link_status_cb(void *data)
{
    OPERATE_RET rt = OPRT_OK;
    netmgr_status_e status = (netmgr_status_e)data;
    if (NETMGR_LINK_DOWN == status)
        return OPRT_OK;

    THREAD_CFG_T thread_cfg = {0};
    thread_cfg.stackDepth = 1024 * 4;
    thread_cfg.priority = THREAD_PRIO_2;
    thread_cfg.thrdname = "eg_tcp_client";
    if (NULL == tcp_client) {
        TUYA_CALL_ERR_RETURN(
            tal_thread_create_and_start(&tcp_client, NULL, NULL, __tcp_client_task, NULL, &thread_cfg));
    }

    return rt;
}

/**
 * @brief tcp client example
 *
 * @param[in] :
 *
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
    tal_event_subscribe(EVENT_LINK_STATUS_CHG, "tcp_client", __link_status_cb, SUBSCRIBE_TYPE_NORMAL);

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    TUYA_LwIP_Init();
#endif

    // network init
    netmgr_type_e type = 0;
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    type |= NETCONN_WIFI;
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    type |= NETCONN_WIRED;
#endif
    netmgr_init(type);

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    // connect wifi
    netconn_wifi_info_t wifi_info = {0};
    strcpy(wifi_info.ssid, DEFAULT_WIFI_SSID);
    strcpy(wifi_info.pswd, DEFAULT_WIFI_PSWD);
    netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info);
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