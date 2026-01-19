/**
 * @file ai_ui_icon_font.c 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
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
// Declare all possible fonts at file scope
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
static AI_UI_EMOJI_LIST_T sg_awesome_emo_list[] = {
    {"NEUTRAL",  FONT_AWESOME_EMOJI_NEUTRAL},  
    {"SAD",      FONT_AWESOME_EMOJI_SAD},
    {"ANGRY",    FONT_AWESOME_EMOJI_ANGRY},       
    {"SURPRISE", FONT_AWESOME_EMOJI_SURPRISED},
    {"CONFUSED", FONT_AWESOME_EMOJI_CONFUSED}, 
    {"THINKING", FONT_AWESOME_EMOJI_THINKING},
    {"HAPPY",    FONT_AWESOME_EMOJI_HAPPY},
};
#else
static  AI_UI_EMOJI_LIST_T sg_emo_list[] = {
    {"NEUTRAL",  "😶"}, 
    {"SAD",      "😔"},      
    {"ANGRY",    "😠"}, 
    {"SURPRISE", "😯"},
    {"CONFUSED", "😏"}, 
    {"THINKING", "🤔"}, 
    {"HAPPY",    "🙂"},
};
#endif

/***********************************************************
***********************function define**********************
***********************************************************/
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

#endif