/**
 * @file tuya_display.c
 * @brief Handle display initialization and message processing
 *
 * This source file provides the implementation for initializing the display system,
 * creating a message queue, and handling display messages in a separate task.
 * It includes functions to initialize the display, send messages to the display,
 * and manage the display task.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_queue.h"
#include "tkl_memory.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_log.h"

#include "tuya_display.h"
#include "display_gui.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static TKL_QUEUE_HANDLE sg_chat_msg_queue_hdl = NULL;
static THREAD_HANDLE    sg_display_thrd_hdl   = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Task to handle display messages
 * 
 * @param args Arguments passed to the task (not used in this function)
 * @return None
 */
static void __chat_display_task(void *args)
{
    DISP_CHAT_MSG_T msg_data;

    (void)args;

    display_gui_homepage();

    display_gui_chat_frame_init();

    while (1) {
        tkl_queue_fetch(sg_chat_msg_queue_hdl, &msg_data, TKL_QUEUE_WAIT_FROEVER);

        display_gui_chat_msg_handle(&msg_data);

        if(msg_data.data) {
            tkl_system_psram_free(msg_data.data);
        }
        msg_data.data = NULL;
    }
}

/**
 * @brief Initialize the display system
 * 
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET tuya_display_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    display_gui_init();

    TUYA_CALL_ERR_RETURN(tkl_queue_create_init(&sg_chat_msg_queue_hdl, sizeof(DISP_CHAT_MSG_T), 8));

    THREAD_CFG_T cfg = {
        .thrdname = "chat_display",
        .priority = THREAD_PRIO_1,
        .stackDepth = 1024 * 4,
    };

    TUYA_CALL_ERR_RETURN(
        tal_thread_create_and_start(&sg_display_thrd_hdl, NULL, NULL, __chat_display_task, NULL, &cfg));

    return OPRT_OK;
}

/**
 * @brief Send display message to the display system
 * 
 * @param tp Type of the display message
 * @param data Pointer to the message data
 * @param len Length of the message data
 * @return OPERATE_RET Result of sending the message, OPRT_OK indicates success
 */
OPERATE_RET tuya_display_send_msg(TY_DISPLAY_TYPE_E tp, char *data, int len)
{
    DISP_CHAT_MSG_T chat_msg;

    chat_msg.type = tp;
    chat_msg.len = len;
    if (len && data != NULL) {
        chat_msg.data = (char *)tkl_system_psram_malloc(len + 1);
        if (NULL == chat_msg.data) {
            return OPRT_MALLOC_FAILED;
        }
        memcpy(chat_msg.data, data, len);
        chat_msg.data[len] = 0; //"\0"
    } else {
        chat_msg.data = NULL;
    }

    tkl_queue_post(sg_chat_msg_queue_hdl, &chat_msg, TKL_QUEUE_WAIT_FROEVER);

    return OPRT_OK;
}