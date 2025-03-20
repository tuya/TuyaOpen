/**
 * @file tuya_display.c
 * @version 0.1
 * @date 2025-03-17
 */
#include "tkl_queue.h"
#include "tkl_memory.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_log.h"

#include "tuya_display.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TY_DISPLAY_TYPE_E type;
    int               len;
    char             *data;
}DISP_CHAT_MSG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
const char *POWER_TEXT   =  "你好啊，我来了，让我们一起玩耍吧";
const char *NET_OK_TEXT  =  "我已联网，让我们开始对话吧";
const char *NET_CFG_TEXT =  "我已进入配网状态，你能帮我用涂鸦智能app配网嘛";

static TKL_QUEUE_HANDLE sg_chat_msg_queue_hdl = NULL;
static THREAD_HANDLE    sg_display_thrd_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __chat_display_task(void *args)
{
    DISP_CHAT_MSG_T msg_data;

    (void) args;

    tuya_display_lv_homepage();

    while(1) {
        tkl_queue_fetch(sg_chat_msg_queue_hdl, &msg_data, TKL_QUEUE_WAIT_FROEVER);

        switch(msg_data.type) {
            case TY_DISPLAY_TP_HUMAN_CHAT:
                tuya_display_lv_chat_message(msg_data.data, false);
                break;
            case TY_DISPLAY_TP_AI_CHAT:
                tuya_display_lv_chat_message(msg_data.data, true);
                break;
            case TY_DISPLAY_TP_STAT_LISTEN:
                
                break;
            case TY_DISPLAY_TP_STAT_SPEAK:
                break;
            case TY_DISPLAY_TP_STAT_IDLE:
                break;
            case TY_DISPLAY_TP_STAT_NETCFG:
                tuya_display_lv_chat_message(NET_CFG_TEXT, true);
                tuya_display_lv_wifi_state(false);
                break;
            case TY_DISPLAY_TP_STAT_POWERON:
                tuya_display_lv_chat_message(POWER_TEXT, true);
                break;
            case TY_DISPLAY_TP_STAT_ONLINE:
                tuya_display_lv_chat_message(NET_OK_TEXT, true);
                tuya_display_lv_wifi_state(true);
                break;
            default:
                break;

        }

        if(msg_data.data) {
            tkl_system_psram_free(msg_data.data);
        }
        msg_data.data = NULL;
    }
}

OPERATE_RET tuya_display_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    tuya_display_lvgl_init();

    TUYA_CALL_ERR_RETURN(tkl_queue_create_init(&sg_chat_msg_queue_hdl, sizeof(DISP_CHAT_MSG_T), 8));
 
    THREAD_CFG_T cfg = {
        .thrdname = "chat_display",
        .priority = THREAD_PRIO_1,
        .stackDepth = 1024*2,
    };

    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_display_thrd_hdl, NULL, NULL, __chat_display_task, NULL, &cfg));

    return OPRT_OK;
}

OPERATE_RET tuya_display_send_msg(TY_DISPLAY_TYPE_E tp, char *data, int len)
{
    DISP_CHAT_MSG_T chat_msg;

    chat_msg.type = tp;
    chat_msg.len  = len;
    chat_msg.data = (char *)tkl_system_psram_malloc(len+1);
    if(NULL == chat_msg.data) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(chat_msg.data, data, len);
    chat_msg.data[len] = 0; //"\0"

    tkl_queue_post(sg_chat_msg_queue_hdl, &chat_msg, TKL_QUEUE_WAIT_FROEVER);

    return OPRT_OK;
}