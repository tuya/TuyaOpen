/**
 * @file example_http_client_post.c
 * @brief HTTP POST client example - Network and HTTP request handling
 *
 * This file handles network initialization, Wi-Fi connection, and HTTP POST requests.
 * UI-related code is separated into ui_http_client_post.c
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tuya_cloud_types.h"
#include "http_client_interface.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "netmgr.h"
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "netconn_wired.h"
#endif

#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)
#include "ui.h"
#endif

/***********************************************************
*********************** macro define ***********************
***********************************************************/
// Server configuration - HTTP server settings
#define SERVER_HOST "your-server-ip"  // Server IP address, modify according to your actual server
#define SERVER_PORT 8080               // Server port number
#define SERVER_PATH "/api/random"      // Server API endpoint path

#ifdef ENABLE_WIFI
// Wi-Fi configuration - Wi-Fi network settings
#define DEFAULT_WIFI_SSID "your-ssid"        // Wi-Fi network name (SSID), modify according to your actual network
#define DEFAULT_WIFI_PSWD "your-password"    // Wi-Fi password, modify according to your actual network
#endif

#define HTTP_REQUEST_TIMEOUT 10 * 1000

/***********************************************************
********************** variable define *********************
***********************************************************/
static char server_response[512] = {0};

/***********************************************************
********************** function define *********************
***********************************************************/

/**
 * @brief Send HTTP POST request
 */
static void __send_http_post_request(void)
{
    PR_NOTICE("Sending HTTP POST request...");

    http_client_response_t http_response = {0};

    http_client_header_t headers[] = {
        {.key = "Content-Type", .value = "application/json"},
        {.key = "User-Agent", .value = "TuyaOpen-HTTP-Client"}
    };

    const char *post_body = "{\"action\":\"get_random_string\"}";

    http_client_status_t http_status = http_client_request(
        &(const http_client_request_t){
            .host = SERVER_HOST,
            .port = SERVER_PORT,
            .method = "POST",
            .path = SERVER_PATH,
            .headers = headers,
            .headers_count = sizeof(headers) / sizeof(http_client_header_t),
            .body = (const uint8_t *)post_body,
            .body_length = strlen(post_body),
            .timeout_ms = HTTP_REQUEST_TIMEOUT},
        &http_response);

    if (HTTP_CLIENT_SUCCESS != http_status) {
        PR_ERR("HTTP POST request failed, error: %d", http_status);
#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)
        ui_update_response_text("Request Failed", true);
#endif
        http_client_free(&http_response);
        return;
    }

    PR_DEBUG("HTTP POST request successful, status code: %d", http_response.status_code);

    if (http_response.status_code == 200 && http_response.body != NULL && http_response.body_length > 0) {
        // Copy raw response data directly to display buffer
        size_t copy_len = http_response.body_length;
        if (copy_len > sizeof(server_response) - 1) {
            copy_len = sizeof(server_response) - 1;
        }
        memcpy(server_response, http_response.body, copy_len);
        server_response[copy_len] = '\0';  // Ensure null-terminated
        
        PR_NOTICE("Server response: %.*s", (int)copy_len, (char *)http_response.body);
#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)
        ui_update_response_text(server_response, false);
#endif
    } else {
        PR_ERR("HTTP response error: status_code=%d, body_length=%zu", 
               http_response.status_code, 
               http_response.body_length);
    }

    http_client_free(&http_response);
}

#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)
/**
 * @brief Button click callback - called when UI button is clicked
 */
static void __button_click_callback(void)
{
    PR_NOTICE("Button clicked, sending HTTP POST request");
    
    // Check network status before sending
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    
    if (status != NETMGR_LINK_UP) {
        PR_ERR("Network not connected, cannot send request");
        ui_update_response_text("Network Not Connected", true);
        return;
    }
    
    // Update UI to show sending status
    ui_update_response_sending();
    
    // Send HTTP POST request
    __send_http_post_request();
}
#endif

/**
 * @brief Link status callback - updates Wi-Fi indicator
 */
OPERATE_RET __link_status_cb(void *data)
{
    netmgr_status_e status = *(netmgr_status_e *)data;

#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)
    // Update Wi-Fi status indicator
    if (status == NETMGR_LINK_UP) {
        ui_update_wifi_status(true);
        PR_NOTICE("Network connected");
    } else {
        ui_update_wifi_status(false);
        PR_NOTICE("Network disconnected");
    }
#endif

    return OPRT_OK;
}

/**
 * @brief user_main
 */
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

    PR_NOTICE("HTTP Client POST Configuration:");
    PR_NOTICE("Server Host:         %s", SERVER_HOST);
    PR_NOTICE("Server Port:         %d", SERVER_PORT);
    PR_NOTICE("Server Path:         %s", SERVER_PATH);

#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)
    // Initialize LVGL display UI
    ui_http_client_post_init(__button_click_callback);
    PR_NOTICE("LVGL display initialized");
#endif

    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();
    tal_event_subscribe(EVENT_LINK_STATUS_CHG, "http_client_post", __link_status_cb, SUBSCRIBE_TYPE_NORMAL);

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    TUYA_LwIP_Init();
#endif

    // Network init
    netmgr_type_e type = 0;
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    type |= NETCONN_WIFI;
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    type |= NETCONN_WIRED;
#endif
    netmgr_init(type);

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    // Connect Wi-Fi
    netconn_wifi_info_t wifi_info = {0};
    strcpy(wifi_info.ssid, DEFAULT_WIFI_SSID);
    strcpy(wifi_info.pswd, DEFAULT_WIFI_PSWD);
    netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info);
    PR_NOTICE("Connecting to Wi-Fi: %s", DEFAULT_WIFI_SSID);
#endif

    return;
}

/**
 * @brief main
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

static THREAD_HANDLE ty_app_thread = NULL;

static void tuya_app_thread(void *arg)
{
    user_main();
    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {1024 * 4, 4, "tuya_app_main",0};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
