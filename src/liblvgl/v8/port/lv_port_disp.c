/**
 * @file lv_port_disp.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>
#include "lv_port_disp.h"
#include "lv_vendor.h"

#include "tkl_memory.h"
#include "tal_api.h"
#include "tdl_display_manage.h"

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
#include "tal_dma2d.h"
#endif

/*********************
 *      DEFINES
 *********************/
#define DISP_DRAW_BUF_ALIGN    4

#define LV_DISP_FB_MAX_NUM    3
/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    uint8_t                is_used;
    TDL_DISP_FRAME_BUFF_T *fb;
}LV_DISP_FRAME_BUFF_T;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(char *device);

static void disp_deinit(void);

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

static uint8_t * __disp_draw_buf_align_alloc(uint32_t size_bytes);


/**********************
 *  STATIC VARIABLES
 **********************/
static TDL_DISP_HANDLE_T sg_tdl_disp_hdl = NULL;
static TDL_DISP_DEV_INFO_T sg_display_info;
static MUTEX_HANDLE sg_disp_flush_mutex = NULL;

static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb = NULL;
static TDL_FB_MANAGE_HANDLE_T sg_disp_fb_manage = NULL;
/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(char *device)
{
    uint8_t per_pixel_byte = LV_COLOR_DEPTH/8;

    if (NULL == sg_disp_flush_mutex) {
        tal_mutex_create_init(&sg_disp_flush_mutex);
    }

    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init(device);

    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/

    /**
     * LVGL requires a buffer where it internally draws the widgets.
     * Later this buffer will passed to your display driver's `flush_cb` to copy its content to your display.
     * The buffer has to be greater than 1 display row
     *
     * There are 3 buffering configurations:
     * 1. Create ONE buffer:
     *      LVGL will draw the display's content here and writes it to your display
     *
     * 2. Create TWO buffer:
     *      LVGL will draw the display's content to a buffer and writes it your display.
     *      You should use DMA to write the buffer's content to the display.
     *      It will enable LVGL to draw the next part of the screen to the other buffer while
     *      the data is being sent form the first buffer. It makes rendering and flushing parallel.
     *
     * 3. Double buffering
     *      Set 2 screens sized buffers and set disp_drv.full_refresh = 1.
     *      This way LVGL will always provide the whole rendered screen in `flush_cb`
     *      and you only need to change the frame buffer's address.
     */


    uint32_t buf_len = (sg_display_info.height / LV_DRAW_BUF_PARTS) * sg_display_info.width * per_pixel_byte;

    /* Example for 2) */
    static lv_disp_draw_buf_t draw_buf_dsc_2;
    static uint8_t *buf_2_1;
    buf_2_1 = __disp_draw_buf_align_alloc(buf_len);
    if (buf_2_1 == NULL) {
        PR_ERR("malloc failed");
        return;
    }

    static uint8_t *buf_2_2;
    buf_2_2 = __disp_draw_buf_align_alloc(buf_len);
    if (buf_2_2 == NULL) {
        PR_ERR("malloc failed");
        return;
    }
    lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, buf_len/per_pixel_byte);   /*Initialize the display buffer*/

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/

    static lv_disp_drv_t disp_drv;                         /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);                    /*Basic initialization*/

    /*Set up the functions to access to your display*/

    /*Set the resolution of the display*/
    disp_drv.hor_res = sg_display_info.width;
    disp_drv.ver_res = sg_display_info.height;

    /*Used to copy the buffer's content to the display*/
    disp_drv.flush_cb = disp_flush;

    /*Set a display buffer*/
    disp_drv.draw_buf = &draw_buf_dsc_2;

    /*Required for Example 3)*/
    //disp_drv.full_refresh = 1;

    /* Fill a memory array with a color if you have GPU.
     * Note that, in lv_conf.h you can enable GPUs that has built-in support in LVGL.
     * But if you have a different GPU you can use with this callback.*/
    //disp_drv.gpu_fill_cb = gpu_fill;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
}

void lv_port_disp_deinit(void)
{
    disp_deinit();
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
static TAL_DMA2D_HANDLE_T sg_lvgl_dma2d_hdl = NULL;
static void __dma2d_drawbuffer_memcpy_syn(const lv_area_t * area, uint8_t * px_map, \
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

    // Perform memory copy based on color format
#if defined(LV_COLOR_DEPTH) && (LV_COLOR_DEPTH==16)
    in_frame.type  = TUYA_FRAME_FMT_RGB565;
    out_frame.type = TUYA_FRAME_FMT_RGB565;
#elif defined(LV_COLOR_DEPTH) && (LV_COLOR_DEPTH==24)
    in_frame.type  = TUYA_FRAME_FMT_RGB888;
    out_frame.type = TUYA_FRAME_FMT_RGB888;
#else 
    #error "LV_COLOR_DEPTH not support"
#endif

    in_frame.width  = area->x2 - area->x1 + 1;
    in_frame.height = area->y2 - area->y1 + 1;
    in_frame.pbuf   = px_map;
    in_frame.axis.x_axis   = 0;
    in_frame.axis.y_axis   = 0;
    in_frame.width_cp      = 0;
    in_frame.height_cp     = 0;

    out_frame.width  = fb->width;
    out_frame.height = fb->height;
    out_frame.pbuf   = fb->frame;
    out_frame.axis.x_axis   = area->x1;
    out_frame.axis.y_axis   = area->y1;

    tal_dma2d_memcpy(sg_lvgl_dma2d_hdl, &in_frame, &out_frame);

    tal_dma2d_wait_finish(sg_lvgl_dma2d_hdl, 1000);
}

static void __dma2d_framebuffer_memcpy_async(TDL_DISP_DEV_INFO_T *dev_info,\
                                             uint8_t *dst_frame,\
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

    in_frame.width  = dev_info->width;
    in_frame.height = dev_info->height;
    in_frame.pbuf   = src_frame;
    in_frame.axis.x_axis   = 0;
    in_frame.axis.y_axis   = 0;
    in_frame.width_cp      = 0;
    in_frame.height_cp     = 0;
    
    out_frame.width  = dev_info->width;
    out_frame.height = dev_info->height;
    out_frame.pbuf   = dst_frame;
    out_frame.axis.x_axis   = 0;
    out_frame.axis.y_axis   = 0;
    out_frame.width_cp      = 0;
    out_frame.height_cp     = 0;

    tal_dma2d_memcpy(sg_lvgl_dma2d_hdl, &in_frame, &out_frame);
}
#endif

static void disp_frame_buff_init(TUYA_DISPLAY_PIXEL_FMT_E fmt, uint16_t width, uint16_t height, bool has_vram)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t disp_fb_num = 0;

    TUYA_CALL_ERR_LOG(tdl_disp_fb_manage_init(&sg_disp_fb_manage));

#if defined(ENABLE_LVGL_DUAL_DISP_BUFF) && (ENABLE_LVGL_DUAL_DISP_BUFF == 1)
    disp_fb_num = 2 + (has_vram ? 0 : 1);
#else
    disp_fb_num = 1 + (has_vram ? 0 : 1);
#endif

    for(uint8_t i=0; i<disp_fb_num; i++) {
        TUYA_CALL_ERR_LOG(tdl_disp_fb_manage_add(sg_disp_fb_manage, fmt, width, height));
    }

    sg_p_display_fb = tdl_disp_get_free_fb(sg_disp_fb_manage);
}

/*Initialize your display and the required peripherals.*/
static void disp_init(char *device)
{
    OPERATE_RET rt = OPRT_OK;

    memset(&sg_display_info, 0, sizeof(TDL_DISP_DEV_INFO_T));

    sg_tdl_disp_hdl = tdl_disp_find_dev(device);
    if(NULL == sg_tdl_disp_hdl) {
        PR_ERR("display dev %s not found", device);
        return;
    }

    rt = tdl_disp_dev_get_info(sg_tdl_disp_hdl, &sg_display_info);
    if(rt != OPRT_OK) {
        PR_ERR("get display dev info failed, rt: %d", rt);
        return;
    }

    rt = tdl_disp_dev_open(sg_tdl_disp_hdl);
    if(rt != OPRT_OK) {
            PR_ERR("open display dev failed, rt: %d", rt);
            return;
    }

    tdl_disp_set_brightness(sg_tdl_disp_hdl, 100); // Set brightness to 100%

    disp_frame_buff_init(sg_display_info.fmt, sg_display_info.width, \
                         sg_display_info.height, sg_display_info.has_vram);

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    tal_dma2d_init(&sg_lvgl_dma2d_hdl);
#endif
}

static uint8_t *__disp_draw_buf_align_alloc(uint32_t size_bytes)
{
    uint8_t *buf_u8= NULL;
    /*Allocate larger memory to be sure it can be aligned as needed*/
    size_bytes += DISP_DRAW_BUF_ALIGN - 1;
    buf_u8 = (uint8_t *)LV_MEM_CUSTOM_ALLOC(size_bytes);
    if (buf_u8) {
        buf_u8 += DISP_DRAW_BUF_ALIGN - 1;
        buf_u8 = (uint8_t *)((uint32_t) buf_u8 & ~(DISP_DRAW_BUF_ALIGN - 1));
    }

    return buf_u8;
}

static void __disp_mono_write_point(uint32_t x, uint32_t y, bool enable, TDL_DISP_FRAME_BUFF_T *fb)
{
    if(NULL == fb || x >= fb->width || y >= fb->height) {
        PR_ERR("Point (%d, %d) out of bounds", x, y);
        return;
    }

    uint32_t write_byte_index = y * (fb->width/8) + x/8;
    uint8_t write_bit = x%8;

    if (enable) {
        fb->frame[write_byte_index] |= (1 << write_bit);
    } else {
        fb->frame[write_byte_index] &= ~(1 << write_bit);
    }
}

static void __disp_i2_write_point(uint32_t x, uint32_t y, uint8_t color, TDL_DISP_FRAME_BUFF_T *fb)
{
    if(NULL == fb || x >= fb->width || y >= fb->height) {
        PR_ERR("Point (%d, %d) out of bounds", x, y);
        return;
    }

    uint32_t write_byte_index = y * (fb->width/4) + x/4;
    uint8_t write_bit = (x%4)*2;
    uint8_t cleared = fb->frame[write_byte_index] & (~(0x03 << write_bit)); // Clear the bits we are going to write

    fb->frame[write_byte_index] = cleared | ((color & 0x03) << write_bit);
}

static void __disp_fill_display_framebuffer(const lv_area_t * area, uint8_t * px_map,\
                                            TDL_DISP_FRAME_BUFF_T *fb)
{
    uint32_t offset = 0, x = 0, y = 0;

    if(NULL == area || NULL == px_map || NULL == fb) {
        PR_ERR("Invalid parameters: area or px_map or fb is NULL");
        return;
    }
    
    if(fb->fmt == TUYA_PIXEL_FMT_MONOCHROME) {
        for(y = area->y1 ; y <= area->y2; y++) {
            for(x = area->x1; x <= area->x2; x++) {
                uint16_t *px_map_u16 = (uint16_t *)px_map;
                bool enable = (px_map_u16[offset++]> 0x8FFF) ? false : true;
                __disp_mono_write_point(x, y, enable, fb);
            }
        }
    }else if(fb->fmt == TUYA_PIXEL_FMT_I2) { 
        for(y = area->y1 ; y <= area->y2; y++) {
            for(x = area->x1; x <= area->x2; x++) {
                lv_color16_t *px_map_color16 = (lv_color16_t *)px_map;
            #if LV_COLOR_16_SWAP == 0
                uint8_t grey2 = ~((px_map_color16[offset].ch.red + px_map_color16[offset].ch.green*2 +\
                                 px_map_color16[offset].ch.blue) >> 2);
            #else 
                uint8_t green = (px_map_color16[offset].ch.green_l) + ((px_map_color16[offset].ch.green_h)<<3);
                uint8_t grey2 = ~((px_map_color16[offset].ch.red + green*2 +\
                                 px_map_color16[offset].ch.blue) >> 2);
            #endif
                offset++;
                __disp_i2_write_point(x, y, grey2, fb);
            }
        }
    }else {
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
            offset += fb->width * per_pixel_byte; // Move to the next line in the display buffer
            color_ptr += width * per_pixel_byte;
        }
#endif
    }
}

static void __disp_framebuffer_memcpy(TDL_DISP_DEV_INFO_T *dev_info,\
                                      uint8_t *dst_frame,uint8_t *src_frame,\
                                      uint32_t frame_size)
{

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    __dma2d_framebuffer_memcpy_async(dev_info, dst_frame, src_frame);
#else
    memcpy(dst_frame, src_frame, frame_size);
#endif
}

static void disp_deinit(void)
{
    tdl_disp_dev_close(sg_tdl_disp_hdl);
    sg_tdl_disp_hdl = NULL;

    tdl_disp_fb_manage_release(&sg_disp_fb_manage);
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    tal_mutex_lock(sg_disp_flush_mutex);

    if(sg_p_display_fb) {
        tdl_disp_dev_flush(sg_tdl_disp_hdl, sg_p_display_fb);

        TDL_DISP_FRAME_BUFF_T *next_fb = tdl_disp_get_free_fb(sg_disp_fb_manage);
        if(next_fb &&  next_fb != sg_p_display_fb) {
            __disp_framebuffer_memcpy(&sg_display_info, next_fb->frame,\
                                       sg_p_display_fb->frame, sg_p_display_fb->len);
            sg_p_display_fb = next_fb;
        }
    }

    disp_flush_enabled = true;

    tal_mutex_unlock(sg_disp_flush_mutex);
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    tal_mutex_lock(sg_disp_flush_mutex);

    disp_flush_enabled = false;

    tal_mutex_unlock(sg_disp_flush_mutex);
}

/**
 * @brief Sets the display backlight brightness
 * 
 * @param brightness Brightness level (0-100)
 */
void disp_set_backlight(uint8_t brightness)
{
    tdl_disp_set_brightness(sg_tdl_disp_hdl, brightness);
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    uint8_t *color_ptr = (uint8_t *)color_p;
    lv_area_t *target_area = (lv_area_t *)area;

    tal_mutex_lock(sg_disp_flush_mutex);

    if(disp_flush_enabled) {

        __disp_fill_display_framebuffer(target_area, color_ptr, sg_p_display_fb);

        if (lv_disp_flush_is_last(disp_drv)) {
            tdl_disp_dev_flush(sg_tdl_disp_hdl, sg_p_display_fb);

            TDL_DISP_FRAME_BUFF_T *next_fb = tdl_disp_get_free_fb(sg_disp_fb_manage);
            if(next_fb &&  next_fb != sg_p_display_fb) {
                __disp_framebuffer_memcpy(&sg_display_info, next_fb->frame, sg_p_display_fb->frame, sg_p_display_fb->len);
                sg_p_display_fb = next_fb;
            }
        }
    }

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);

    tal_mutex_unlock(sg_disp_flush_mutex);
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
