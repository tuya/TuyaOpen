#ifndef LV_LIB_ANIMATION_H
#define LV_LIB_ANIMATION_H

#include "../lv_lib_conf.h"

#if LV_USE_LIB_ANIMATION 

void lv_lib_anim_callback_set_x(void * var, int32_t v);
void lv_lib_anim_callback_set_y(void * var, int32_t v);
void lv_lib_anim_callback_set_hight(void * var, int32_t v);
void lv_lib_anim_callback_set_width(void * var, int32_t v);
void lv_lib_anim_callback_set_image_angle(void * var, int32_t v);
void lv_lib_anim_callback_set_scale(void * var, int32_t v);
void lv_lib_anim_callback_set_opacity(void * var, int32_t v);
// user set animation
void lv_lib_anim_user_animation(lv_obj_t * TagetObj, uint16_t delay, uint16_t time, int16_t start_value, int16_t end_value,
                                uint16_t playback_delay, uint16_t playback_time, uint16_t repeat_delay, uint16_t repeat_count,
                                lv_anim_path_cb_t path_cb, lv_anim_exec_xcb_t exec_cb, lv_anim_completed_cb_t completed_cb);
#endif  /*LV_USE_LIB_TEMPLATE*/

#endif // LV_LIB_TEMPLATE_H