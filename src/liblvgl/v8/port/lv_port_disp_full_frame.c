/**
 * @file lv_port_disp_full_frame.c
 * @brief Full-frame flush implementation with framebuffer pool management
 *
 * Compiled only when ENABLE_LVGL_PARTIAL_FLUSH is NOT enabled.
 * Accumulates draw calls into a full-size framebuffer, then flushes
 * the entire frame to the display on the last LVGL chunk.
 */

#include "lv_port_disp_internal.h"

#if !defined(ENABLE_LVGL_PARTIAL_FLUSH) || (ENABLE_LVGL_PARTIAL_FLUSH != 1)

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
#include "tal_dma2d.h"

static TAL_DMA2D_HANDLE_T sg_lvgl_dma2d_hdl = NULL;
static bool               sg_dma2d_used     = false; // TRUE only when a format actually needs DMA2D

static void __dma2d_drawbuffer_memcpy_syn(const lv_area_t *area, uint8_t *px_map,
                                          TDL_DISP_FRAME_BUFF_T *fb)
{
    if (NULL == sg_lvgl_dma2d_hdl) {
        return;
    }

    TKL_DMA2D_FRAME_INFO_T in_frame = {0};
    TKL_DMA2D_FRAME_INFO_T out_frame = {0};

    if (area == NULL || px_map == NULL || fb == NULL) {
        PR_ERR("Invalid parameter");
        return;
    }

#if defined(LV_COLOR_DEPTH) && (LV_COLOR_DEPTH == 16)
    in_frame.type  = TUYA_FRAME_FMT_RGB565;
    out_frame.type = TUYA_FRAME_FMT_RGB565;
#elif defined(LV_COLOR_DEPTH) && (LV_COLOR_DEPTH == 24)
    in_frame.type  = TUYA_FRAME_FMT_RGB888;
    out_frame.type = TUYA_FRAME_FMT_RGB888;
#else
    #error "LV_COLOR_DEPTH not support"
#endif

    in_frame.width         = area->x2 - area->x1 + 1;
    in_frame.height        = area->y2 - area->y1 + 1;
    in_frame.pbuf          = px_map;
    in_frame.axis.x_axis   = 0;
    in_frame.axis.y_axis   = 0;
    in_frame.width_cp      = 0;
    in_frame.height_cp     = 0;

    out_frame.width        = fb->width;
    out_frame.height       = fb->height;
    out_frame.pbuf         = fb->frame;
    out_frame.axis.x_axis  = area->x1;
    out_frame.axis.y_axis  = area->y1;

    tal_dma2d_memcpy(sg_lvgl_dma2d_hdl, &in_frame, &out_frame);
    tal_dma2d_wait_finish(sg_lvgl_dma2d_hdl, 1000);
}

static void __dma2d_framebuffer_memcpy_async(TDL_DISP_DEV_INFO_T *dev_info,
                                             uint8_t *dst_frame,
                                             uint8_t *src_frame)
{
    if (NULL == sg_lvgl_dma2d_hdl) {
        return;
    }

    TKL_DMA2D_FRAME_INFO_T in_frame = {0};
    TKL_DMA2D_FRAME_INFO_T out_frame = {0};

    switch (dev_info->fmt) {
        case TUYA_PIXEL_FMT_RGB565:
            in_frame.type  = TUYA_FRAME_FMT_RGB565;
            out_frame.type = TUYA_FRAME_FMT_RGB565;
            break;
        case TUYA_PIXEL_FMT_RGB888:
            in_frame.type  = TUYA_FRAME_FMT_RGB888;
            out_frame.type = TUYA_FRAME_FMT_RGB888;
            break;
        default:
            PR_ERR("Unsupported color format");
            return;
    }

    in_frame.width         = dev_info->width;
    in_frame.height        = dev_info->height;
    in_frame.pbuf          = src_frame;
    in_frame.axis.x_axis   = 0;
    in_frame.axis.y_axis   = 0;
    in_frame.width_cp      = 0;
    in_frame.height_cp     = 0;

    out_frame.width        = dev_info->width;
    out_frame.height       = dev_info->height;
    out_frame.pbuf         = dst_frame;
    out_frame.axis.x_axis  = 0;
    out_frame.axis.y_axis  = 0;
    out_frame.width_cp     = 0;
    out_frame.height_cp    = 0;

    tal_dma2d_memcpy(sg_lvgl_dma2d_hdl, &in_frame, &out_frame);
}
#endif /* ENABLE_DMA2D */

/**********************
 *  STATIC HELPERS
 **********************/

static void __disp_framebuffer_memcpy(TDL_DISP_DEV_INFO_T *dev_info,
                                      uint8_t *dst_frame, uint8_t *src_frame,
                                      uint32_t frame_size)
{
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    __dma2d_framebuffer_memcpy_async(dev_info, dst_frame, src_frame);
#else
    (void)dev_info;
    memcpy(dst_frame, src_frame, frame_size);
#endif
}

static void __disp_mono_write_point(uint32_t x, uint32_t y, bool enable,
                                    TDL_DISP_FRAME_BUFF_T *fb)
{
    if (NULL == fb || x >= fb->width || y >= fb->height) {
        PR_ERR("Point (%d, %d) out of bounds", x, y);
        return;
    }

    uint32_t write_byte_index = y * (fb->width / 8) + x / 8;
    uint8_t write_bit = x % 8;

    if (enable) {
        fb->frame[write_byte_index] |= (1 << write_bit);
    } else {
        fb->frame[write_byte_index] &= ~(1 << write_bit);
    }
}

static void __disp_i2_write_point(uint32_t x, uint32_t y, uint8_t color,
                                  TDL_DISP_FRAME_BUFF_T *fb)
{
    if (NULL == fb || x >= fb->width || y >= fb->height) {
        PR_ERR("Point (%d, %d) out of bounds", x, y);
        return;
    }

    uint32_t write_byte_index = y * (fb->width / 4) + x / 4;
    uint8_t write_bit = (x % 4) * 2;
    uint8_t cleared = fb->frame[write_byte_index] & (~(0x03 << write_bit));

    fb->frame[write_byte_index] = cleared | ((color & 0x03) << write_bit);
}

static void __disp_fill_display_framebuffer(const lv_area_t *area, uint8_t *px_map,
                                            TDL_DISP_FRAME_BUFF_T *fb, bool is_swap)
{
    uint32_t offset = 0, x = 0, y = 0;

    if (NULL == area || NULL == px_map || NULL == fb) {
        PR_ERR("Invalid parameters: area or px_map or fb is NULL");
        return;
    }

    if (fb->fmt == TUYA_PIXEL_FMT_MONOCHROME) {
        for (y = area->y1; y <= area->y2; y++) {
            for (x = area->x1; x <= area->x2; x++) {
                uint16_t *px_map_u16 = (uint16_t *)px_map;
                bool enable = (px_map_u16[offset++] > 0x8FFF) ? false : true;
                __disp_mono_write_point(x, y, enable, fb);
            }
        }
    } else if (fb->fmt == TUYA_PIXEL_FMT_I2) {
        for (y = area->y1; y <= area->y2; y++) {
            for (x = area->x1; x <= area->x2; x++) {
                lv_color16_t *px_map_color16 = (lv_color16_t *)px_map;
                uint8_t grey2 = ~((px_map_color16[offset].ch.red +
                                   px_map_color16[offset].ch.green * 2 +
                                   px_map_color16[offset].ch.blue) >> 2);
                offset++;
                __disp_i2_write_point(x, y, grey2, fb);
            }
        }
    } else {
        if (is_swap) {
            tdl_disp_dev_rgb565_swap((uint16_t *)px_map,
                                     lv_area_get_width(area) * lv_area_get_height(area));
        }
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
        tal_dma2d_wait_finish(sg_lvgl_dma2d_hdl, 1000);
        __dma2d_drawbuffer_memcpy_syn(area, px_map, fb);
#else
        uint8_t *color_ptr = px_map;
        uint8_t per_pixel_byte = (tdl_disp_get_fmt_bpp(fb->fmt) + 7) / 8;
        int32_t width = lv_area_get_width(area);

        offset = (area->y1 * fb->width + area->x1) * per_pixel_byte;
        for (y = area->y1; y <= area->y2 && y < fb->height; y++) {
            memcpy(fb->frame + offset, color_ptr, width * per_pixel_byte);
            offset += fb->width * per_pixel_byte;
            color_ptr += width * per_pixel_byte;
        }
#endif
    }
}

/**********************
 *  INTERFACE IMPL
 **********************/

void lv_port_flush_init(LV_DISP_NODE_T *node)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t disp_fb_num = 0;

    TUYA_CALL_ERR_LOG(tdl_disp_fb_manage_init(&node->fb_mag));

#if defined(ENABLE_LVGL_DUAL_DISP_BUFF) && (ENABLE_LVGL_DUAL_DISP_BUFF == 1)
    disp_fb_num = 3;
#else
    disp_fb_num = 2;
#endif

    for (uint8_t i = 0; i < disp_fb_num; i++) {
        TUYA_CALL_ERR_LOG(tdl_disp_fb_manage_add(node->fb_mag,
                          node->dev_info.fmt, node->dev_info.width, node->dev_info.height));
    }

    node->disp_fb = tdl_disp_get_free_fb(node->fb_mag);

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    /* DMA2D only accelerates RGB565/RGB888 framebuffer memcpy; monochrome (and any other) format
       fills the framebuffer on the CPU and never touches DMA2D. Powering up the DMA2D hardware
       for such panels just burns ~5mA on the VIDP_DMA2D domain for nothing - and toggling that
       domain later interferes with GPIO wake from deep sleep. Only bring it up when used. */
    sg_dma2d_used = (TUYA_PIXEL_FMT_RGB565 == node->dev_info.fmt ||
                     TUYA_PIXEL_FMT_RGB888 == node->dev_info.fmt);
    if (sg_dma2d_used && NULL == sg_lvgl_dma2d_hdl) {
        TUYA_CALL_ERR_LOG(tal_dma2d_init(&sg_lvgl_dma2d_hdl));
    }
#endif
}

void lv_port_flush_execute(LV_DISP_NODE_T *node, lv_disp_drv_t *disp_drv,
                           const lv_area_t *area, uint8_t *color_ptr)
{
    __disp_fill_display_framebuffer(area, color_ptr, node->disp_fb, node->dev_info.is_swap);

    if (lv_disp_flush_is_last(disp_drv)) {
        tdl_disp_dev_flush(node->dev_hdl, node->disp_fb);

        TDL_DISP_FRAME_BUFF_T *next_fb = tdl_disp_get_free_fb(node->fb_mag);
        if (next_fb && next_fb != node->disp_fb) {
            __disp_framebuffer_memcpy(&node->dev_info, next_fb->frame,
                                      node->disp_fb->frame, node->disp_fb->len);
            node->disp_fb = next_fb;
        }
    }

    lv_disp_flush_ready(disp_drv);
}

void lv_port_flush_on_enable(LV_DISP_NODE_T *node)
{
    if (node->disp_fb) {
        tdl_disp_dev_flush(node->dev_hdl, node->disp_fb);

        TDL_DISP_FRAME_BUFF_T *next_fb = tdl_disp_get_free_fb(node->fb_mag);
        if (next_fb && next_fb != node->disp_fb) {
            __disp_framebuffer_memcpy(&node->dev_info, next_fb->frame,
                                      node->disp_fb->frame, node->disp_fb->len);
            node->disp_fb = next_fb;
        }
    }
}

void lv_port_flush_release(LV_DISP_NODE_T *node)
{
    if (node->fb_mag) {
        tdl_disp_fb_manage_release(&node->fb_mag);
        node->fb_mag = NULL;
    }
}

/* Low-power hooks: tear down / rebuild the DMA2D that flush_init leaves running. flush_init
   calls tal_dma2d_init once and normally nothing deinits it, so its VIDP_DMA2D power domain
   stays on (~5mA). Release it when suspending the display, reacquire before resuming flush. */
void lv_port_flush_dma2d_deinit(void)
{
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    if (sg_lvgl_dma2d_hdl) {
        tal_dma2d_deinit(sg_lvgl_dma2d_hdl);
        sg_lvgl_dma2d_hdl = NULL;
    }
#endif
}

void lv_port_flush_dma2d_reinit(void)
{
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    OPERATE_RET rt = OPRT_OK;
    /* Only re-init if this display actually used DMA2D (see flush_init); never for mono panels. */
    if (sg_dma2d_used && NULL == sg_lvgl_dma2d_hdl) {
        TUYA_CALL_ERR_LOG(tal_dma2d_init(&sg_lvgl_dma2d_hdl));
    }
    (void)rt;
#endif
}

#endif /* !ENABLE_LVGL_PARTIAL_FLUSH */
