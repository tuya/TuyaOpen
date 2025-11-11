#include "ui_templatePage.h"

///////////////////// VARIABLES ////////////////////


///////////////////// ANIMATIONS ////////////////////


///////////////////// FUNCTIONS ////////////////////


///////////////////// SCREEN init ////////////////////

void ui_templatePage_init()
{
    lv_obj_t * obj = lv_obj_create(NULL);

    // load page
    lv_scr_load_anim(obj, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

/////////////////// SCREEN deinit ////////////////////

void ui_templatePage_deinit()
{
    // deinit
}