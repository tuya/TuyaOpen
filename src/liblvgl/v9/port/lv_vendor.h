/**
 * @file lvgl_vendor.h
 */

#ifndef LVGL_VENDOR_H
#define LVGL_VENDOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef enum{
    STATE_INIT,
    STATE_RUNNING,
    STATE_STOP
} lvgl_task_state_t;

void lv_vendor_init(void *device);
void lv_vendor_start(uint32_t lvgl_task_pri, uint32_t lvgl_stack_size);
void lv_vendor_stop(void);
void lv_vendor_disp_lock(void);
void lv_vendor_disp_unlock(void);
void lv_vendor_add_disp_dev(void *device);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LVGL_VENDOR_H*/

