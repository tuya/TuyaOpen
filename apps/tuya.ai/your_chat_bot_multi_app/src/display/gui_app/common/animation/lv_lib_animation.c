#include "lv_lib_animation.h"

void lv_lib_anim_callback_set_x(void * var, int32_t v)
{
    lv_obj_set_x(var, v);
}

void lv_lib_anim_callback_set_y(void * var, int32_t v)
{
    lv_obj_set_y(var, v);
}

void lv_lib_anim_callback_set_hight(void * var, int32_t v)
{
    lv_obj_set_height(var, v);
}

void lv_lib_anim_callback_set_width(void * var, int32_t v)
{
    lv_obj_set_width(var, v);
}

void lv_lib_anim_callback_set_image_angle(void * var, int32_t v)
{
    lv_image_set_rotation(var, v);
}

void lv_lib_anim_callback_set_scale(void * var, int32_t v)
{
    lv_image_set_scale(var, v);
}

void lv_lib_anim_callback_set_opacity(void * var, int32_t v)
{
    lv_obj_set_style_opa(var, v, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void lv_lib_anim_user_animation(lv_obj_t * TagetObj, uint16_t delay, uint16_t time, int16_t start_value, int16_t end_value,
                                uint16_t playback_delay, uint16_t playback_time, uint16_t repeat_delay, uint16_t repeat_count,
                                lv_anim_path_cb_t path_cb, lv_anim_exec_xcb_t exec_cb, lv_anim_completed_cb_t completed_cb)
{
    lv_anim_t Animation;
    lv_anim_init(&Animation);
    lv_anim_set_var(&Animation, TagetObj);
    lv_anim_set_time(&Animation, time);
    lv_anim_set_values(&Animation, start_value, end_value);
    lv_anim_set_exec_cb(&Animation, exec_cb);
    lv_anim_set_path_cb(&Animation, path_cb);
    lv_anim_set_delay(&Animation, delay);
    lv_anim_set_playback_time(&Animation, playback_time);
    lv_anim_set_playback_delay(&Animation, playback_delay);
    lv_anim_set_repeat_count(&Animation, repeat_count);
    lv_anim_set_repeat_delay(&Animation, repeat_delay);
    lv_anim_set_early_apply(&Animation, false);
    lv_anim_set_completed_cb(&Animation, completed_cb);
    lv_anim_start(&Animation);
}

