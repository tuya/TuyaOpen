/**
 * @file ui_emoji.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "lvgl.h"
#include "ai_ui_manage.h"
#include "app_ui.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define EMMO_GIF_W           160
#define EMMO_GIF_H           80
#define EMMO_CHANGE_INTERVAL (5*1000) // Expression switching interval (5 seconds)

#define GIF_EMOJI_NAME_NUM   5
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    const lv_img_dsc_t *img;
    char* name[GIF_EMOJI_NAME_NUM];
}UI_GIF_EMOJI_T;

typedef struct {
    lv_obj_t     *gif;
    lv_obj_t     *container;
}AI_UI_EMOJI_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
// Declare all emoji GIF animations (optimized, faster animations + fun expressions)
LV_IMG_DECLARE(happy);
LV_IMG_DECLARE(sad);
LV_IMG_DECLARE(anger);
LV_IMG_DECLARE(surprise);
LV_IMG_DECLARE(sleep);
LV_IMG_DECLARE(wakeup);
LV_IMG_DECLARE(left);
LV_IMG_DECLARE(right);
LV_IMG_DECLARE(center);
// Fun expressions
LV_IMG_DECLARE(wink);
LV_IMG_DECLARE(heart_eyes);
LV_IMG_DECLARE(rolling);
LV_IMG_DECLARE(zigzag);
LV_IMG_DECLARE(rainbow);

/* Desk-Emoji inspired expressions converted to LVGL GIF animations
   Based on geometric eye system with various emotional states
   Optimized for faster animation speed (50ms duration)
   Includes 5 fun new expressions for enhanced interactivity */
static const UI_GIF_EMOJI_T cGIF_EMOTION[] = {
    // Basic emotions
    {&happy,       {EMOJI_HAPPY,    NULL,           NULL,               NULL, NULL} },
    {&sad,         {EMOJI_SAD,      EMOJI_FEARFUL,  EMOJI_DISAPPOINTED, NULL, NULL} },
    {&anger,       {EMOJI_ANGRY,    EMOJI_ANNOYED,  NULL,               NULL, NULL} },
    {&surprise,    {EMOJI_SURPRISE, EMOJI_CONFUSED, NULL,               NULL, NULL} },
    {&sleep,       {EMOJI_SLEEP,    NULL,           NULL,               NULL, NULL} },
    {&wakeup,      {EMOJI_WAKEUP,   NULL,           NULL,               NULL, NULL} },
    {&left,        {EMOJI_LEFT,     NULL,           NULL,               NULL, NULL} },
    {&right,       {EMOJI_RIGHT,    NULL,           NULL,               NULL, NULL} },
    {&center,      {"center",       EMOJI_NEUTRAL,  EMOJI_THINKING,     NULL, NULL} },
    // Fun expressions
    {&wink,        {EMOJI_WINK,     NULL,           NULL,               NULL, NULL} },
    {&heart_eyes,  {"heart_eyes",   EMOJI_TOUCH,    NULL,               NULL, NULL} },
    {&rolling,     {"rolling",      NULL,           NULL,               NULL, NULL} },
    {&zigzag,      {"zigzag",       NULL,           NULL,               NULL, NULL} },
    {&rainbow,     {"rainbow",      NULL,           NULL,               NULL, NULL} },
};

static AI_UI_EMOJI_T sg_ui;
/***********************************************************
***********************function define**********************
***********************************************************/
static const lv_img_dsc_t *__ui_get_emoji_gif(char *name)
{
    for (int i = 0; i < CNTSOF(cGIF_EMOTION); i++) {
        for(int j = 0; j < GIF_EMOJI_NAME_NUM; j++) {
            if(cGIF_EMOTION[i].name[j] == NULL) {
                continue;
            }

            if (0 == strcasecmp(cGIF_EMOTION[i].name[j], name)) {
                return cGIF_EMOTION[i].img;
            }
        }
    }

    return NULL;
}

const lv_img_dsc_t *__ui_emoji_get_next_img(const lv_img_dsc_t *cur_gif)
{
    if(cur_gif == NULL) {
        return cGIF_EMOTION[0].img;
    }

    for (int i = 0; i < CNTSOF(cGIF_EMOTION); i++) {
        if(cGIF_EMOTION[i].img == cur_gif) {
            int next_index = (i + 1) % CNTSOF(cGIF_EMOTION);
            return cGIF_EMOTION[next_index].img;
        }
    }

    return cGIF_EMOTION[0].img;
}

int __ui_emoji_init(void)
{
    sg_ui.container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sg_ui.container, EMMO_GIF_W, EMMO_GIF_H);
    lv_obj_set_style_pad_all(sg_ui.container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.container, 0, 0);

    sg_ui.gif = lv_gif_create(sg_ui.container);
    lv_obj_set_size(sg_ui.gif, EMMO_GIF_W, EMMO_GIF_H);
    const lv_img_dsc_t *gif_img = __ui_get_emoji_gif(EMOJI_HAPPY);
    if(gif_img) {
        lv_gif_set_src(sg_ui.gif, gif_img);
    }else {
        PR_ERR("Failed to get initial emoji gif");
    }
    
    // Initially hide the emoji UI
    lv_obj_add_flag(sg_ui.container, LV_OBJ_FLAG_HIDDEN);
    
    PR_DEBUG("=== EMOJI UI INIT COMPLETE ===");

    return 0;
}

void __ui_emoji_show(void)
{
    lv_obj_clear_flag(sg_ui.container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(sg_ui.container);
    PR_DEBUG("Emoji UI shown");
}

void __ui_emoji_hide(void)
{
    lv_obj_add_flag(sg_ui.container, LV_OBJ_FLAG_HIDDEN);
    PR_DEBUG("Emoji UI hidden");
}

const lv_img_dsc_t *__ui_emoji_set_emotion(char *emotion)
{
    const lv_img_dsc_t *gif = NULL;
    
    gif = __ui_get_emoji_gif(emotion);
    if(NULL == gif) {
        PR_ERR("Emotion '%s' not found, defaulting to happy", emotion ? emotion : "NULL");
        gif = __ui_get_emoji_gif(EMOJI_HAPPY);
    }

    lv_gif_set_src(sg_ui.gif, gif);

    return gif;
}

void __ui_emoji_set_img(const lv_img_dsc_t *gif)
{
    lv_gif_set_src(sg_ui.gif, gif);
}