/**
 * @file ui_display.h
 * @brief Header file for UI Display
 *
 * This header file defines the interface for the UI display module, including
 * initialization functions and data structures for managing fonts, emojis, and
 * other display elements. It provides the necessary declarations for integrating
 * display functionality into the chatbot application.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __UI_DISPLAY_H__
#define __UI_DISPLAY_H__

#include "tuya_cloud_types.h"

#include "lang_config.h"

#include "lvgl.h"

#include "app_display.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define EMO_ICON_MAX_NUM 7

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char emo_text[32];
    char *emo_icon;
} UI_EMOJI_LIST_T;

typedef struct {
    lv_font_t *text;
    lv_font_t *icon;

    const lv_font_t *emoji;
    UI_EMOJI_LIST_T *emoji_list;
} UI_FONT_T;

/***********************************************************
********************function declaration********************
***********************************************************/

int ui_init(UI_FONT_T *ui_font);

void ui_set_user_msg(const char *text);

void ui_set_assistant_msg(const char *text);

void ui_set_system_msg(const char *text);

void ui_set_emotion(const char *emotion);

void ui_set_status(const char *status);

void ui_set_notification(const char *notification);

void ui_set_network(char *wifi_icon);

void ui_set_chat_mode(const char *chat_mode);

void ui_set_status_bar_pad(int32_t value);

#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
void ui_set_assistant_msg_stream_start(void);

void ui_set_assistant_msg_stream_data(const char *text);

void ui_set_assistant_msg_stream_end(void);

void ui_set_assistant_msg_stream_interrupt(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __UI_DISPLAY_H__ */
