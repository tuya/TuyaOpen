/**
 * @file lv_port_disp_partial.c
 * @brief Partial (region-based) flush implementation for LVGL v9 displays with VRAM
 *
 * Compiled only when ENABLE_LVGL_PARTIAL_FLUSH == 1.
 */

#include "lv_port_disp_internal.h"

#if defined(ENABLE_LVGL_PARTIAL_FLUSH) && (ENABLE_LVGL_PARTIAL_FLUSH == 1)

/**********************
 *  STATIC FUNCTIONS
 **********************/

static void __disp_partial_flush_done(TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    LV_DISP_NODE_T *node = (LV_DISP_NODE_T *)frame_buff->free_arg;
    if (node && node->lv_disp) {
        lv_display_flush_ready(node->lv_disp);
    }
}

/**********************
 *  INTERFACE IMPL
 **********************/

void lv_port_flush_init(LV_DISP_NODE_T *node)
{
    node->partial_fb.fmt      = node->dev_info.fmt;
    node->partial_fb.free_cb  = __disp_partial_flush_done;
    node->partial_fb.free_arg = (void *)node;
}

void lv_port_flush_execute(LV_DISP_NODE_T *node, lv_display_t *disp,
                           const lv_area_t *area, uint8_t *color_ptr)
{
    lv_color_format_t cf = lv_display_get_color_format(disp);

    if (LV_COLOR_FORMAT_RGB565 == cf && node->dev_info.is_swap) {
        lv_draw_sw_rgb565_swap(color_ptr,
                               lv_area_get_width(area) * lv_area_get_height(area));
    }

    node->partial_fb.x_start = area->x1;
    node->partial_fb.y_start = area->y1;
    node->partial_fb.width   = lv_area_get_width(area);
    node->partial_fb.height  = lv_area_get_height(area);
    node->partial_fb.frame   = color_ptr;
    node->partial_fb.len     = node->partial_fb.width * node->partial_fb.height *
                               ((tdl_disp_get_fmt_bpp(node->dev_info.fmt) + 7) / 8);

    tdl_disp_dev_flush(node->dev_hdl, &node->partial_fb);

    /* lv_display_flush_ready() will be called by __disp_partial_flush_done via free_cb */
}

void lv_port_flush_on_enable(LV_DISP_NODE_T *node)
{
    (void)node;
}

void lv_port_flush_release(LV_DISP_NODE_T *node)
{
    (void)node;
}

/* Partial flush does not use DMA2D; low-power hooks are no-ops (kept for a uniform API). */
void lv_port_flush_dma2d_deinit(void)
{
}

void lv_port_flush_dma2d_reinit(void)
{
}

#endif /* ENABLE_LVGL_PARTIAL_FLUSH */
