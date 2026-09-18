/**
 * @file guided_wechat_locale.h
 * @brief Small runtime locale store for the guided WeChat overlay.
 */

#ifndef __GUIDED_WECHAT_LOCALE_H__
#define __GUIDED_WECHAT_LOCALE_H__

#include <stdbool.h>

typedef enum {
    GUIDED_WECHAT_LANG_ZH = 0,
    GUIDED_WECHAT_LANG_EN,
} guided_wechat_lang_t;

bool guided_wechat_locale_load(void);
guided_wechat_lang_t guided_wechat_locale_get(void);
void guided_wechat_locale_set(guided_wechat_lang_t lang);
const char *guided_wechat_tr(const char *zh, const char *en);

#endif /* __GUIDED_WECHAT_LOCALE_H__ */
