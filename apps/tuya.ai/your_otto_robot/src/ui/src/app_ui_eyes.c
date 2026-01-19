/**
 * @file app_ui_eyes.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#include "ai_ui_manage.h"

#include "lvgl.h"
#include "lv_vendor.h"

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
#if defined(APP_UI_EYES_240_240) && (APP_UI_EYES_240_240 == 1)
// 240x240 screen image declarations
LV_IMG_DECLARE(staticstate);  // NEUTRAL
LV_IMG_DECLARE(anger);        // ANGRY
LV_IMG_DECLARE(scare);        // FEARFUL
LV_IMG_DECLARE(sad);          // SAD
LV_IMG_DECLARE(happy);        // HAPPY
LV_IMG_DECLARE(buxue);        // Additional emotion

static const UI_EYES_EMOJI_T cEYES_EMOJI_LIST[] = {
    {EMOJI_NEUTRAL,      &staticstate},
    {EMOJI_ANGRY,        &anger},
    {EMOJI_FEARFUL,      &scare},
    {EMOJI_SAD,          &sad},
    {EMOJI_HAPPY,        &happy},
    // TODO: Missing emotions - using happy as placeholder
    {EMOJI_SURPRISE,     &happy},  // TODO: Need surprise240.c
    {EMOJI_TOUCH,        &happy},  // TODO: Need touch240.c
    {EMOJI_THINKING,     &happy},  // TODO: Need thinking240.c
    {EMOJI_CONFUSED,     &happy},  // TODO: Need confused240.c
    {EMOJI_DISAPPOINTED, &happy},  // TODO: Need disappointed240.c
};

#elif defined(APP_UI_EYES_160_80) && (APP_UI_EYES_160_80 == 1)
// 160x80 screen image declarations
LV_IMG_DECLARE(Neutral);
LV_IMG_DECLARE(Touched);
LV_IMG_DECLARE(Angry);
LV_IMG_DECLARE(Fearful);
LV_IMG_DECLARE(Surprise);
LV_IMG_DECLARE(Sad);
LV_IMG_DECLARE(Think);
LV_IMG_DECLARE(Happy);
LV_IMG_DECLARE(Confused);
LV_IMG_DECLARE(Disappointed);

static const UI_EYES_EMOJI_T cEYES_EMOJI_LIST[] = {
    {EMOJI_NEUTRAL,      &Neutral},
    {EMOJI_SURPRISE,     &Surprise},
    {EMOJI_ANGRY,        &Angry},
    {EMOJI_FEARFUL,      &Fearful},
    {EMOJI_TOUCH,        &Touched},
    {EMOJI_SAD,          &Sad},
    {EMOJI_THINKING,     &Think},
    {EMOJI_HAPPY,        &Happy},
    {EMOJI_CONFUSED,     &Confused},
    {EMOJI_DISAPPOINTED, &Disappointed},
};

#elif defined(APP_UI_EYES_160_160) && (APP_UI_EYES_160_160 == 1)
// 160x160 screen image declarations
LV_IMG_DECLARE(Neutral);
LV_IMG_DECLARE(Touched);
LV_IMG_DECLARE(Angry);
LV_IMG_DECLARE(Fearful);
LV_IMG_DECLARE(Surprise);
LV_IMG_DECLARE(Sad);
LV_IMG_DECLARE(Think);
LV_IMG_DECLARE(Happy);
LV_IMG_DECLARE(Confused);
LV_IMG_DECLARE(Disappointed);

static const UI_EYES_EMOJI_T cEYES_EMOJI_LIST[] = {
    {EMOJI_NEUTRAL,      &Neutral},
    {EMOJI_SURPRISE,     &Surprise},
    {EMOJI_ANGRY,        &Angry},
    {EMOJI_FEARFUL,      &Fearful},
    {EMOJI_TOUCH,        &Touched},
    {EMOJI_SAD,          &Sad},
    {EMOJI_THINKING,     &Think},
    {EMOJI_HAPPY,        &Happy},
    {EMOJI_CONFUSED,     &Confused},
    {EMOJI_DISAPPOINTED, &Disappointed},
};

#elif defined(APP_UI_EYES_128_128) && (APP_UI_EYES_128_128 == 1)
// 128x128 screen image declarations (original code)
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

#else
     #error "please choose eyes ui size format in Kconfig"
#endif

static lv_obj_t *sg_eyes_gif;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __lvgl_init(void)
{
    lv_vendor_init(DISPLAY_NAME);

    lv_vendor_start(5, 1024*8);
}


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

static int __ui_init(void)
{
    lv_img_dsc_t *img = NULL;

    __lvgl_init();

    lv_vendor_disp_lock();

    sg_eyes_gif = lv_gif_create(lv_scr_act());
    img = __ui_eyes_get_img(EMOJI_NEUTRAL);
    if (NULL == img) {
        PR_ERR("invalid emotion: %s", EMOJI_NEUTRAL);
        lv_vendor_disp_unlock();

        return OPRT_INVALID_PARM;
    }

    lv_gif_set_src(sg_eyes_gif, img);
    lv_obj_align(sg_eyes_gif, LV_ALIGN_CENTER, 0, 0);
    lv_vendor_disp_unlock();

    return OPRT_OK;
}

static void __ui_set_emotion(char *emotion)
{
    lv_img_dsc_t *img = NULL;

    img = __ui_eyes_get_img((char *)emotion);
    if (NULL == img) {
        PR_ERR("invalid emotion: %s", emotion);
        return;
    }

    lv_vendor_disp_lock();
    lv_gif_set_src(sg_eyes_gif, img);
    lv_vendor_disp_unlock();

    return;
}

OPERATE_RET ai_ui_eyes_register(void)
{
    AI_UI_INTFS_T intfs;

    memset(&intfs, 0, sizeof(AI_UI_INTFS_T));

    intfs.disp_init                = __ui_init;
    intfs.disp_emotion             = __ui_set_emotion;

    return ai_ui_register(&intfs);
}