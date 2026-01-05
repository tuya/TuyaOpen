/**
 * @file app_system_info.c
 * @brief app_system_info module is used to
 * @version 0.1
 * @date 2025-04-28
 */

#include "app_system_info.h"
#include "ai_audio_player.h"

#include "app_display.h"
#include "app_chat_bot.h"

#include "tal_api.h"
#include "tuya_iot.h"
#include "netmgr.h"

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "tkl_wifi.h"
#else
// Stub WiFi functions for non-WiFi platforms (e.g., Ubuntu with wired)
#include "tkl_wifi_stub.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define FREE_HEAP_TM      (10 * 1000)
#define DISPLAY_STATUS_TM (1 * 1000)

// Display status message
typedef enum {
    DISPLAY_STATUS_VERSION = 0,
    DISPLAY_STATUS_STANDBY,
    DISPLAY_STATUS_TIME,
} SI_DISPLAY_STATUS_E;

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TIMER_ID heap_tm;

    TIMER_ID display_status_tm;
    UI_WIFI_STATUS_E last_net_status;

    SI_DISPLAY_STATUS_E display_status;

    uint8_t hour;
    uint8_t min;
} APP_SYSTEM_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_SYSTEM_INFO_T system_info = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

static void __app_free_heap_tm_cb(TIMER_ID timer_id, void *arg)
{
    uint32_t free_heap = tal_system_get_free_heap_size();
    PR_INFO("Free heap size:%d", free_heap);
}

static void __app_display_net_status_update(void)
{
    UI_WIFI_STATUS_E wifi_status = UI_WIFI_STATUS_DISCONNECTED;
    netmgr_status_e net_status = NETMGR_LINK_DOWN;

    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &net_status);
    if (net_status == NETMGR_LINK_UP) {
        // get rssi
        int8_t rssi = 0;
#ifndef PLATFORM_T5
        // BUG: Getting RSSI causes a crash on T5 platform
        tkl_wifi_station_get_conn_ap_rssi(&rssi);
#endif
        if (rssi >= -60) {
            wifi_status = UI_WIFI_STATUS_GOOD;
        } else if (rssi >= -70) {
            wifi_status = UI_WIFI_STATUS_FAIR;
        } else {
            wifi_status = UI_WIFI_STATUS_WEAK;
        }
    } else {
        wifi_status = UI_WIFI_STATUS_DISCONNECTED;
    }

    if (wifi_status != system_info.last_net_status) {
        system_info.last_net_status = wifi_status;
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_NETWORK, (uint8_t *)&wifi_status, sizeof(UI_WIFI_STATUS_E));
#endif
    }
}

static __attribute__((unused)) void __app_display_status_time_update(uint8_t force_update)
{
    POSIX_TM_S tm = {0};
    tal_time_get_local_time_custom(0, &tm);

    if (tm.tm_hour != system_info.hour || tm.tm_min != system_info.min || force_update) {
        system_info.hour = tm.tm_hour;
        system_info.min = tm.tm_min;

        char tm_str[10] = {0};
        snprintf(tm_str, sizeof(tm_str), "%02d:%02d", system_info.hour, system_info.min);
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)tm_str, strlen(tm_str));
#endif
    }
}

static void __app_display_status_tm_cb(TIMER_ID timer_id, void *arg)
{
    static uint32_t net_status_cnt = 0;

    // Update the network status every 10 minutes
    if ((net_status_cnt * DISPLAY_STATUS_TM) >= 1000 || net_status_cnt == 0) {
        __app_display_net_status_update();
        net_status_cnt = 0;
    }
    net_status_cnt++;
}

void app_system_info(void)
{
    // Free heap size
    tal_sw_timer_create(__app_free_heap_tm_cb, NULL, &system_info.heap_tm);
    tal_sw_timer_start(system_info.heap_tm, FREE_HEAP_TM, TAL_TIMER_CYCLE);

    // display status update
    tal_sw_timer_create(__app_display_status_tm_cb, NULL, &system_info.display_status_tm);

    // Set the initial network status
    system_info.last_net_status = UI_WIFI_STATUS_DISCONNECTED;
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
    app_display_send_msg(TY_DISPLAY_TP_NETWORK, &system_info.last_net_status, sizeof(system_info.last_net_status));
#endif

    // Set the initial status
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
    app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)INITIALIZING, strlen(INITIALIZING));
    app_display_send_msg(TY_DISPLAY_TP_EMOTION, (uint8_t *)"NATURAL", strlen("NATURAL"));
#endif

    tal_sw_timer_start(system_info.display_status_tm, DISPLAY_STATUS_TM, TAL_TIMER_CYCLE);
}
