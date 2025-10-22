/**
 * @file ui_eyes.c
 * @version 0.1
 * @date 2025-06-13
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#if defined(ENABLE_GUI_EYES) && (ENABLE_GUI_EYES == 1)
#include "ui_display.h"

#include "lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char *name;
    const lv_img_dsc_t *img;
} UI_EYES_EMOJI_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
LV_IMG_DECLARE(Nature128);
LV_IMG_DECLARE(Touch128);
LV_IMG_DECLARE(Angry128);
LV_IMG_DECLARE(Fearful128);
LV_IMG_DECLARE(Surprise128);
LV_IMG_DECLARE(Sad128);
LV_IMG_DECLARE(Think128);
LV_IMG_DECLARE(Happy128);
LV_IMG_DECLARE(Confused128);
LV_IMG_DECLARE(Disappointed128);

static const UI_EYES_EMOJI_T cEYES_EMOJI_LIST[] = {
    {EMOJI_NEUTRAL,      &Nature128},
    {EMOJI_SURPRISE,     &Surprise128},
    {EMOJI_ANGRY,        &Angry128},
    {EMOJI_FEARFUL,      &Fearful128},
    {EMOJI_TOUCH,        &Touch128},
    {EMOJI_SAD,          &Sad128},
    {EMOJI_THINKING,     &Think128},
    {EMOJI_HAPPY,        &Happy128},
    {EMOJI_CONFUSED,     &Confused128},
    {EMOJI_DISAPPOINTED, &Disappointed128},
};

static lv_obj_t *sg_eyes_gif;

/***********************************************************
***********************function define**********************
***********************************************************/
static lv_img_dsc_t *__ui_eyes_get_img(char *name)
{
    int i = 0;

    for (i = 0; i < CNTSOF(cEYES_EMOJI_LIST); i++) {
        if (0 == strcasecmp(cEYES_EMOJI_LIST[i].name, name)) {
            return (lv_img_dsc_t *)cEYES_EMOJI_LIST[i].img;
        }
    }

    return NULL;
}

int ui_init(UI_FONT_T *ui_font)
{
    lv_img_dsc_t *img = NULL;

    sg_eyes_gif = lv_gif_create(lv_scr_act());
    img = __ui_eyes_get_img(EMOJI_NEUTRAL);
    if(NULL == img) {
        PR_ERR("invalid emotion: %s", EMOJI_NEUTRAL);
        return OPRT_INVALID_PARM;
    }

    lv_gif_set_src(sg_eyes_gif, img);
    lv_obj_align(sg_eyes_gif, LV_ALIGN_CENTER, 0, 0);

    return OPRT_OK;
}

void ui_set_emotion(const char *emotion)
{
    lv_img_dsc_t *img = NULL;
    
    img = __ui_eyes_get_img((char *)emotion);
    if(NULL == img) {
        PR_ERR("invalid emotion: %s", emotion);
        return;
    }

    lv_gif_set_src(sg_eyes_gif, img);

    return;
}


void ui_set_user_msg(const char *text)
{
    return;
}

void ui_set_assistant_msg(const char *text)
{
    return;
}

void ui_set_system_msg(const char *text)
{
    return;
}

void ui_set_status(const char *status)
{
    return;
}

void ui_set_notification(const char *notification)
{
    return;
}

void ui_set_network(char *wifi_icon)
{
    return;
}

void ui_set_chat_mode(const char *chat_mode)
{
    return;
}

void ui_set_status_bar_pad(int32_t value)
{
    return;
}

#endif