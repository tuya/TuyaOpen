/**
 * @file app_chat_bot.c
 * @brief app_chat_bot module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#include "tal_api.h"

#include "netmgr.h"

#include "ai_chat_main.h"
#include "app_chat_bot.h"

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "tkl_wifi.h"
#endif
/***********************************************************
************************macro define************************
***********************************************************/
#define PRINTF_FREE_HEAP_TTIME (10 * 1000)
#define DISP_NET_STATUS_TIME   (1 * 1000)

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************const declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TIMER_ID sg_printf_heap_tm;

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
static AI_UI_WIFI_STATUS_E sg_wifi_status = AI_UI_WIFI_STATUS_DISCONNECTED;
static TIMER_ID            sg_disp_status_tm;
#endif
/***********************************************************
***********************function define**********************
***********************************************************/
static void __printf_free_heap_tm_cb(TIMER_ID timer_id, void *arg)
{
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    uint32_t free_heap       = tal_system_get_free_heap_size();
    uint32_t free_psram_heap = tal_psram_get_free_heap_size();
    PR_INFO("Free heap size:%d, Free psram heap size:%d", free_heap, free_psram_heap);
#else
    uint32_t free_heap = tal_system_get_free_heap_size();
    PR_INFO("Free heap size:%d", free_heap);
#endif
}

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
static void __display_net_status_update(void)
{
    AI_UI_WIFI_STATUS_E wifi_status = AI_UI_WIFI_STATUS_DISCONNECTED;
    netmgr_status_e     net_status  = NETMGR_LINK_DOWN;

    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &net_status);
    if (net_status == NETMGR_LINK_UP) {
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
        // get rssi
        int8_t rssi = 0;
#ifndef PLATFORM_T5
        // BUG: Getting RSSI causes a crash on T5 platform
        tkl_wifi_station_get_conn_ap_rssi(&rssi);
#endif
        if (rssi >= -60) {
            wifi_status = AI_UI_WIFI_STATUS_GOOD;
        } else if (rssi >= -70) {
            wifi_status = AI_UI_WIFI_STATUS_FAIR;
        } else {
            wifi_status = AI_UI_WIFI_STATUS_WEAK;
        }
#else
        wifi_status = AI_UI_WIFI_STATUS_GOOD;
#endif
    } else {
        wifi_status = AI_UI_WIFI_STATUS_DISCONNECTED;
    }

    if (wifi_status != sg_wifi_status) {
        sg_wifi_status = wifi_status;
        ai_ui_disp_msg(AI_UI_DISP_NETWORK, (uint8_t *)&wifi_status, sizeof(AI_UI_WIFI_STATUS_E));
    }
}

static void __display_status_tm_cb(TIMER_ID timer_id, void *arg)
{
    __display_net_status_update();
}

#endif

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
static void __ai_video_display_flush(TDL_CAMERA_FRAME_T *frame)
{
#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    ai_ui_camera_flush(frame->data, frame->width, frame->height);
#endif
}
#endif

static void __ai_chat_handle_event(AI_NOTIFY_EVENT_T *event)
{
    (void)event;
}

OPERATE_RET app_chat_bot_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    AI_CHAT_MODE_CFG_T ai_chat_cfg = {
        .default_mode = AI_CHAT_MODE_WAKEUP,
        .default_vol  = 70,
        .evt_cb       = __ai_chat_handle_event,
    };
    TUYA_CALL_ERR_RETURN(ai_chat_init(&ai_chat_cfg));

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    AI_VIDEO_CFG_T ai_video_cfg = {
        .disp_flush_cb = __ai_video_display_flush,
    };

    TUYA_CALL_ERR_LOG(ai_video_init(&ai_video_cfg));
#endif

#if defined(ENABLE_COMP_AI_MCP) && (ENABLE_COMP_AI_MCP == 1)
    TUYA_CALL_ERR_RETURN(ai_mcp_init());
#endif

    // Free heap size
    tal_sw_timer_create(__printf_free_heap_tm_cb, NULL, &sg_printf_heap_tm);
    tal_sw_timer_start(sg_printf_heap_tm, PRINTF_FREE_HEAP_TTIME, TAL_TIMER_CYCLE);

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    ai_ui_disp_msg(AI_UI_DISP_NETWORK, (uint8_t *)&sg_wifi_status, sizeof(AI_UI_WIFI_STATUS_E));

    ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)INITIALIZING, strlen(INITIALIZING));
    ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_NEUTRAL, strlen(EMOJI_NEUTRAL));

    // display status update
    tal_sw_timer_create(__display_status_tm_cb, NULL, &sg_disp_status_tm);
    tal_sw_timer_start(sg_disp_status_tm, DISP_NET_STATUS_TIME, TAL_TIMER_CYCLE);
#endif

    return OPRT_OK;
}
