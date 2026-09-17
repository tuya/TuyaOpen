/**
 * @file ai_ui_locale.c
 * @brief Runtime language preference shared by all AI UI variants.
 */

#include "tal_api.h"
#include "tal_kv.h"
#include "ai_ui_locale.h"

#define AI_UI_LOCALE_KEY "guided_wechat_lang"

static bool sg_english;

bool ai_ui_locale_load(void)
{
    uint8_t *value = NULL;
    size_t len = 0;
    OPERATE_RET rt;

    sg_english = false;
    rt = tal_kv_get(AI_UI_LOCALE_KEY, &value, &len);
    if (rt != OPRT_OK || value == NULL || len == 0) {
        if (value != NULL) {
            tal_kv_free(value);
        }
        return false;
    }

    sg_english = value[0] == '1';
    tal_kv_free(value);
    return true;
}

bool ai_ui_locale_is_english(void)
{
    return sg_english;
}

void ai_ui_locale_set_english(bool english)
{
    uint8_t value = english ? '1' : '0';

    sg_english = english;
    if (OPRT_OK != tal_kv_set(AI_UI_LOCALE_KEY, &value, 1)) {
        PR_WARN("AI UI language save failed");
    }
}

const char *ai_ui_locale_tr(const char *zh, const char *en)
{
    return sg_english ? en : zh;
}
