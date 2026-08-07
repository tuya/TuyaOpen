/**
 * @file lv_port_disp.h
 *
 */

/*Copy this file as "lv_port_disp.h" and set this value to "1" to enable content*/
#if 1

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/* Initialize low level display driver */
void lv_port_disp_init(char *device);

void lv_port_disp_deinit(char *device);

/* Low-power hooks for the flush backend's DMA2D (full-frame; no-op on partial).
   Release powers the HW down for suspend; reinit brings it back before resuming flush. */
void lv_port_flush_dma2d_deinit(void);
void lv_port_flush_dma2d_reinit(void);

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(lv_disp_t *lv_disp);

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(lv_disp_t *lv_disp);

/**
 * @brief Sets the display backlight brightness
 * 
 * @param brightness Brightness level (0-100)
 */
void disp_set_backlight(lv_disp_t *lv_disp, uint8_t brightness);


lv_disp_t *lv_port_get_lv_disp_by_name(char *device);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_PORT_DISP_H*/

#endif /*Disable/Enable content*/
