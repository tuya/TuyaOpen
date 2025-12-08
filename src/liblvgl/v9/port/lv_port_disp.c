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
#include "tkl_dma2d.h"
#endif

/*********************
 *      DEFINES
 *********************/
#define DISP_DRAW_BUF_ALIGN    4

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define LV_MEM_CUSTOM_ALLOC   tkl_system_psram_malloc
#define LV_MEM_CUSTOM_FREE    tkl_system_psram_free
#define LV_MEM_CUSTOM_REALLOC tkl_system_psram_realloc
#else
#define LV_MEM_CUSTOM_ALLOC   tkl_system_malloc
#define LV_MEM_CUSTOM_FREE    tkl_system_free
#define LV_MEM_CUSTOM_REALLOC tkl_system_realloc
#endif

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

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

static uint8_t * __disp_draw_buf_align_alloc(uint32_t size_bytes);

static lv_color_format_t __disp_get_lv_color_format(TUYA_DISPLAY_PIXEL_FMT_E pixel_fmt);

static uint8_t __disp_get_pixels_size_bytes(TUYA_DISPLAY_PIXEL_FMT_E pixel_fmt);

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
static void __disp_dma2d_init(void);
#endif


/**********************
 *  STATIC VARIABLES
 **********************/
static TDL_DISP_HANDLE_T sg_tdl_disp_hdl = NULL;
static TDL_DISP_DEV_INFO_T sg_display_info;

static LV_DISP_FRAME_BUFF_T sg_disp_fb_arr[LV_DISP_FB_MAX_NUM];
static uint8_t sg_disp_fb_num = 0;
static bool sg_is_wait_disp_free_fb = false;
static SEM_HANDLE sg_disp_fb_free_sem = NULL;
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb = NULL; 

static uint8_t *sg_rotate_buf = NULL;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_port_disp_init(char *device)
{
    uint8_t per_pixel_byte = 0;

    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init(device);

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    lv_display_t * disp = lv_display_create(sg_display_info.width, sg_display_info.height);
    lv_display_set_flush_cb(disp, disp_flush);

    lv_color_format_t color_format = __disp_get_lv_color_format(sg_display_info.fmt);
    PR_NOTICE("lv_color_format:%d", color_format);
    lv_display_set_color_format(disp, color_format);

    /* Example 2
     * Two buffers for partial rendering
     * In flush_cb DMA or similar hardware should be used to update the display in the background.*/
    per_pixel_byte = lv_color_format_get_size(color_format);

    uint32_t buf_len = (sg_display_info.height / LV_DRAW_BUF_PARTS) * sg_display_info.width * per_pixel_byte;

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

    lv_display_set_buffers(disp, buf_2_1, buf_2_2, buf_len, LV_DISPLAY_RENDER_MODE_PARTIAL);

    if (sg_display_info.rotation != TUYA_DISPLAY_ROTATION_0) {
        if (sg_display_info.rotation == TUYA_DISPLAY_ROTATION_90) {
            lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
        }else if (sg_display_info.rotation == TUYA_DISPLAY_ROTATION_180){
            lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_180);
        }else if(sg_display_info.rotation == TUYA_DISPLAY_ROTATION_270){
            lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
        }

        PR_NOTICE("rotation:%d", sg_display_info.rotation);

        sg_rotate_buf = __disp_draw_buf_align_alloc(buf_len);
        if (sg_rotate_buf == NULL) {
            PR_ERR("lvgl rotate buffer malloc fail!\n");
        }
    }
}

void lv_port_disp_deinit(void)
{
    lv_display_delete(lv_disp_get_default());
    disp_deinit();
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
static SEM_HANDLE sg_dma2d_finish_sem = NULL;
static bool sg_is_wait_dma2d = false;
static void __disp_dma2d_event_cb(TUYA_DMA2D_IRQ_E type, VOID_T *args)
{
    tal_semaphore_post(sg_dma2d_finish_sem);
}

static void __disp_dma2d_init(void)
{
    tal_semaphore_create_init(&sg_dma2d_finish_sem, 0, 1);

    TUYA_DMA2D_BASE_CFG_T cfg = {
        .cb = __disp_dma2d_event_cb,
    };

    tkl_dma2d_init(&cfg);
}

static void __wait_dma2d_trans_finish(void)
{
    OPERATE_RET ret = OPRT_OK;

    if(sg_dma2d_finish_sem && sg_is_wait_dma2d) {
        ret = tal_semaphore_wait(sg_dma2d_finish_sem, 1000);
        if(ret != OPRT_OK) {
            PR_ERR("wait dma2d finish failed, rt: %d", ret);
        }
        sg_is_wait_dma2d = false;
    }
}

static void __dma2d_drawbuffer_memcpy_syn(const lv_area_t * area, uint8_t * px_map, \
                                          lv_color_format_t cf, TDL_DISP_FRAME_BUFF_T *fb)
{
    TKL_DMA2D_FRAME_INFO_T in_frame = {0};
    TKL_DMA2D_FRAME_INFO_T out_frame = {0};

    if (area == NULL || px_map == NULL || fb == NULL) {
        PR_ERR("Invalid parameter");
        return;
    }

    // Perform memory copy based on color format
    switch (cf) {
        case LV_COLOR_FORMAT_RGB565:
            in_frame.type  = TUYA_FRAME_FMT_RGB565;
            out_frame.type = TUYA_FRAME_FMT_RGB565;
            break;
        case LV_COLOR_FORMAT_RGB888:
            in_frame.type  = TUYA_FRAME_FMT_RGB888;
            out_frame.type = TUYA_FRAME_FMT_RGB888;
            break;
        default:
            PR_ERR("Unsupported color format");
            return;
    }

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


    tkl_dma2d_memcpy(&in_frame, &out_frame);

    sg_is_wait_dma2d = true;

    __wait_dma2d_trans_finish();
}

static void __dma2d_framebuffer_memcpy_async(TDL_DISP_DEV_INFO_T *dev_info,\
                                             uint8_t *dst_frame,\
                                             uint8_t *src_frame)
{
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


    in_frame.type  = TUYA_FRAME_FMT_RGB565;
    in_frame.width  = dev_info->width;
    in_frame.height = dev_info->height;
    in_frame.pbuf   = src_frame;
    in_frame.axis.x_axis   = 0;
    in_frame.axis.y_axis   = 0;
    in_frame.width_cp      = 0;
    in_frame.height_cp     = 0;
    
    out_frame.type = TUYA_FRAME_FMT_RGB565;
    out_frame.width  = dev_info->width;
    out_frame.height = dev_info->height;
    out_frame.pbuf   = dst_frame;
    out_frame.axis.x_axis   = 0;
    out_frame.axis.y_axis   = 0;
    out_frame.width_cp      = 0;
    out_frame.height_cp     = 0;

    tkl_dma2d_memcpy(&in_frame, &out_frame);

    sg_is_wait_dma2d = true;
}
#endif
static void disp_frame_buff_free(TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    if(NULL == frame_buff) {
        return;
    }

    for (uint8_t i = 0; i < sg_disp_fb_num; i++) {
        if(sg_disp_fb_arr[i].fb == frame_buff) {
            sg_disp_fb_arr[i].is_used = 0;
            if(sg_is_wait_disp_free_fb) {
                sg_is_wait_disp_free_fb = false;
                tal_semaphore_post(sg_disp_fb_free_sem);
            }
            return;
        }
    }

    PR_ERR("frame buffer not found");
}

static TDL_DISP_FRAME_BUFF_T *disp_get_free_frame_buff(void)
{
    for (uint8_t i = 0; i < sg_disp_fb_num; i++) {
        if(0 == sg_disp_fb_arr[i].is_used) {
            return sg_disp_fb_arr[i].fb;
        }
    }

    sg_is_wait_disp_free_fb = true;
    tal_semaphore_wait(sg_disp_fb_free_sem, SEM_WAIT_FOREVER);

    for (uint8_t i = 0; i < sg_disp_fb_num; i++) {
        if(0 == sg_disp_fb_arr[i].is_used) {
            return sg_disp_fb_arr[i].fb;
        }
    }

    PR_ERR("no free frame buffer available");

    return NULL;
}
static void disp_set_frame_buff_used(TDL_DISP_FRAME_BUFF_T *fb)
{
    if(NULL == fb) {
        return;
    }

    for (uint8_t i = 0; i < sg_disp_fb_num; i++) {
        if(sg_disp_fb_arr[i].fb == fb) {
            sg_disp_fb_arr[i].is_used = 1;
            return;
        }
    }

    PR_ERR("frame buffer not found");
}

static void disp_frame_buff_init(TUYA_DISPLAY_PIXEL_FMT_E fmt, uint16_t width, uint16_t height, bool has_vram)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t per_pixel_byte = 0;
    uint32_t frame_len = 0;

    if(fmt == TUYA_PIXEL_FMT_MONOCHROME) {
        frame_len = (width + 7) / 8 * height;
    } else if(fmt == TUYA_PIXEL_FMT_I2){
        frame_len = (width + 3) / 4 * height;
    }else {
        per_pixel_byte = __disp_get_pixels_size_bytes(fmt);
        frame_len = width * height * per_pixel_byte;
    }

    rt = tal_semaphore_create_init(&sg_disp_fb_free_sem, 0 ,1);
    if(rt != OPRT_OK) {
        PR_ERR("create semaphore failed, rt: %d", rt);
        return;
    }

#if defined(ENABLE_LVGL_DUAL_DISP_BUFF) && (ENABLE_LVGL_DUAL_DISP_BUFF == 1)
    sg_disp_fb_num = 2 + (has_vram ? 0 : 1);
#else
    sg_disp_fb_num = 1 + (has_vram ? 0 : 1);
#endif

    for (uint8_t i = 0; i < sg_disp_fb_num; i++) {
        sg_disp_fb_arr[i].is_used = 0;

        sg_disp_fb_arr[i].fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
        if(sg_disp_fb_arr[i].fb == NULL) {
            PR_ERR("create display frame buff failed");
            return;
        }

        sg_disp_fb_arr[i].fb->fmt    = fmt;
        sg_disp_fb_arr[i].fb->width  = width;
        sg_disp_fb_arr[i].fb->height = height;

        sg_disp_fb_arr[i].fb->free_cb = disp_frame_buff_free;
    }

    sg_p_display_fb = disp_get_free_frame_buff();
}

static void disp_frame_buff_deinit(void)
{
    if(sg_disp_fb_free_sem) {
        tal_semaphore_release(sg_disp_fb_free_sem);
        sg_disp_fb_free_sem = NULL;
    }

    for (uint8_t i = 0; i < sg_disp_fb_num; i++) {
        if(sg_disp_fb_arr[i].fb) {
            tdl_disp_free_frame_buff(sg_disp_fb_arr[i].fb);
        }
    }
    
    memset(sg_disp_fb_arr, 0, sizeof(sg_disp_fb_arr));

    sg_disp_fb_num = 0;
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
    __disp_dma2d_init();
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

static lv_color_format_t __disp_get_lv_color_format(TUYA_DISPLAY_PIXEL_FMT_E pixel_fmt)
{
    PR_NOTICE("pixel_fmt:%d", pixel_fmt);

    switch (pixel_fmt) {
        case TUYA_PIXEL_FMT_RGB565:
            return LV_COLOR_FORMAT_RGB565;
        case TUYA_PIXEL_FMT_RGB666:
            return LV_COLOR_FORMAT_RGB888; // LVGL does not have RGB666, use RGB888 as a workaround
        case TUYA_PIXEL_FMT_RGB888:
            return LV_COLOR_FORMAT_RGB888;
        case TUYA_PIXEL_FMT_MONOCHROME:
        case TUYA_PIXEL_FMT_I2:
            return LV_COLOR_FORMAT_RGB565; // LVGL does not support monochrome/I2 directly, use RGB565 as a workaround
        default:
            return LV_COLOR_FORMAT_RGB565;
    }
}

static uint8_t __disp_get_pixels_size_bytes(TUYA_DISPLAY_PIXEL_FMT_E pixel_fmt)
{
    switch (pixel_fmt) {
        case TUYA_PIXEL_FMT_RGB565:
            return 2;
        case TUYA_PIXEL_FMT_RGB666:
            return 3;
        case TUYA_PIXEL_FMT_RGB888:
            return 3;
        default:
            return 0;
    }
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

static void __disp_fill_display_framebuffer(const lv_area_t * area, uint8_t * px_map, \
                                            lv_color_format_t cf, TDL_DISP_FRAME_BUFF_T *fb)
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
                uint8_t grey2 = ~((px_map_color16[offset].red + px_map_color16[offset].green*2 +\
                                 px_map_color16[offset].blue) >> 2);
                offset++;
                __disp_i2_write_point(x, y, grey2, fb);
            }
        }
    }else {
        if(LV_COLOR_FORMAT_RGB565 == cf) {
            if(sg_display_info.is_swap) {
                lv_draw_sw_rgb565_swap(px_map, lv_area_get_width(area) * lv_area_get_height(area));
            }
        }

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
        __wait_dma2d_trans_finish();

        __dma2d_drawbuffer_memcpy_syn(area, px_map, cf, fb);
#else
        uint8_t *color_ptr = px_map;
        uint8_t per_pixel_byte = __disp_get_pixels_size_bytes(fb->fmt);
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

    disp_frame_buff_deinit();
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
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
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    uint8_t *color_ptr = px_map;
    lv_area_t *target_area = (lv_area_t *)area;

    if (disp_flush_enabled) {

        lv_color_format_t cf = lv_display_get_color_format(disp);

        if(sg_rotate_buf) {
            lv_display_rotation_t rotation = lv_display_get_rotation(disp);
            lv_area_t rotated_area;

            rotated_area.x1 = area->x1;
            rotated_area.x2 = area->x2;
            rotated_area.y1 = area->y1;
            rotated_area.y2 = area->y2;

            /*Calculate the position of the rotated area*/
            lv_display_rotate_area(disp, &rotated_area);

            /*Calculate the source stride (bytes in a line) from the width of the area*/
            uint32_t src_stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), cf);
            /*Calculate the stride of the destination (rotated) area too*/
            uint32_t dest_stride = lv_draw_buf_width_to_stride(lv_area_get_width(&rotated_area), cf);
            /*Have a buffer to store the rotated area and perform the rotation*/
            
            int32_t src_w = lv_area_get_width(area);
            int32_t src_h = lv_area_get_height(area);

            lv_draw_sw_rotate(px_map, sg_rotate_buf, src_w, src_h, src_stride, dest_stride, rotation, cf);
            /*Use the rotated area and rotated buffer from now on*/

            color_ptr = sg_rotate_buf;
            target_area = &rotated_area;
        }

        __disp_fill_display_framebuffer(target_area, color_ptr, cf, sg_p_display_fb);

        if (lv_display_flush_is_last(disp)) {

            disp_set_frame_buff_used(sg_p_display_fb);
            tdl_disp_dev_flush(sg_tdl_disp_hdl, sg_p_display_fb);

            TDL_DISP_FRAME_BUFF_T *next_fb = disp_get_free_frame_buff();
            if(next_fb &&  next_fb != sg_p_display_fb) {
                __disp_framebuffer_memcpy(&sg_display_info, next_fb->frame, sg_p_display_fb->frame, sg_p_display_fb->len);
                sg_p_display_fb = next_fb;
            }
        }
    }

    lv_display_flush_ready(disp);
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
