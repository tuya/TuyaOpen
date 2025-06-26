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
/*********************
 *      DEFINES
 *********************/
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define LV_MEM_CUSTOM_ALLOC   tkl_system_psram_malloc
#define LV_MEM_CUSTOM_FREE    tkl_system_psram_free
#define LV_MEM_CUSTOM_REALLOC tkl_system_psram_realloc
#else
#define LV_MEM_CUSTOM_ALLOC   tkl_system_malloc
#define LV_MEM_CUSTOM_FREE    tkl_system_free
#define LV_MEM_CUSTOM_REALLOC tkl_system_realloc
#endif


/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(char *device);

static void disp_deinit(void);

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

static uint8_t * __disp_draw_buf_align_alloc(uint32_t size_bytes);

static lv_color_format_t __disp_get_lv_color_format(TUYA_DISPLAY_PIXEL_FMT_E pixel_fmt);

static uint8_t __disp_get_pixels_size_bytes(TUYA_DISPLAY_PIXEL_FMT_E pixel_fmt);

/**********************
 *  STATIC VARIABLES
 **********************/
static TDL_DISP_HANDLE_T sg_tdl_disp_hdl = NULL;
static TDL_DISP_DEV_INFO_T sg_display_info;
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
    uint32_t buf_len = sg_display_info.width * sg_display_info.height / LV_DRAW_BUF_PARTS * per_pixel_byte;

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

/*Initialize your display and the required peripherals.*/
static void disp_init(char *device)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t per_pixel_byte = 0;
    uint32_t frame_len = 0;

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

    if(sg_display_info.fmt == TUYA_PIXEL_FMT_MONOCHROME) {
        frame_len = (sg_display_info.width + 7) / 8 * sg_display_info.height;
    } else {
        per_pixel_byte = __disp_get_pixels_size_bytes(sg_display_info.fmt);
        frame_len = sg_display_info.width * sg_display_info.height * per_pixel_byte;
    }

    sg_p_display_fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if(NULL == sg_p_display_fb) {
        PR_ERR("create display frame buff failed");
        return;
    }

    sg_p_display_fb->fmt    = sg_display_info.fmt;
    sg_p_display_fb->width  = sg_display_info.width;
    sg_p_display_fb->height = sg_display_info.height;
}

static uint8_t *__disp_draw_buf_align_alloc(uint32_t size_bytes)
{
    uint8_t *buf_u8= NULL;
    /*Allocate larger memory to be sure it can be aligned as needed*/
    size_bytes += LV_DRAW_BUF_ALIGN - 1;
    buf_u8 = (uint8_t *)LV_MEM_CUSTOM_ALLOC(size_bytes);
    if (buf_u8) {
        memset(buf_u8, 0x00, size_bytes);
        buf_u8 += LV_DRAW_BUF_ALIGN - 1;
        buf_u8 = (uint8_t *)((uint32_t) buf_u8 & ~(LV_DRAW_BUF_ALIGN - 1));
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
            return LV_COLOR_FORMAT_RGB565; // LVGL does not support monochrome directly, use RGB565 as a workaround
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

static void __disp_fill_display_framebuffer(const lv_area_t * area, uint8_t * px_map, \
                                            lv_color_format_t cf, TDL_DISP_FRAME_BUFF_T *fb)
{
    uint32_t offset = 0, x = 0, y = 0;
    uint8_t *disp_buf = NULL;
    int32_t width = 0;

    if(NULL == area || NULL == px_map || NULL == fb) {
        PR_ERR("Invalid parameters: area or px_map or fb is NULL");
        return;
    }

    width = lv_area_get_width(area);

    disp_buf = fb->frame;
    
    if(fb->fmt == TUYA_PIXEL_FMT_MONOCHROME) {
        for(y = area->y1 ; y <= area->y2; y++) {
            for(x = area->x1; x <= area->x2; x++) {
                uint16_t *px_map_u16 = (uint16_t *)px_map;
                bool enable = (px_map_u16[offset++]> 0x8FFF) ? false : true;
                __disp_mono_write_point(x, y, enable, fb);
            }
        }
    }else {
        if(LV_COLOR_FORMAT_RGB565 == cf) {
            #if defined(LVGL_COLOR_16_SWAP) && (LVGL_COLOR_16_SWAP == 1)
            lv_draw_sw_rgb565_swap(px_map, lv_area_get_width(area) * lv_area_get_height(area));
            #endif
        }

        uint8_t *color_ptr = px_map;
        uint8_t per_pixel_byte = __disp_get_pixels_size_bytes(fb->fmt);

        offset = (area->y1 * fb->width + area->x1) * per_pixel_byte;
        for (y = area->y1; y <= area->y2 && y < fb->height; y++) {
            memcpy(disp_buf + offset, color_ptr, width * per_pixel_byte);
            offset += fb->width * per_pixel_byte; // Move to the next line in the display buffer
            color_ptr += width * per_pixel_byte;
        }
    }
}

static void disp_deinit(void)
{

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

        if (lv_disp_flush_is_last(disp)) {
            tdl_disp_dev_flush(sg_tdl_disp_hdl, sg_p_display_fb);
        }
    }

    lv_disp_flush_ready(disp);
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
