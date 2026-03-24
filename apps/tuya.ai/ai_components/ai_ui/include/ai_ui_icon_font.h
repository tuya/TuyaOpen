/**
 * @file ai_ui_icon_font.h
 * @brief Icon and font management interface definitions.
 *
 * DuckyClaw override of TuyaOpen's ai_ui_icon_font.h.
 * Expands FONT_EMO_ICON_MAX_NUM to cover all 27 canonical emotion names
 * plus 27 raw UTF-8 emoji fallback entries (UTF-8 variant only), so that
 * the display layer correctly handles:
 *   - All 27 emotion names returned by the AI model.
 *   - Raw UTF-8 emoji characters (e.g. "😊") if the LLM sends those instead
 *     of a canonical name.
 *   - Any emotion string outside the defined set falls back to NEUTRAL.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
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

/* 27 canonical emotion names.  For the Font Awesome variant the list stops
 * here.  For the UTF-8 emoji variant an additional 27 raw-emoji entries are
 * appended so that a bare emoji character (e.g. "😊") returned by the LLM is
 * also matched. */
#define FONT_EMO_ICON_NAME_NUM 27

#if defined(FONT_EMO_AWESOME) && (FONT_EMO_AWESOME == 1)
#define FONT_EMO_ICON_MAX_NUM FONT_EMO_ICON_NAME_NUM
#else
/* name entries + UTF-8 emoji fallback entries */
#define FONT_EMO_ICON_MAX_NUM (FONT_EMO_ICON_NAME_NUM * 2)
#endif

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
} AI_UI_FONT_LIST_T;

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
 * @return Pointer to emoji list structure (FONT_EMO_ICON_MAX_NUM entries).
 */
AI_UI_EMOJI_LIST_T *ai_ui_get_emo_list(void);

/**
 * @brief Get WiFi icon string based on WiFi status.
 *
 * @param status WiFi status (disconnected, good, fair, weak).
 * @return Pointer to WiFi icon string.
 */
char *ai_ui_get_wifi_icon(AI_UI_WIFI_STATUS_E status);

#ifdef __cplusplus
}
#endif

#endif /* ENABLE_LIBLVGL */

#endif /* __AI_UI_ICON_FONT_H__ */
