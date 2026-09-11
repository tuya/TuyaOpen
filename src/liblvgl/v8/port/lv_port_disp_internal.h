/**
 * @file lv_port_disp_internal.h
 * @brief Internal shared types and interface for display flush implementations
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
#define DISP_DRAW_BUF_ALIGN    4
#define LV_DISP_FB_MAX_NUM     3

/* Draw buffer allocation - SRAM for higher throughput when configured */
#if defined(ENABLE_LVGL_DRAW_BUF_PSRAM) && (ENABLE_LVGL_DRAW_BUF_PSRAM == 1)
#define LV_DRAW_BUF_ALLOC     tkl_system_psram_malloc
#define LV_DRAW_BUF_FREE      tkl_system_psram_free
#else
#define LV_DRAW_BUF_ALLOC     tkl_system_malloc
#define LV_DRAW_BUF_FREE      tkl_system_free
#endif

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    struct tuya_list_head   node;
    bool                    is_enable_flush;
    bool                    is_add_rotate_buf;
    lv_disp_drv_t           lv_disp_drv;
    lv_disp_draw_buf_t      draw_buf_dsc;
    lv_disp_t              *lv_disp;
    TDL_DISP_HANDLE_T       dev_hdl;
    TDL_DISP_DEV_INFO_T     dev_info;
    MUTEX_HANDLE            mutex;
    uint8_t                *buf_2_1;
    uint8_t                *buf_2_2;
    uint8_t                *rotate_buf;
    TDL_DISP_FRAME_BUFF_T  *disp_fb;      /* full-frame: current fb from fb_mag */
    TDL_FB_MANAGE_HANDLE_T  fb_mag;       /* full-frame: fb pool */
    TDL_DISP_FRAME_BUFF_T   partial_fb;   /* partial: reusable flush descriptor */
    TDL_DISP_FRAME_BUFF_T  *direct_fb[2]; /* direct: shells over the driver's vram fbs */
} LV_DISP_NODE_T;

/**********************
 * FLUSH IMPL INTERFACE
 *
 * Each flush strategy (partial / full-frame / direct) implements these
 * functions. Only one is compiled, by the LIBLVGL_FLUSH_MODE choice.
 **********************/

/**
 * @brief Initialize flush-specific resources for a display node
 * @brief direct mode fails when the driver offers no vram fbs (no fallback)
 */
OPERATE_RET lv_port_flush_init(LV_DISP_NODE_T *node);

/**
 * @brief Set up the LVGL draw buffers for the active flush mode
 * @brief each mode file implements its own: partial/full allocate, direct
 *        hands the driver's vram fbs to LVGL. Called after lv_disp_drv_init
 *        and before lv_disp_drv_register.
 */
OPERATE_RET lv_port_disp_set_buffers(LV_DISP_NODE_T *node);

/**
 * @brief Aligned draw buffer allocation (shared by the mode files)
 */
uint8_t *lv_port_disp_draw_buf_alloc(uint32_t size_bytes);

/**
 * @brief Execute flush operation
 *
 * For full-frame: fills framebuffer, flushes on last
 *                 chunk, calls lv_disp_flush_ready().
 * For partial:    sends partial region to driver,
 *                 lv_disp_flush_ready() called by free_cb.
 *
 * Caller holds node->mutex. This function must NOT unlock it.
 */
void lv_port_flush_execute(LV_DISP_NODE_T *node, lv_disp_drv_t *disp_drv,
                           const lv_area_t *area, uint8_t *color_ptr);

/**
 * @brief Called when display update is re-enabled (after disable)
 *
 * Caller holds node->mutex.
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
