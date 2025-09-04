/**
 * @file lv_draw_sw.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "../lv_draw.h"
#include "lv_draw_sw.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  GLOBAL PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_sw_init_ctx(lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx)
{
    LV_UNUSED(drv);

    lv_draw_sw_ctx_t * draw_sw_ctx = (lv_draw_sw_ctx_t *) draw_ctx;
    lv_memset_00(draw_sw_ctx, sizeof(lv_draw_sw_ctx_t));

    draw_sw_ctx->base_draw.draw_arc = lv_draw_sw_arc;
    draw_sw_ctx->base_draw.draw_rect = lv_draw_sw_rect;
    draw_sw_ctx->base_draw.draw_bg = lv_draw_sw_bg;
    draw_sw_ctx->base_draw.draw_letter = lv_draw_sw_letter;
    draw_sw_ctx->base_draw.draw_img_decoded = lv_draw_sw_img_decoded;
    draw_sw_ctx->base_draw.draw_line = lv_draw_sw_line;
    draw_sw_ctx->base_draw.draw_polygon = lv_draw_sw_polygon;
#if LV_DRAW_COMPLEX
    draw_sw_ctx->base_draw.draw_transform = lv_draw_sw_transform;
#endif
    draw_sw_ctx->base_draw.wait_for_finish = lv_draw_sw_wait_for_finish;
    draw_sw_ctx->base_draw.buffer_copy = lv_draw_sw_buffer_copy;
    draw_sw_ctx->base_draw.layer_init = lv_draw_sw_layer_create;
    draw_sw_ctx->base_draw.layer_adjust = lv_draw_sw_layer_adjust;
    draw_sw_ctx->base_draw.layer_blend = lv_draw_sw_layer_blend;
    draw_sw_ctx->base_draw.layer_destroy = lv_draw_sw_layer_destroy;
    draw_sw_ctx->blend = lv_draw_sw_blend_basic;
    draw_ctx->layer_instance_size = sizeof(lv_draw_sw_layer_ctx_t);
}

void lv_draw_sw_deinit_ctx(lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx)
{
    LV_UNUSED(drv);

    lv_draw_sw_ctx_t * draw_sw_ctx = (lv_draw_sw_ctx_t *) draw_ctx;
    lv_memset_00(draw_sw_ctx, sizeof(lv_draw_sw_ctx_t));
}

void lv_draw_sw_wait_for_finish(lv_draw_ctx_t * draw_ctx)
{
    LV_UNUSED(draw_ctx);
    /*Nothing to wait for*/
}

void lv_draw_sw_buffer_copy(lv_draw_ctx_t * draw_ctx,
                            void * dest_buf, lv_coord_t dest_stride, const lv_area_t * dest_area,
                            void * src_buf, lv_coord_t src_stride, const lv_area_t * src_area)
{
    LV_UNUSED(draw_ctx);

    lv_color_t * dest_bufc =  dest_buf;
    lv_color_t * src_bufc =  src_buf;

    /*Got the first pixel of each buffer*/
    dest_bufc += dest_stride * dest_area->y1;
    dest_bufc += dest_area->x1;

    src_bufc += src_stride * src_area->y1;
    src_bufc += src_area->x1;

    uint32_t line_length = lv_area_get_width(dest_area) * sizeof(lv_color_t);
    lv_coord_t y;
    for(y = dest_area->y1; y <= dest_area->y2; y++) {
        lv_memcpy(dest_bufc, src_bufc, line_length);
        dest_bufc += dest_stride;
        src_bufc += src_stride;
    }
}

void lv_draw_sw_rgb565_swap(void * buf, uint32_t buf_size_px)
{
    uint32_t u32_cnt = buf_size_px / 2;
    uint16_t * buf16 = buf;
    uint32_t * buf32 = buf;

    while(u32_cnt >= 8) {
        buf32[0] = ((buf32[0] & 0xff00ff00) >> 8) | ((buf32[0] & 0x00ff00ff) << 8);
        buf32[1] = ((buf32[1] & 0xff00ff00) >> 8) | ((buf32[1] & 0x00ff00ff) << 8);
        buf32[2] = ((buf32[2] & 0xff00ff00) >> 8) | ((buf32[2] & 0x00ff00ff) << 8);
        buf32[3] = ((buf32[3] & 0xff00ff00) >> 8) | ((buf32[3] & 0x00ff00ff) << 8);
        buf32[4] = ((buf32[4] & 0xff00ff00) >> 8) | ((buf32[4] & 0x00ff00ff) << 8);
        buf32[5] = ((buf32[5] & 0xff00ff00) >> 8) | ((buf32[5] & 0x00ff00ff) << 8);
        buf32[6] = ((buf32[6] & 0xff00ff00) >> 8) | ((buf32[6] & 0x00ff00ff) << 8);
        buf32[7] = ((buf32[7] & 0xff00ff00) >> 8) | ((buf32[7] & 0x00ff00ff) << 8);
        buf32 += 8;
        u32_cnt -= 8;
    }

    while(u32_cnt) {
        *buf32 = ((*buf32 & 0xff00ff00) >> 8) | ((*buf32 & 0x00ff00ff) << 8);
        buf32++;
        u32_cnt--;
    }

    if(buf_size_px & 0x1) {
        uint32_t e = buf_size_px - 1;
        buf16[e] = ((buf16[e] & 0xff00) >> 8) | ((buf16[e] & 0x00ff) << 8);
    }

}

/**********************
 *   STATIC FUNCTIONS
 **********************/
