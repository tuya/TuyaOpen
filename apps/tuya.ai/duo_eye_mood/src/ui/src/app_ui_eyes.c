/**
 * @file app_ui_eyes.c
 * @version 0.1
 * @date 2025-06-13
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
    {EMOJI_NEUTRAL, &Nature128},    {EMOJI_SURPRISE, &Surprise128},
    {EMOJI_ANGRY, &Angry128},       {EMOJI_FEARFUL, &Fearful128},
    {EMOJI_TOUCH, &Touch128},       {EMOJI_SAD, &Sad128},
    {EMOJI_THINKING, &Think128},    {EMOJI_HAPPY, &Happy128},
    {EMOJI_CONFUSED, &Confused128}, {EMOJI_DISAPPOINTED, &Disappointed128},
};

static lv_obj_t *sg_eyes_gif;

#if defined(ENABLE_DISPLAY_2) && (ENABLE_DISPLAY_2 == 1)
static lv_obj_t *sg_eyes_gif_2;
#endif

/***********************************************************
***********************function define**********************
***********************************************************/
static void __lvgl_init(void)
{
    lv_vendor_init(DISPLAY_NAME);

#if defined(ENABLE_DISPLAY_2) && (ENABLE_DISPLAY_2 == 1)
    lv_vendor_add_disp_dev(DISPLAY_NAME_2);
#endif

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

    img = __ui_eyes_get_img(EMOJI_NEUTRAL);
    if (NULL == img) {
        PR_ERR("invalid emotion: %s", EMOJI_NEUTRAL);
        return OPRT_INVALID_PARM;
    }

    lv_vendor_disp_lock();

    lv_disp_t * disp1 = lv_disp_get_next(NULL);
    if(NULL == disp1) {
        PR_ERR("get display 1 failed");
        lv_vendor_disp_unlock();
        return OPRT_COM_ERROR;
    }

    sg_eyes_gif = lv_gif_create(lv_disp_get_scr_act(disp1));
    lv_gif_set_src(sg_eyes_gif, img);
    lv_obj_align(sg_eyes_gif, LV_ALIGN_CENTER, 0, 0);

#if defined(ENABLE_DISPLAY_2) && (ENABLE_DISPLAY_2 == 1)
    lv_disp_t * disp2 = lv_disp_get_next(disp1); 
    if(NULL == disp2) {
        PR_ERR("get display 2 failed");
        lv_vendor_disp_unlock();
        return OPRT_COM_ERROR;
    }

    sg_eyes_gif_2 = lv_gif_create(lv_disp_get_scr_act(disp2));
    lv_gif_set_src(sg_eyes_gif_2, img);
    lv_obj_align(sg_eyes_gif_2, LV_ALIGN_CENTER, 0, 0);
#endif

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

#if defined(ENABLE_DISPLAY_2) && (ENABLE_DISPLAY_2 == 1)
    lv_gif_set_src(sg_eyes_gif_2, img);
#endif

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

