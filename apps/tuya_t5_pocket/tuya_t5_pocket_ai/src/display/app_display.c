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

#include "ai_pocket_pet_app.h"
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

    switch (msg_data->type) {
    case POCKET_DISP_TP_MENU_UP:
        lv_demo_ai_pocket_pet_handle_input(KEY_UP);
        break;
    case POCKET_DISP_TP_MENU_DOWN:
        lv_demo_ai_pocket_pet_handle_input(KEY_DOWN);
        break;
    case POCKET_DISP_TP_MENU_RIGHT:
        lv_demo_ai_pocket_pet_handle_input(KEY_RIGHT);
        break;
    case POCKET_DISP_TP_MENU_LEFT:
        lv_demo_ai_pocket_pet_handle_input(KEY_LEFT);
        break;
    case POCKET_DISP_TP_MENU_ENTER:
        lv_demo_ai_pocket_pet_handle_input(KEY_ENTER);
        break;
    case POCKET_DISP_TP_MENU_ESC:
        lv_demo_ai_pocket_pet_handle_input(KEY_ESC);
        break;
    case POCKET_DISP_TP_AI:
        lv_demo_ai_pocket_pet_handle_input(KEY_AI);
        break;
    case POCKET_DISP_TP_EMOJ_HAPPY:
        lv_demo_ai_pocket_pet_show_toast("Pet: Happy", 1000);
        break;
    case POCKET_DISP_TP_EMOJ_ANGRY:
        lv_demo_ai_pocket_pet_show_toast("Pet: Angry", 1000);
        break;
    case POCKET_DISP_TP_EMOJ_CRY:
        lv_demo_ai_pocket_pet_show_toast("Pet: Crying", 1000);
        break;
    case POCKET_DISP_TP_WIFI_OFF:
        status_bar_set_wifi_strength(0);
        break;
    case POCKET_DISP_TP_WIFI_CONNECTED:
        status_bar_set_wifi_strength(3);
        break;
    case POCKET_DISP_TP_WIFI_FIND:
        status_bar_set_wifi_strength(4);
        break;
    case POCKET_DISP_TP_WIFI_ADD:
        status_bar_set_wifi_strength(5);
        break;
    case POCKET_DISP_TP_BATTERY_STATUS: {
        lv_demo_ai_pocket_pet_set_battery_status((uint8_t)(axp2101_getBatteryPercent() / 100.0f * 7),
                                                 axp2101_isCharging());
    } break;
    case POCKET_DISP_TP_BATTERY_CHARGING: {
        lv_demo_ai_pocket_pet_set_battery_status(lv_demo_ai_pocket_pet_get_battery_level(), true);
    } break;

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

    lv_demo_ai_pocket_pet();

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