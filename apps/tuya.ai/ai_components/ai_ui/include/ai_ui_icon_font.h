/**
 * @file ai_ui_icon_font.h
 * @brief Icon and font management interface definitions.
 *
 * This header provides function declarations for managing fonts and icons
 * used in AI UI, including text fonts, icon fonts, emoji fonts, and WiFi icons.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
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
/**
 * @brief Get text font for UI display.
 *
 * @return Pointer to LVGL text font structure.
 */
lv_font_t *ai_ui_get_text_font(void);

/**
 * @brief Get icon font for UI display.
 *
 * @return Pointer to LVGL icon font structure.
 */
lv_font_t *ai_ui_get_icon_font(void);

/**
 * @brief Get emoji font for UI display.
 *
 * @return Pointer to LVGL emoji font structure.
 */
lv_font_t *ai_ui_get_emo_font(void);

/**
 * @brief Get emoji list for UI display.
 *
 * @return Pointer to emoji list structure.
 */
AI_UI_EMOJI_LIST_T *ai_ui_get_emo_list(void);

/**
 * @brief Get WiFi icon string based on WiFi status.
 *
 * @param status WiFi status (disconnected, good, fair, weak).
 * @return Pointer to WiFi icon string.
 */
char *ai_ui_get_wifi_icon(AI_UI_WIFI_STATUS_E status);

#endif

#ifdef __cplusplus
}
#endif

#endif /* __AI_UI_ICON_FONT_H__ */
