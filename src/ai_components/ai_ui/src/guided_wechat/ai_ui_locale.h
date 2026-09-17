/**
 * @file ai_ui_locale.h
 * @brief Runtime language preference shared by the AI UI and onboarding UI.
 */

#ifndef __AI_UI_LOCALE_H__
#define __AI_UI_LOCALE_H__

#include <stdbool.h>

bool ai_ui_locale_load(void);
bool ai_ui_locale_is_english(void);
void ai_ui_locale_set_english(bool english);
const char *ai_ui_locale_tr(const char *zh, const char *en);

#endif /* __AI_UI_LOCALE_H__ */
