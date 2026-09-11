/**
 * @file lv_port_disp_direct.c
 * @brief Direct (zero-copy) flush for LVGL v9: render into the driver's vram fbs
 *
 * Compiled only when ENABLE_LVGL_DIRECT_FLUSH == 1. The two fbs come from
 * tdl_disp_get_vram(); LVGL renders into them with RENDER_MODE_DIRECT and the
 * flush only commits the scanned buffer - the tdd waits for the scan switch
 * (anti-tearing) and no copy happens anywhere.
 */

#include "lv_port_disp_internal.h"

#if defined(ENABLE_LVGL_DIRECT_FLUSH) && (ENABLE_LVGL_DIRECT_FLUSH == 1)

/**********************
 *  INTERFACE IMPL
 **********************/

OPERATE_RET lv_port_flush_init(LV_DISP_NODE_T *node)
{
    void *vram_fbs[2] = {NULL};
    uint8_t vram_num = 0;
    OPERATE_RET rt = OPRT_OK;

    if (TUYA_DISPLAY_ROTATION_0 != node->dev_info.rotation) {
        /* direct renders in place: no rotate buffer exists */
        PR_ERR("direct flush needs rotation 0, got %d", node->dev_info.rotation);
        return OPRT_NOT_SUPPORTED;
    }

    if (node->dev_info.is_swap) {
        PR_ERR("direct flush does not support byte-swapped panels");
        return OPRT_NOT_SUPPORTED;
    }

    /* strict: no driver vram double-buffer, no fallback */
    rt = tdl_disp_get_vram(node->dev_hdl, 2, vram_fbs, &vram_num);
    if (OPRT_OK != rt || vram_num < 2) {
        PR_ERR("direct flush needs >=2 driver vram fbs, rt:%d num:%d", rt, vram_num);
        return (OPRT_OK == rt) ? OPRT_NOT_SUPPORTED : rt;
    }

    for (uint8_t i = 0; i < 2; i++) {
        node->direct_fb[i] = tkl_system_malloc(sizeof(TDL_DISP_FRAME_BUFF_T));
        if (NULL == node->direct_fb[i]) {
            lv_port_flush_release(node);
            return OPRT_MALLOC_FAILED;
        }
        memset(node->direct_fb[i], 0, sizeof(TDL_DISP_FRAME_BUFF_T));
        node->direct_fb[i]->type   = DISP_FB_TP_VRAM;
        node->direct_fb[i]->fmt    = node->dev_info.fmt;
        node->direct_fb[i]->width  = node->dev_info.width;
        node->direct_fb[i]->height = node->dev_info.height;
        node->direct_fb[i]->frame  = vram_fbs[i];
        node->direct_fb[i]->len    = node->dev_info.width * node->dev_info.height *
                                     ((tdl_disp_get_fmt_bpp(node->dev_info.fmt) + 7) / 8);
    }

    PR_NOTICE("direct flush: fb0=%p fb1=%p", vram_fbs[0], vram_fbs[1]);

    return OPRT_OK;
}

OPERATE_RET lv_port_disp_set_buffers(LV_DISP_NODE_T *node, lv_display_t *disp)
{
    /* LVGL renders straight into the driver's vram fbs, no port-owned draw buf */
    lv_display_set_buffers(disp, node->direct_fb[0]->frame,
                           node->direct_fb[1]->frame,
                           node->direct_fb[0]->len,
                           LV_DISPLAY_RENDER_MODE_DIRECT);

    return OPRT_OK;
}

void lv_port_flush_execute(LV_DISP_NODE_T *node, lv_display_t *disp,
                           const lv_area_t *area, uint8_t *color_ptr)
{
    TDL_DISP_FRAME_BUFF_T *fb = NULL;

    (void)area;

    if (lv_display_flush_is_last(disp)) {
        /* px_map is the buffer base in direct mode. The first LVGL frame may
         * re-commit the fb the panel already scans: worst case one
         * frame-buf-complete timeout, self-resolves after the first swap. */
        if (color_ptr == node->direct_fb[0]->frame) {
            fb = node->direct_fb[0];
        } else if (color_ptr == node->direct_fb[1]->frame) {
            fb = node->direct_fb[1];
        } else {
            PR_ERR("flush buf %p is not a vram fb", color_ptr);
            lv_display_flush_ready(disp);
            return;
        }

        tdl_disp_dev_flush(node->dev_hdl, fb);
    }

    lv_display_flush_ready(disp);
}

void lv_port_flush_on_enable(LV_DISP_NODE_T *node)
{
    if (node->direct_fb[0]) {
        /* first commit: the panel is parked on fb1, so this is a real switch */
        tdl_disp_dev_flush(node->dev_hdl, node->direct_fb[0]);
    }
}

void lv_port_flush_release(LV_DISP_NODE_T *node)
{
    for (uint8_t i = 0; i < 2; i++) {
        if (node->direct_fb[i]) {
            tkl_system_free(node->direct_fb[i]); /* shell only; the vram belongs to the driver */
            node->direct_fb[i] = NULL;
        }
    }
}

/* Direct mode uses no DMA2D (nothing to copy); hooks stay for a uniform API. */
void lv_port_flush_dma2d_deinit(void)
{
}

void lv_port_flush_dma2d_reinit(void)
{
}

#endif /* ENABLE_LVGL_DIRECT_FLUSH */
