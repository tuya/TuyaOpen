/**
 * @file app_display.c
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

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_memory.h"

#include "app_display.h"

#include "lvgl.h"
#include "lv_vendor.h"
#include "main_screen.h"
#include "toast_screen.h"
#include "rfid_scan_screen.h"
#include "ai_log_screen.h"
#include "axp2101_driver.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    POCKET_DISP_TP_E type;
    int len;
    uint8_t *data;
} DISPLAY_MSG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static QUEUE_HANDLE sg_queue_hdl = NULL;
static THREAD_HANDLE sg_thrd_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __app_display_msg_handle(DISPLAY_MSG_T *msg_data)
{
    if (msg_data == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    // PR_DEBUG("Display message type: %d", msg_data->type);
    // PR_NOTICE("Free heap size: %d", tal_system_get_free_heap_size());

    switch (msg_data->type) {
    case POCKET_DISP_TP_MENU_UP:
    case POCKET_DISP_TP_MENU_DOWN:
    case POCKET_DISP_TP_MENU_RIGHT:
    case POCKET_DISP_TP_MENU_LEFT:
    case POCKET_DISP_TP_MENU_ENTER:
    case POCKET_DISP_TP_MENU_ESC:
    case POCKET_DISP_TP_AI:
    case POCKET_DISP_TP_EMOJ_HAPPY:
        toast_screen_show("Pet: Happy", 1000);
        break;
    case POCKET_DISP_TP_EMOJ_ANGRY:
        toast_screen_show("Pet: Angry", 1000);
        break;
    case POCKET_DISP_TP_EMOJ_CRY:
        toast_screen_show("Pet: Crying", 1000);
        break;
    case POCKET_DISP_TP_WIFI_OFF:
        main_screen_set_wifi_state(0);
        break;
    case POCKET_DISP_TP_WIFI_CONNECTED:
        // toast_screen_show("WiFi Connected", 2000);
        main_screen_set_wifi_state(3);
        break;
    case POCKET_DISP_TP_WIFI_FIND:
        main_screen_set_wifi_state(4);
        break;
    case POCKET_DISP_TP_WIFI_ADD:
        main_screen_set_wifi_state(5);
        break;
    case POCKET_DISP_TP_BATTERY_STATUS:
        // Battery state is automatically updated by main_screen timer in hardware mode
        // No need to manually update here
        break;
    case POCKET_DISP_TP_BATTERY_CHARGING:
        // Battery charging state is automatically detected by main_screen timer in hardware mode
        // No need to manually update here
        break;

    case POCKET_DISP_TP_RFID_SCAN_SUCCESS:
        if (screen_get_now_screen() != &rfid_scan_screen) {
            screen_load(&rfid_scan_screen);
        }
        break;

    case POCKET_DISP_TP_AI_LOG:
        // For AI log messages, simply print to console
        PR_DEBUG("AI LOG: %d", msg_data->len);
        if (msg_data->data && msg_data->len > 0) {
            ai_log_screen_update_log((const char *)msg_data->data, msg_data->len);
        }
        break;

    default:
        break;
    }

    lv_vendor_disp_unlock();
}

static void __disp_pet_task(void *args)
{
    DISPLAY_MSG_T msg_data = {0};

    (void)args;

    for (;;) {
        memset(&msg_data, 0, sizeof(DISPLAY_MSG_T));
        tal_queue_fetch(sg_queue_hdl, &msg_data, 0xFFFFFFFF);

        __app_display_msg_handle(&msg_data);

        if (msg_data.data) {
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
OPERATE_RET app_display_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    lv_vendor_init(DISPLAY_NAME);

    screens_init();

    lv_vendor_start(5, 1024*8);

    PR_DEBUG("app_display_init success");

    TUYA_CALL_ERR_RETURN(tal_queue_create_init(&sg_queue_hdl, sizeof(DISPLAY_MSG_T), 8));
    THREAD_CFG_T cfg = {
        .thrdname = "pet_ui",
        .priority = THREAD_PRIO_1,
        .stackDepth = 1024 * 4,
    };
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_thrd_hdl, NULL, NULL, __disp_pet_task, NULL, &cfg));
    PR_DEBUG("app_display_init success");

    return rt;
}

/**
 * @brief Send display message to the display system
 *
 * @param tp Type of the display message
 * @param data Pointer to the message data
 * @param len Length of the message data
 * @return OPERATE_RET Result of sending the message, OPRT_OK indicates success
 */
OPERATE_RET app_display_send_msg(POCKET_DISP_TP_E tp, uint8_t *data, int len)
{
    DISPLAY_MSG_T msg_data;

    msg_data.type = tp;
    msg_data.len = len;
    if (len && data != NULL) {
        msg_data.data = (uint8_t *)tkl_system_psram_malloc(len + 1);
        if (NULL == msg_data.data) {
            return OPRT_MALLOC_FAILED;
        }
        memcpy(msg_data.data, data, len);
        msg_data.data[len] = 0; //"\0"
    } else {
        msg_data.data = NULL;
    }

    tal_queue_post(sg_queue_hdl, &msg_data, 0xFFFFFFFF);

    return OPRT_OK;
}