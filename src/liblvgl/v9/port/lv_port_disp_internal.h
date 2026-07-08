/**
 * @file lv_port_disp_internal.h
 * @brief Internal shared types and interface for display flush implementations (LVGL v9)
 */

#ifndef LV_PORT_DISP_INTERNAL_H
#define LV_PORT_DISP_INTERNAL_H

#include <stdbool.h>
#include "lv_port_disp.h"
#include "lv_vendor.h"

#include "tkl_memory.h"
#include "tal_api.h"
#include "tuya_list.h"
#include "tdl_display_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/
/* 64 = cache-line size on ESP32-P4; keeps LVGL draw buffers cache-line aligned */
#define DISP_DRAW_BUF_ALIGN    64

/* General large-buffer allocation (rotate buf, etc.) - always PSRAM when available */
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define LV_MEM_CUSTOM_ALLOC   tal_psram_malloc
#define LV_MEM_CUSTOM_FREE    tal_psram_free
#define LV_MEM_CUSTOM_REALLOC tal_psram_realloc
#else
#define LV_MEM_CUSTOM_ALLOC   tal_malloc
#define LV_MEM_CUSTOM_FREE    tal_free
#define LV_MEM_CUSTOM_REALLOC tal_realloc
#endif

/* Draw buffer allocation - SRAM for higher throughput when configured */
#if defined(ENABLE_LVGL_DRAW_BUF_PSRAM) && (ENABLE_LVGL_DRAW_BUF_PSRAM == 1)
#define LV_DRAW_BUF_ALLOC     tal_psram_malloc
#define LV_DRAW_BUF_FREE      tal_psram_free
#else
#define LV_DRAW_BUF_ALLOC     tal_malloc
#define LV_DRAW_BUF_FREE      tal_free
#endif

#define LV_DISP_FB_MAX_NUM     3

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    struct tuya_list_head   node;
    bool                    is_enable_flush;
    lv_display_t           *lv_disp;
    TDL_DISP_HANDLE_T       dev_hdl;
    TDL_DISP_DEV_INFO_T     dev_info;
    MUTEX_HANDLE            mutex;
    uint8_t                *buf_2_1;
    uint8_t                *buf_2_2;
    uint8_t                *rotate_buf;
    TDL_DISP_FRAME_BUFF_T  *disp_fb;      /* full-frame: current fb from fb_mag */
    TDL_FB_MANAGE_HANDLE_T  fb_mag;       /* full-frame: fb pool */
    TDL_DISP_FRAME_BUFF_T   partial_fb;   /* partial: reusable flush descriptor */
} LV_DISP_NODE_T;

/**********************
 * FLUSH IMPL INTERFACE
 **********************/

/**
 * @brief Initialize flush-specific resources for a display node
 */
void lv_port_flush_init(LV_DISP_NODE_T *node);

/**
 * @brief Execute flush operation
 */
void lv_port_flush_execute(LV_DISP_NODE_T *node, lv_display_t *disp,
                           const lv_area_t *area, uint8_t *color_ptr);

/**
 * @brief Called when display update is re-enabled
 */
void lv_port_flush_on_enable(LV_DISP_NODE_T *node);

/**
 * @brief Release flush-specific resources
 */
void lv_port_flush_release(LV_DISP_NODE_T *node);

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_DISP_INTERNAL_H */
