/**
 * @file ai_ui_icon_font.c
 * @brief Icon and font management implementation.
 *
 * DuckyClaw override of TuyaOpen's ai_ui_icon_font.c.
 *
 * Changes from upstream TuyaOpen:
 *   1. Expanded sg_emo_list / sg_awesome_emo_list from 7 to all 27 emotions
 *      defined in skill_emotion.h, so every AI-returned emotion name is
 *      displayed correctly.
 *   2. Added 27 raw UTF-8 emoji fallback entries in sg_emo_list (non-awesome
 *      variant).  If the LLM returns a bare emoji character such as "😊"
 *      instead of the canonical name "HAPPY", the character is still matched
 *      and the correct icon is shown.
 *   3. FONT_EMO_ICON_MAX_NUM is now 27 (Font Awesome) or 54 (UTF-8) so the
 *      search loop in __ui_set_emotion covers the full list.
 *   4. Any emotion string that is not in the list falls back to NEUTRAL
 *      (the first entry), which is the existing fallback behaviour.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"

#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)

#include "font_awesome_symbols.h"

#include "ai_ui_icon_font.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
/* Declare all possible fonts at file scope */
#if defined(FONT_TEXT_SIZE_14_1) && (FONT_TEXT_SIZE_14_1 == 1)
LV_FONT_DECLARE(font_puhui_14_1);
#endif

#if defined(FONT_TEXT_SIZE_18_2) && (FONT_TEXT_SIZE_18_2 == 1)
LV_FONT_DECLARE(font_puhui_18_2);
#endif

#if defined(FONT_TEXT_SIZE_20_4) && (FONT_TEXT_SIZE_20_4 == 1)
LV_FONT_DECLARE(font_puhui_20_4);
#endif

#if defined(FONT_TEXT_SIZE_30_4) && (FONT_TEXT_SIZE_30_4 == 1)
LV_FONT_DECLARE(font_puhui_30_4);
#endif

#if defined(FONT_ICON_SIZE_14_1) && (FONT_ICON_SIZE_14_1 == 1)
LV_FONT_DECLARE(font_awesome_14_1);
#endif

#if defined(FONT_ICON_SIZE_16_4) && (FONT_ICON_SIZE_16_4 == 1)
LV_FONT_DECLARE(font_awesome_16_4);
#endif

#if defined(FONT_ICON_SIZE_20_4) && (FONT_ICON_SIZE_20_4 == 1)
LV_FONT_DECLARE(font_awesome_20_4);
#endif

#if defined(FONT_ICON_SIZE_30_4) && (FONT_ICON_SIZE_30_4 == 1)
LV_FONT_DECLARE(font_awesome_30_4);
#endif

#if defined(FONT_EMO_AWESOME) && (FONT_EMO_AWESOME == 1)

LV_FONT_DECLARE(font_awesome_30_1);

/* ---------------------------------------------------------------------------
 * Font Awesome emoji list – 27 canonical emotion names.
 * For emotions without a dedicated FA glyph the closest available symbol
 * is used so that the display always shows something meaningful.
 * --------------------------------------------------------------------------- */
static AI_UI_EMOJI_LIST_T sg_awesome_emo_list[FONT_EMO_ICON_NAME_NUM] = {
    {"NEUTRAL", FONT_AWESOME_EMOJI_NEUTRAL},
    {"HAPPY", FONT_AWESOME_EMOJI_HAPPY},
    {"LAUGHING", FONT_AWESOME_EMOJI_LAUGHING},
    {"FUNNY", FONT_AWESOME_EMOJI_FUNNY},
    {"SAD", FONT_AWESOME_EMOJI_SAD},
    {"ANGRY", FONT_AWESOME_EMOJI_ANGRY},
    {"FEARFUL", FONT_AWESOME_EMOJI_CRYING}, /* closest: crying/fearful */
    {"LOVING", FONT_AWESOME_EMOJI_LOVING},
    {"EMBARRASSED", FONT_AWESOME_EMOJI_EMBARRASSED},
    {"SURPRISE", FONT_AWESOME_EMOJI_SURPRISED},
    {"SHOCKED", FONT_AWESOME_EMOJI_SHOCKED},
    {"THINKING", FONT_AWESOME_EMOJI_THINKING},
    {"WINK", FONT_AWESOME_EMOJI_WINKING},
    {"COOL", FONT_AWESOME_EMOJI_COOL},
    {"RELAXED", FONT_AWESOME_EMOJI_RELAXED},
    {"DELICIOUS", FONT_AWESOME_EMOJI_DELICIOUS},
    {"KISSY", FONT_AWESOME_EMOJI_KISSY},
    {"CONFIDENT", FONT_AWESOME_EMOJI_CONFIDENT},
    {"SLEEP", FONT_AWESOME_EMOJI_SLEEPY},
    {"SILLY", FONT_AWESOME_EMOJI_SILLY},
    {"CONFUSED", FONT_AWESOME_EMOJI_CONFUSED},
    {"TOUCH", FONT_AWESOME_EMOJI_WINKING},    /* closest: friendly/wink */
    {"DISAPPOINTED", FONT_AWESOME_EMOJI_SAD}, /* closest: sad */
    {"ANNOYED", FONT_AWESOME_EMOJI_CONFUSED}, /* closest: confused */
    {"WAKEUP", FONT_AWESOME_EMOJI_SURPRISED}, /* closest: surprised */
    {"LEFT", FONT_AWESOME_EMOJI_NEUTRAL},     /* directional – use neutral */
    {"RIGHT", FONT_AWESOME_EMOJI_NEUTRAL},    /* directional – use neutral */
};

#else /* UTF-8 emoji variant */

/* ---------------------------------------------------------------------------
 * UTF-8 emoji list.
 *
 * The first FONT_EMO_ICON_NAME_NUM (27) entries map canonical emotion NAME
 * strings (e.g. "HAPPY") to the corresponding UTF-8 emoji character.
 *
 * The following FONT_EMO_ICON_NAME_NUM (27) entries map the raw UTF-8 emoji
 * characters (e.g. "🙂") directly, allowing the display layer to handle the
 * case where the LLM returns a bare emoji instead of a canonical name.
 *
 * Any input not matched by any entry falls back to sg_emo_list[0].emo_icon
 * (NEUTRAL / 😶) in __ui_set_emotion.
 * --------------------------------------------------------------------------- */
static AI_UI_EMOJI_LIST_T sg_emo_list[FONT_EMO_ICON_MAX_NUM] = {
    /* --- canonical name → UTF-8 emoji --- */
    {"NEUTRAL", "😶"},      /* U+1F636 */
    {"HAPPY", "🙂"},        /* U+1F642 */
    {"LAUGHING", "😆"},     /* U+1F606 */
    {"FUNNY", "😂"},        /* U+1F602 */
    {"SAD", "😔"},          /* U+1F614 */
    {"ANGRY", "😠"},        /* U+1F620 */
    {"FEARFUL", "😭"},      /* U+1F62D */
    {"LOVING", "😍"},       /* U+1F60D */
    {"EMBARRASSED", "😳"},  /* U+1F633 */
    {"SURPRISE", "😯"},     /* U+1F62F */
    {"SHOCKED", "😱"},      /* U+1F631 */
    {"THINKING", "🤔"},     /* U+1F914 */
    {"WINK", "😉"},         /* U+1F609 */
    {"COOL", "😎"},         /* U+1F60E */
    {"RELAXED", "😌"},      /* U+1F60C */
    {"DELICIOUS", "🤤"},    /* U+1F924 */
    {"KISSY", "😘"},        /* U+1F618 */
    {"CONFIDENT", "😏"},    /* U+1F60F */
    {"SLEEP", "😴"},        /* U+1F634 */
    {"SILLY", "😜"},        /* U+1F61C */
    {"CONFUSED", "🙄"},     /* U+1F644 */
    {"TOUCH", "🤝"},        /* U+1F91D */
    {"DISAPPOINTED", "😞"}, /* U+1F61E */
    {"ANNOYED", "😒"},      /* U+1F612 */
    {"WAKEUP", "😲"},       /* U+1F632 */
    {"LEFT", "👈"},         /* U+1F448 */
    {"RIGHT", "👉"},        /* U+1F449 */

    /* --- raw UTF-8 emoji → same UTF-8 emoji (fallback for bare-emoji input) ---
     *
     * When the LLM returns a raw emoji character instead of the canonical name
     * the strcmp() in __ui_set_emotion() will match one of these entries and
     * display the correct icon.  The emo_icon pointer re-uses the same string
     * literal, so no extra memory is needed. */
    {"😶", "😶"}, /* NEUTRAL      */
    {"🙂", "🙂"}, /* HAPPY        */
    {"😆", "😆"}, /* LAUGHING     */
    {"😂", "😂"}, /* FUNNY        */
    {"😔", "😔"}, /* SAD          */
    {"😠", "😠"}, /* ANGRY        */
    {"😭", "😭"}, /* FEARFUL      */
    {"😍", "😍"}, /* LOVING       */
    {"😳", "😳"}, /* EMBARRASSED  */
    {"😯", "😯"}, /* SURPRISE     */
    {"😱", "😱"}, /* SHOCKED      */
    {"🤔", "🤔"}, /* THINKING     */
    {"😉", "😉"}, /* WINK         */
    {"😎", "😎"}, /* COOL         */
    {"😌", "😌"}, /* RELAXED      */
    {"🤤", "🤤"}, /* DELICIOUS    */
    {"😘", "😘"}, /* KISSY        */
    {"😏", "😏"}, /* CONFIDENT    */
    {"😴", "😴"}, /* SLEEP        */
    {"😜", "😜"}, /* SILLY        */
    {"🙄", "🙄"}, /* CONFUSED     */
    {"🤝", "🤝"}, /* TOUCH        */
    {"😞", "😞"}, /* DISAPPOINTED */
    {"😒", "😒"}, /* ANNOYED      */
    {"😲", "😲"}, /* WAKEUP       */
    {"👈", "👈"}, /* LEFT         */
    {"👉", "👉"}, /* RIGHT        */
};

#endif /* FONT_EMO_AWESOME */

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Get text font for UI display.
 *
 * @return Pointer to LVGL text font structure.
 */
lv_font_t *ai_ui_get_text_font(void)
{
    lv_font_t *font = NULL;

#if defined(FONT_TEXT_SIZE_14_1) && (FONT_TEXT_SIZE_14_1 == 1)
    font = (lv_font_t *)&font_puhui_14_1;
#elif defined(FONT_TEXT_SIZE_18_2) && (FONT_TEXT_SIZE_18_2 == 1)
    font = (lv_font_t *)&font_puhui_18_2;
#elif defined(FONT_TEXT_SIZE_20_4) && (FONT_TEXT_SIZE_20_4 == 1)
    font = (lv_font_t *)&font_puhui_20_4;
#elif defined(FONT_TEXT_SIZE_30_4) && (FONT_TEXT_SIZE_30_4 == 1)
    font = (lv_font_t *)&font_puhui_30_4;
#endif

    return font;
}

/**
 * @brief Get icon font for UI display.
 *
 * @return Pointer to LVGL icon font structure.
 */
lv_font_t *ai_ui_get_icon_font(void)
{
    lv_font_t *font = NULL;

#if defined(FONT_ICON_SIZE_14_1) && (FONT_ICON_SIZE_14_1 == 1)
    font = (lv_font_t *)&font_awesome_14_1;
#elif defined(FONT_ICON_SIZE_16_4) && (FONT_ICON_SIZE_16_4 == 1)
    font = (lv_font_t *)&font_awesome_16_4;
#elif defined(FONT_ICON_SIZE_20_4) && (FONT_ICON_SIZE_20_4 == 1)
    font = (lv_font_t *)&font_awesome_20_4;
#elif defined(FONT_ICON_SIZE_30_4) && (FONT_ICON_SIZE_30_4 == 1)
    font = (lv_font_t *)&font_awesome_30_4;
#endif

    return font;
}

/**
 * @brief Get emoji font for UI display.
 *
 * @return Pointer to LVGL emoji font structure.
 */
lv_font_t *ai_ui_get_emo_font(void)
{
    lv_font_t *font = NULL;

#if defined(FONT_EMOJI_SIZE_32) && (FONT_EMOJI_SIZE_32 == 1)
    extern const lv_font_t *font_emoji_32_init(void);
    font = (lv_font_t *)font_emoji_32_init();
#elif defined(FONT_EMOJI_SIZE_64) && (FONT_EMOJI_SIZE_64 == 1)
    extern const lv_font_t *font_emoji_64_init(void);
    font = (lv_font_t *)font_emoji_64_init();
#elif defined(FONT_EMO_AWESOME) && (FONT_EMO_AWESOME == 1)
    font = (lv_font_t *)&font_awesome_30_1;
#endif

    return font;
}

/**
 * @brief Get emoji list for UI display.
 *
 * Returns a pointer to the full emoji mapping table.  The table has
 * FONT_EMO_ICON_MAX_NUM entries:
 *   - Font Awesome variant: 27 entries (canonical name → FA glyph).
 *   - UTF-8 variant: 54 entries (27 name entries + 27 raw-emoji entries).
 *
 * @return Pointer to emoji list array.
 */
AI_UI_EMOJI_LIST_T *ai_ui_get_emo_list(void)
{
    AI_UI_EMOJI_LIST_T *emo_list = NULL;

#if defined(FONT_EMO_AWESOME) && (FONT_EMO_AWESOME == 1)
    emo_list = sg_awesome_emo_list;
#else
    emo_list = sg_emo_list;
#endif

    return emo_list;
}

/**
 * @brief Get WiFi icon string based on WiFi status.
 *
 * @param status WiFi status (disconnected, good, fair, weak).
 * @return Pointer to WiFi icon string.
 */
char *ai_ui_get_wifi_icon(AI_UI_WIFI_STATUS_E status)
{
    char *wifi_icon = FONT_AWESOME_WIFI_OFF;

    switch (status) {
    case AI_UI_WIFI_STATUS_DISCONNECTED:
        wifi_icon = FONT_AWESOME_WIFI_OFF;
        break;
    case AI_UI_WIFI_STATUS_GOOD:
        wifi_icon = FONT_AWESOME_WIFI;
        break;
    case AI_UI_WIFI_STATUS_FAIR:
        wifi_icon = FONT_AWESOME_WIFI_FAIR;
        break;
    case AI_UI_WIFI_STATUS_WEAK:
        wifi_icon = FONT_AWESOME_WIFI_WEAK;
        break;
    default:
        wifi_icon = FONT_AWESOME_WIFI_OFF;
        break;
    }

    return wifi_icon;
}

#endif /* ENABLE_LIBLVGL */
