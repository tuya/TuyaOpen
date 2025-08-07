/**
 * @file toast.h
 * Toast Message Component for AI Pocket Pet
 */

#ifndef TOAST_H
#define TOAST_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../lvgl/lvgl.h"

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create the toast message container and label
 * @param parent Parent object to attach the toast to
 */
void toast_create(lv_obj_t *parent);

/**
 * Show a toast message with the given text and delay
 * @param message The text message to display
 * @param delay_ms How long to show the toast (in milliseconds, 0 for default)
 */
void toast_show(const char *message, uint32_t delay_ms);

/**
 * Hide the toast message immediately
 */
void toast_hide(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TOAST_H */
