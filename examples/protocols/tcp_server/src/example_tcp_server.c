/**
 * @file example_tcp_server.c
 * @brief Demonstrates the implementation of a TCP server using Tuya SDK.
 *
 * This file contains the implementation of a simple TCP server that listens on a specified port, accepts client
 * connections, and echoes received messages back to the client. It showcases the use of Tuya's application layer (TAL)
 * APIs for network communication, including socket creation, binding, listening, accepting client connections, data
 * reception, and transmission. Additionally, it demonstrates handling network link status changes and initializing the
 * network manager for different connection types (WiFi, Wired).
 *
 * Key operations demonstrated in this file:
 * - Initialization of network manager and connection to a network.
 * - Creation of a TCP socket, binding it to a port, and listening for client connections.
 * - Accepting client connections and handling received data.
 * - Echoing received messages back to the client.
 * - Handling network link status changes.
 * - Cleanup and resource management upon server shutdown.
 *
 * This example serves as a practical guide for developers looking to understand how to implement a TCP server in their
 * Tuya SDK-based IoT applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tal_network.h"
#include "tkl_output.h"
#include "netmgr.h"
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
#define SERVER_PORT 1234

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
static THREAD_HANDLE tcp_server = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief tcp server task
 *
 * @param[in] :
 *
 * @return none
 */
static void __tcp_server_task(void *args)
{
    char recv_buf[255] = {0};
    int listen_fd, sock_fd;
    TUYA_ERRNO net_errno = 0;

    /* wait connect network */
    PR_NOTICE("--- start the tcp server at port %d", SERVER_PORT);

    /* TCP server */
    listen_fd = tal_net_socket_create(PROTOCOL_TCP);

    tal_net_bind(listen_fd, TY_IPADDR_ANY, SERVER_PORT);

    tal_net_listen(listen_fd, 1);

    TUYA_IP_ADDR_T client_ip = 0;
    uint16_t client_port = 0;
    sock_fd = tal_net_accept(listen_fd, &client_ip, &client_port);
    char *client_ip_str = tal_net_addr2str(client_ip);
    PR_DEBUG("accept client ip:%s, port:%d", client_ip_str, client_port);

    memset(recv_buf, 0, 255);
    snprintf(recv_buf, 255, "You can send \"stop\" to stop the tcp service.");
    tal_net_send(sock_fd, recv_buf, strlen(recv_buf));

    for (;;) {
        memset(recv_buf, 0, 255);
        net_errno = tal_net_recv(sock_fd, recv_buf, 255);
        PR_DEBUG("%s:%d==> %s", client_ip_str, client_port, recv_buf);
        if (0 == strcmp("stop", recv_buf) || net_errno < 0) {
            PR_NOTICE("--- stop the tcp server");
            break;
        }

        tal_net_send(sock_fd, recv_buf, strlen(recv_buf));
    }

    tal_net_close(sock_fd);
    tal_net_close(listen_fd);

    tal_thread_delete(tcp_server);
    tcp_server = NULL;

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
    thread_cfg.thrdname = "eg_tcp_server";
    if (NULL == tcp_server) {
        TUYA_CALL_ERR_RETURN(
            tal_thread_create_and_start(&tcp_server, NULL, NULL, __tcp_server_task, NULL, &thread_cfg));
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
    tal_event_subscribe(EVENT_LINK_STATUS_CHG, "tcp_server", __link_status_cb, SUBSCRIBE_TYPE_NORMAL);

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