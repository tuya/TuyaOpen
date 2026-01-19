/**
 * @file ai_ui_icon_font.h
 * @brief ai_ui_icon_font module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_UI_ICON_FONT_H__
#define __AI_UI_ICON_FONT_H__

#include "tuya_cloud_types.h"
#include "ai_ui_manage.h"

#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define FONT_EMO_ICON_MAX_NUM 7

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char  emo_name[32];
    char *emo_icon;
} AI_UI_EMOJI_LIST_T;

typedef struct {
    lv_font_t          *text;
    lv_font_t          *icon;
    const lv_font_t    *emoji;
    AI_UI_EMOJI_LIST_T *emoji_list;
}AI_UI_FONT_LIST_T;

/***********************************************************
********************function declaration********************
***********************************************************/

lv_font_t *ai_ui_get_text_font(void);

lv_font_t *ai_ui_get_icon_font(void);

lv_font_t *ai_ui_get_emo_font(void);

AI_UI_EMOJI_LIST_T *ai_ui_get_emo_list(void);

char *ai_ui_get_wifi_icon(AI_UI_WIFI_STATUS_E status);

#endif

#ifdef __cplusplus
}
#endif

#endif /* __AI_UI_ICON_FONT_H__ */
