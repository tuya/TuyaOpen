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

    // custom ui register
    TUYA_CALL_ERR_RETURN(app_ai_ui_register());

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

    return OPRT_OK;
}
