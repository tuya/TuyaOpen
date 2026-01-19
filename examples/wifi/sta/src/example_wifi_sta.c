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
#include "tal_network.h"
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tal_wifi.h"
#include "tal_system.h"     
#include "tkl_output.h"
#include <string.h>

/***********************************************************
 *                    Macro Definitions
 ***********************************************************/
#define DEMO_SERVER_IP            "192.168.201.114"
#define DEMO_SERVER_PORT          8080
#define DEMO_CONNECT_TIMEOUT_MS   3000

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE g_socket_demo_thread = NULL;
static volatile bool g_demo_stop = false;

/***********************************************************
 *                    Forward Declarations
 ***********************************************************/
static void wifi_event_callback(WF_EVENT_E event, void *arg);
static void socket_demo_thread(void *arg);
static void start_socket_demo(void);
static void stop_socket_demo(void);
static int  send_all(int fd, const uint8_t *buf, int len);

/***********************************************************
 *                    Helper Functions
 ***********************************************************/
/**
 * @brief send_all
 * @param[in] fd socket descriptor
 * @param[in] buf data buffer
 * @param[in] len total length
 * @return <0 error; >=0 sent length
 */
static int send_all(int fd, const uint8_t *buf, int len)
{
    int sent = 0;
    while (sent < len) {
        int r = tal_net_send(fd, buf + sent, len - sent);
        if (r <= 0) {
            return r;
        }
        sent += r;
    }
    return sent;
}

/**
 * @brief socket_demo_thread
 * @param[in] arg unused
 * @return none
 */
static void socket_demo_thread(void *arg)
{
    (void)arg;
    PR_NOTICE("TCP demo thread started");

    int fd = -1;
    char recv_buf[128];
    TUYA_IP_ADDR_T server_addr = 0;
    TUYA_ERRNO conn_ret = 0;

    fd = tal_net_socket_create(PROTOCOL_TCP);
    if (fd < 0) {
        PR_ERR("socket create failed");
        goto __EXIT;
    }

    server_addr = tal_net_str2addr(DEMO_SERVER_IP);
    if (server_addr == 0) {
        PR_ERR("invalid server ip");
        goto __CLEAN_SOCKET;
    }

    conn_ret = tal_net_connect(fd, server_addr, DEMO_SERVER_PORT);
    if (conn_ret != 0) {
        PR_ERR("connect failed errno=%d", conn_ret);
        goto __CLEAN_SOCKET;
    }
    PR_NOTICE("connected to %s:%d", DEMO_SERVER_IP, DEMO_SERVER_PORT);

    const char *msg = "Hello, this is a test message from the device.";
    if (send_all(fd, (const uint8_t *)msg, (int)strlen(msg)) < 0) {
        PR_ERR("send failed");
        goto __CLEAN_SOCKET;
    }
    PR_NOTICE("send ok");

    memset(recv_buf, 0, sizeof(recv_buf));
    while (!g_demo_stop) {
        int r = tal_net_recv(fd, recv_buf, sizeof(recv_buf) - 1);
        if (r > 0) {
            recv_buf[r] = '\0';
            PR_NOTICE("recv len=%d data=%s", r, recv_buf);
            break; /* end after first valid data */
        } else if (r == 0) {
            PR_NOTICE("peer closed connection");
            break;
        } else { /* r < 0 */
            /* Simple wait and retry; could add timeout or error classification */
            tal_system_sleep(200);
            continue;
        }
    }

__CLEAN_SOCKET:
    if (fd >= 0) {
        tal_net_close(fd);
    }
__EXIT:
    PR_NOTICE("TCP demo thread exit");
    g_socket_demo_thread = NULL;
    tal_thread_delete(g_socket_demo_thread);
}

/**
 * @brief start_socket_demo
 * @return none
 */
static void start_socket_demo(void)
{
    if (g_socket_demo_thread) {
        return;
    }
    g_demo_stop = false;
    THREAD_CFG_T cfg = {0};
    cfg.stackDepth = 1024 * 4;
    cfg.priority = THREAD_PRIO_1;
    cfg.thrdname = "socket_demo";
    tal_thread_create_and_start(&g_socket_demo_thread, NULL, NULL, socket_demo_thread, NULL, &cfg);
}

/**
 * @brief stop_socket_demo
 * @return none
 */
static void stop_socket_demo(void)
{
    g_demo_stop = true;
}

/***********************************************************
 *                    WiFi Event Callback
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
    case WFE_CONNECTED: 
        {
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

            start_socket_demo();

            break;
        }

    case WFE_CONNECT_FAILED: 
        {
            PR_DEBUG("connection fail!");
            break;
        }
    case WFE_DISCONNECTED: 
        {
            PR_DEBUG("WiFi disconnected");
            stop_socket_demo();
            break;
        }
    default:
        break;
    }
}

/***********************************************************
 *                    Main User Entry
 ***********************************************************/
/**
 * @brief WiFi STA task
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;
    char connect_ssid[] = "Baiming"; // connect wifi ssid
    char connect_pswd[] = "123456789"; // connect wifi password

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
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
