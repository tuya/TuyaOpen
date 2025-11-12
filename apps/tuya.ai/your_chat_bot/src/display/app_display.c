/**
 * @file app_display.c
 * @author Tuya Inc.
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

#include "app_display.h"
#include "tuya_lvgl.h"

#include "font_awesome_symbols.h"
#include "ui_display.h"

#include "tal_log.h"
#include "tal_queue.h"
#include "tal_thread.h"

#include "tkl_memory.h"

#include "lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TY_DISPLAY_TYPE_E type;
    int len;
    char *data;
} DISPLAY_MSG_T;

typedef struct {
    QUEUE_HANDLE queue_hdl;
    THREAD_HANDLE thrd_hdl;

    UI_FONT_T ui_font;
} TUYA_DISPLAY_T;

/***********************************************************
********************function declaration********************
***********************************************************/
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_puhui_18_2);
LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_puhui_30_4);
LV_FONT_DECLARE(font_awesome_14_1);
LV_FONT_DECLARE(font_awesome_16_4);
LV_FONT_DECLARE(font_awesome_20_4);
LV_FONT_DECLARE(font_awesome_30_1);
LV_FONT_DECLARE(font_awesome_30_4);

extern const lv_font_t *font_emoji_32_init(void);
extern const lv_font_t *font_emoji_64_init(void);

/***********************************************************
***********************variable define**********************
***********************************************************/
#if defined(BOARD_CHOICE_BREAD_COMPACT_WIFI) || defined(BOARD_CHOICE_XINGZHI_CUBE_0_96_OLED_WIFI)
static UI_EMOJI_LIST_T sg_awesome_emo_list[] = {
    {"NEUTRAL", FONT_AWESOME_EMOJI_NEUTRAL},   {"SAD", FONT_AWESOME_EMOJI_SAD},
    {"ANGRY", FONT_AWESOME_EMOJI_ANGRY},       {"SURPRISE", FONT_AWESOME_EMOJI_SURPRISED},
    {"CONFUSED", FONT_AWESOME_EMOJI_CONFUSED}, {"THINKING", FONT_AWESOME_EMOJI_THINKING},
    {"HAPPY", FONT_AWESOME_EMOJI_HAPPY},
};
#endif

static __attribute__((unused)) UI_EMOJI_LIST_T sg_emo_list[EMO_ICON_MAX_NUM] = {
    {"NEUTRAL", "😶"},  {"SAD", "😔"},      {"ANGRY", "😠"}, {"SURPRISE", "😯"},
    {"CONFUSED", "😏"}, {"THINKING", "🤔"}, {"HAPPY", "🙂"},
};

static TUYA_DISPLAY_T sg_display = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

static OPERATE_RET __get_ui_font(UI_FONT_T *ui_font)
{
    OPERATE_RET rt = OPRT_OK;

    if (ui_font == NULL) {
        return OPRT_INVALID_PARM;
    }

#if (defined(BOARD_CHOICE_TUYA_T5AI_BOARD) || defined(BOARD_CHOICE_TUYA_T5AI_EVB) ||                                   \
     defined(BOARD_CHOICE_T5AI_MOJI_1_28) || defined(BOARD_CHOICE_TUYA_T5AI_CORE)|| defined(BOARD_CHOICE_T5AI_MINI) || defined(BOARD_CHOICE_T5AI_OTTO) || defined(BOARD_CHOICE_DNESP32S3_BOX) || \
     defined(BOARD_CHOICE_DNESP32S3_BOX2_WIFI)) ||                                                                     \
    defined(BOARD_CHOICE_WAVESHARE_T5AI_TOUCH_AMOLED_1_75)
#if defined(ENABLE_GUI_WECHAT)
    ui_font->text = (lv_font_t *)&font_puhui_18_2;
    ui_font->icon = (lv_font_t *)&font_awesome_16_4;
    ui_font->emoji = (lv_font_t *)font_emoji_32_init();
    ui_font->emoji_list = sg_emo_list;
#elif defined(ENABLE_GUI_CHATBOT)
    ui_font->text = (lv_font_t *)&font_puhui_18_2;
    ui_font->icon = (lv_font_t *)&font_awesome_16_4;
    ui_font->emoji = (lv_font_t *)font_emoji_64_init();
    ui_font->emoji_list = sg_emo_list;
#endif
#elif defined(BOARD_CHOICE_BREAD_COMPACT_WIFI)
    ui_font->text = (lv_font_t *)&font_puhui_14_1;
    ui_font->icon = (lv_font_t *)&font_awesome_14_1;
    ui_font->emoji = (lv_font_t *)&font_awesome_30_1;
    ui_font->emoji_list = sg_awesome_emo_list;
#elif defined(BOARD_CHOICE_XINGZHI_CUBE_0_96_OLED_WIFI)
    ui_font->text = (lv_font_t *)&font_puhui_14_1;
    ui_font->icon = (lv_font_t *)&font_awesome_14_1;
    ui_font->emoji = (lv_font_t *)&font_awesome_30_1;
    ui_font->emoji_list = sg_awesome_emo_list;
#elif defined(BOARD_CHOICE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_1_8)
    ui_font->text = (lv_font_t *)&font_puhui_30_4;
    ui_font->icon = (lv_font_t *)&font_awesome_30_4;
#if defined(ENABLE_GUI_WECHAT)
    ui_font->emoji = font_emoji_32_init();
#else
    ui_font->emoji = font_emoji_64_init();
#endif
    ui_font->emoji_list = sg_emo_list;
#elif defined(BOARD_CHOICE_DNESP32S3)
    ui_font->text = (lv_font_t *)&font_puhui_20_4;
    ui_font->icon = (lv_font_t *)&font_awesome_20_4;
#if defined(ENABLE_GUI_WECHAT)
    ui_font->emoji = font_emoji_32_init();
#else
    ui_font->emoji = font_emoji_64_init();
#endif
    ui_font->emoji_list = sg_emo_list;
#else
#error "Please define the font for your board"
#endif

    return rt;
}

static char *__ui_wifi_icon_get(UI_WIFI_STATUS_E status)
{
    char *wifi_icon = FONT_AWESOME_WIFI_OFF;

    switch (status) {
    case UI_WIFI_STATUS_DISCONNECTED:
        wifi_icon = FONT_AWESOME_WIFI_OFF;
        break;
    case UI_WIFI_STATUS_GOOD:
        wifi_icon = FONT_AWESOME_WIFI;
        break;
    case UI_WIFI_STATUS_FAIR:
        wifi_icon = FONT_AWESOME_WIFI_FAIR;
        break;
    case UI_WIFI_STATUS_WEAK:
        wifi_icon = FONT_AWESOME_WIFI_WEAK;
        break;
    default:
        wifi_icon = FONT_AWESOME_WIFI_OFF;
        break;
    }

    return wifi_icon;
}

static void __app_display_msg_handle(DISPLAY_MSG_T *msg_data)
{
    if (msg_data == NULL) {
        return;
    }

    tuya_lvgl_mutex_lock();

    switch (msg_data->type) {
    case TY_DISPLAY_TP_USER_MSG: {
        ui_set_user_msg(msg_data->data);
    } break;
    case TY_DISPLAY_TP_ASSISTANT_MSG: {
        ui_set_assistant_msg(msg_data->data);
    } break;
#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
    case TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_START: {
        ui_set_assistant_msg_stream_start();
    } break;
    case TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_DATA: {
        ui_set_assistant_msg_stream_data(msg_data->data);
    } break;
    case TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_END: {
        ui_set_assistant_msg_stream_end();
    } break;
    case TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_INTERRUPT: {
        ui_set_assistant_msg_stream_interrupt();
    } break;
#endif
    case TY_DISPLAY_TP_SYSTEM_MSG: {
        ui_set_system_msg(msg_data->data);
    } break;
    case TY_DISPLAY_TP_EMOTION: {
        ui_set_emotion(msg_data->data);
    } break;
    case TY_DISPLAY_TP_STATUS: {
        ui_set_status(msg_data->data);
    } break;
    case TY_DISPLAY_TP_NOTIFICATION: {
        ui_set_notification(msg_data->data);
    } break;
    case TY_DISPLAY_TP_NETWORK: {
        UI_WIFI_STATUS_E status = ((UI_WIFI_STATUS_E *)msg_data->data)[0];
        char *wifi_icon = __ui_wifi_icon_get(status);
        ui_set_network(wifi_icon);
    } break;
    case TY_DISPLAY_TP_CHAT_MODE: {
        ui_set_chat_mode(msg_data->data);
    } break;
    default: {
        PR_ERR("Invalid display type: %d", msg_data->type);
    } break;
    }

    tuya_lvgl_mutex_unlock();
}

static void __chat_bot_ui_task(void *args)
{
    OPERATE_RET rt = OPRT_OK;
    DISPLAY_MSG_T msg_data = {0};

    (void)args;

    tuya_lvgl_mutex_lock();
    // Initialize the display font
    TUYA_CALL_ERR_LOG(__get_ui_font(&sg_display.ui_font));
    // ui initialization
    TUYA_CALL_ERR_LOG(ui_init(&sg_display.ui_font));
#if defined(BOARD_CHOICE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_1_8)
    extern void lcd_sh8601_set_backlight(uint8_t brightness);
    lcd_sh8601_set_backlight(80); // set backlight to 80%
    ui_set_status_bar_pad(LV_HOR_RES * 0.1);
#endif
    tuya_lvgl_mutex_unlock();
    PR_DEBUG("ui init success");

    for (;;) {
        memset(&msg_data, 0, sizeof(DISPLAY_MSG_T));
        tal_queue_fetch(sg_display.queue_hdl, &msg_data, 0xFFFFFFFF);

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

    memset(&sg_display, 0, sizeof(TUYA_DISPLAY_T));

    // lvgl initialization
    TUYA_CALL_ERR_RETURN(tuya_lvgl_init());
    PR_DEBUG("lvgl init success");

    TUYA_CALL_ERR_RETURN(tal_queue_create_init(&sg_display.queue_hdl, sizeof(DISPLAY_MSG_T), 8));
    THREAD_CFG_T cfg = {
        .thrdname = "chat_ui",
        .priority = THREAD_PRIO_2,
        .stackDepth = 1024 * 4,
    };
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_display.thrd_hdl, NULL, NULL, __chat_bot_ui_task, NULL, &cfg));
    PR_DEBUG("chat bot ui task create success");

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
OPERATE_RET app_display_send_msg(TY_DISPLAY_TYPE_E tp, uint8_t *data, int len)
{
    DISPLAY_MSG_T msg_data;

    msg_data.type = tp;
    msg_data.len = len;
    if (len && data != NULL) {
        msg_data.data = (char *)tkl_system_psram_malloc(len + 1);
        if (NULL == msg_data.data) {
            return OPRT_MALLOC_FAILED;
        }
        memcpy(msg_data.data, data, len);
        msg_data.data[len] = 0; //"\0"
    } else {
        msg_data.data = NULL;
    }

    tal_queue_post(sg_display.queue_hdl, &msg_data, 0xFFFFFFFF);

    return OPRT_OK;
}