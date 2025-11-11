#ifndef  LV_LIB_H
#define  LV_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_lib_conf.h"

#if LV_USE_LIB_STACK
#include "stack/lv_lib_stack.h"
#endif

#if LV_USE_LIB_PAGE_MANAGER
#include "page_manager/lv_lib_pm.h"
#endif

#if LV_USE_LIB_ANIMATION
#include "animation/lv_lib_animation.h"
#endif

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif // LV_LIB_H