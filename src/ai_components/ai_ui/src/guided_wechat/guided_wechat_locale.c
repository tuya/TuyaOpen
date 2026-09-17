/**
 * @file guided_wechat_locale.c
 * @brief Runtime language persistence for the guided WeChat overlay.
 */

#include "tal_api.h"
#include "ai_ui_locale.h"
#include "guided_wechat_locale.h"

bool guided_wechat_locale_load(void)
{
    return ai_ui_locale_load();
}

guided_wechat_lang_t guided_wechat_locale_get(void)
{
    return ai_ui_locale_is_english() ? GUIDED_WECHAT_LANG_EN : GUIDED_WECHAT_LANG_ZH;
}

void guided_wechat_locale_set(guided_wechat_lang_t lang)
{
    ai_ui_locale_set_english(lang == GUIDED_WECHAT_LANG_EN);
}

const char *guided_wechat_tr(const char *zh, const char *en)
{
    return ai_ui_locale_tr(zh, en);
}
