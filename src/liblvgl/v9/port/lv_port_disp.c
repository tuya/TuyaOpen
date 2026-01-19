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
#include "tuya_list.h"
#include "tdl_display_manage.h"

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
#include "tal_dma2d.h"
#endif

/*********************
 *      DEFINES
 *********************/
#define DISP_DRAW_BUF_ALIGN    4

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define LV_MEM_CUSTOM_ALLOC   tal_psram_malloc
#define LV_MEM_CUSTOM_FREE    tal_psram_free
#define LV_MEM_CUSTOM_REALLOC tal_psram_realloc
#else
#define LV_MEM_CUSTOM_ALLOC   tal_malloc
#define LV_MEM_CUSTOM_FREE    tal_free
#define LV_MEM_CUSTOM_REALLOC tal_realloc
#endif

#define LV_DISP_FB_MAX_NUM    3

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
    TDL_DISP_FRAME_BUFF_T  *disp_fb;
    TDL_FB_MANAGE_HANDLE_T  fb_mag;
}LV_DISP_NODE_T;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static LV_DISP_NODE_T *__find_lv_disp_node_by_hdl(TDL_DISP_HANDLE_T hdl);

static LV_DISP_NODE_T *__find_lv_disp_node_by_lv_disp(lv_display_t * lv_disp);

static LV_DISP_NODE_T *__create_lv_disp_dev(TDL_DISP_HANDLE_T dev_hdl);

static void __release_lv_disp_dev(LV_DISP_NODE_T *node);

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

static void __disp_dev_enable_update(LV_DISP_NODE_T *node);

static void __disp_dev_disable_update(LV_DISP_NODE_T *node);

static void __disp_dev_set_backlight(LV_DISP_NODE_T *node, uint8_t brightness);

static void __disp_framebuffer_memcpy(TDL_DISP_DEV_INFO_T *dev_info,\
                                      uint8_t *dst_frame,uint8_t *src_frame,\
                                      uint32_t frame_size);
/**********************
 *  STATIC VARIABLES
 **********************/
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
static TAL_DMA2D_HANDLE_T sg_lvgl_dma2d_hdl = NULL;
#endif

static struct tuya_list_head sg_lv_disp_list = LIST_HEAD_INIT(sg_lv_disp_list);

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_port_disp_init(char *device)
{
    LV_DISP_NODE_T *lv_disp_dev = NULL;
    TDL_DISP_HANDLE_T dev_hdl = NULL;

    dev_hdl = tdl_disp_find_dev(device);
    if(NULL == dev_hdl) {
        PR_ERR("display dev %s not found", device);
        return;
    }

    lv_disp_dev = __create_lv_disp_dev(dev_hdl);
    if(NULL == lv_disp_dev) {
        PR_ERR("create lv display dev failed");
        return;
    }

    tuya_list_add(&lv_disp_dev->node, &sg_lv_disp_list);

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    OPERATE_RET rt = OPRT_OK;
    if(NULL == sg_lvgl_dma2d_hdl) {
        TUYA_CALL_ERR_LOG(tal_dma2d_init(&sg_lvgl_dma2d_hdl));
    }
#endif
}

void lv_port_disp_deinit(char *device)
{
    LV_DISP_NODE_T *lv_disp_dev = NULL;
    TDL_DISP_HANDLE_T dev_hdl = NULL;

    dev_hdl = tdl_disp_find_dev(device);
    if(NULL == dev_hdl) {
        PR_ERR("display dev %s not found", device);
        return;
    }

    lv_disp_dev = __find_lv_disp_node_by_hdl(dev_hdl);
    if(NULL == lv_disp_dev) {
        PR_ERR("lv display dev not found");
        return;
    }

    tuya_list_del(&lv_disp_dev->node);

    __release_lv_disp_dev(lv_disp_dev);

    lv_disp_dev = NULL;
}

void disp_enable_update(lv_display_t *lv_disp)
{
    LV_DISP_NODE_T *node = __find_lv_disp_node_by_lv_disp(lv_disp);
    if(NULL == node) {
        PR_ERR("lv display dev not found");
        return;
    }

    __disp_dev_enable_update(node);
}

void disp_disable_update(lv_display_t *lv_disp)
{
    LV_DISP_NODE_T *node = __find_lv_disp_node_by_lv_disp(lv_disp);
    if(NULL == node) {
        PR_ERR("lv display dev not found");
        return;
    }

    __disp_dev_disable_update(node);
}

void disp_set_backlight(lv_display_t *lv_disp, uint8_t brightness)
{
    LV_DISP_NODE_T *node = __find_lv_disp_node_by_lv_disp(lv_disp);
    if(NULL == node) {
        PR_ERR("lv display dev not found");
        return;
    }

    __disp_dev_set_backlight(node, brightness);
}

lv_display_t *lv_port_get_lv_disp_by_name(char *device)
{
    TDL_DISP_HANDLE_T dev_hdl = NULL;
    LV_DISP_NODE_T *node = NULL;

    dev_hdl = tdl_disp_find_dev(device);
    if(NULL == dev_hdl) {
        PR_ERR("display dev %s not found", device);
        return NULL;
    }

    node = __find_lv_disp_node_by_hdl(dev_hdl);
    if(NULL == node) {
        PR_ERR("lv display dev not found");
        return NULL;
    }

    return node->lv_disp;
}


/**********************
 *   STATIC FUNCTIONS
 **********************/
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
static void __dma2d_drawbuffer_memcpy_syn(const lv_area_t * area, uint8_t * px_map, \
                                          lv_color_format_t cf, TDL_DISP_FRAME_BUFF_T *fb)
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

static void disp_frame_buff_init(LV_DISP_NODE_T *node)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t disp_fb_num = 0;

    if(NULL == node){
        return;
    }

    TUYA_CALL_ERR_LOG(tdl_disp_fb_manage_init(&node->fb_mag));

#if defined(ENABLE_LVGL_DUAL_DISP_BUFF) && (ENABLE_LVGL_DUAL_DISP_BUFF == 1)
    disp_fb_num = 2 + ( node->dev_info.has_vram ? 0 : 1);
#else
    disp_fb_num = 1 + (node->dev_info.has_vram ? 0 : 1);
#endif

    for(uint8_t i=0; i<disp_fb_num; i++) {
        TUYA_CALL_ERR_LOG(tdl_disp_fb_manage_add(node->fb_mag, node->dev_info.fmt, node->dev_info.width, node->dev_info.height));
    }

    node->disp_fb = tdl_disp_get_free_fb(node->fb_mag);
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

static LV_DISP_NODE_T *__find_lv_disp_node_by_hdl(TDL_DISP_HANDLE_T hdl)
{
    LV_DISP_NODE_T *lv_disp_node = NULL;
    struct tuya_list_head *pos = NULL;

    if(NULL == hdl) {
        return NULL;
    }

    tuya_list_for_each(pos, &sg_lv_disp_list) {
        lv_disp_node = tuya_list_entry(pos, LV_DISP_NODE_T, node);
        if (lv_disp_node->dev_hdl == hdl) {
            return lv_disp_node;
        }
    }

    return NULL;
}

static LV_DISP_NODE_T *__find_lv_disp_node_by_lv_disp(lv_display_t * lv_disp)
{
    LV_DISP_NODE_T *lv_disp_node = NULL;
    struct tuya_list_head *pos = NULL;

    if(NULL == lv_disp) {
        lv_disp = lv_display_get_default();
    }

    tuya_list_for_each(pos, &sg_lv_disp_list) {
        lv_disp_node = tuya_list_entry(pos, LV_DISP_NODE_T, node);
        if (lv_disp_node->lv_disp == lv_disp) {
            return lv_disp_node;
        }
    }

    return NULL;
}

static LV_DISP_NODE_T *__create_lv_disp_dev(TDL_DISP_HANDLE_T dev_hdl)
{
    uint8_t per_pixel_byte = 0;
    LV_DISP_NODE_T *lv_disp_node = NULL;
    OPERATE_RET rt = OPRT_OK;

    if(NULL == dev_hdl) {
        return NULL;
    }

    NEW_LIST_NODE(LV_DISP_NODE_T, lv_disp_node);
    if (NULL == lv_disp_node) {
        return NULL;
    }
    memset(lv_disp_node, 0, sizeof(LV_DISP_NODE_T));

    tal_mutex_create_init(&lv_disp_node->mutex);

    /*---------------------------------
     * Initialize your display device *
     * -----------------------------*/
    TUYA_CALL_ERR_GOTO(tdl_disp_dev_open(dev_hdl), __CREATE_ERR);

    lv_disp_node->dev_hdl = dev_hdl;
    TUYA_CALL_ERR_GOTO(tdl_disp_dev_get_info(lv_disp_node->dev_hdl, &lv_disp_node->dev_info), __CREATE_ERR);

    tdl_disp_set_brightness(lv_disp_node->dev_hdl, 100); // Set brightness to 100%

    disp_frame_buff_init(lv_disp_node);

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    lv_display_t * disp = lv_display_create(lv_disp_node->dev_info.width, lv_disp_node->dev_info.height);
    lv_display_set_flush_cb(disp, disp_flush);
    lv_disp_node->lv_disp = disp;

    lv_color_format_t color_format = __disp_get_lv_color_format(lv_disp_node->dev_info.fmt);
    PR_NOTICE("lv_color_format:%d", color_format);
    lv_display_set_color_format(disp, color_format);

    /* Example 2
     * Two buffers for partial rendering
     * In flush_cb DMA or similar hardware should be used to update the display in the background.*/
    per_pixel_byte = lv_color_format_get_size(color_format);

    uint32_t buf_len = (lv_disp_node->dev_info.height / LV_DRAW_BUF_PARTS) * lv_disp_node->dev_info.width * per_pixel_byte;

    lv_disp_node->buf_2_1 = __disp_draw_buf_align_alloc(buf_len);
    TUYA_CHECK_NULL_GOTO(lv_disp_node->buf_2_1, __CREATE_ERR);

    lv_disp_node->buf_2_2 = __disp_draw_buf_align_alloc(buf_len);
    TUYA_CHECK_NULL_GOTO(lv_disp_node->buf_2_2, __CREATE_ERR);

    lv_display_set_buffers(disp, lv_disp_node->buf_2_1, lv_disp_node->buf_2_2, buf_len, LV_DISPLAY_RENDER_MODE_PARTIAL);

    if (lv_disp_node->dev_info.rotation == TUYA_DISPLAY_ROTATION_90) {
        lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
    }else if (lv_disp_node->dev_info.rotation == TUYA_DISPLAY_ROTATION_180){
        lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_180);
    }else if(lv_disp_node->dev_info.rotation == TUYA_DISPLAY_ROTATION_270){
        lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
    }

    PR_NOTICE("rotation:%d", lv_disp_node->dev_info.rotation);

    lv_disp_node->rotate_buf = __disp_draw_buf_align_alloc(buf_len);
    TUYA_CHECK_NULL_GOTO(lv_disp_node->rotate_buf, __CREATE_ERR);

    lv_disp_node->is_enable_flush = true;

    return lv_disp_node;

__CREATE_ERR:
    __release_lv_disp_dev(lv_disp_node);

    return NULL;
}

static void __release_lv_disp_dev(LV_DISP_NODE_T *node)
{
    if(NULL == node){
        return;
    }

    if(node->lv_disp) {
        lv_display_delete(node->lv_disp);
        node->lv_disp = NULL;
    }

    if(node->buf_2_1) {
        LV_MEM_CUSTOM_FREE(node->buf_2_1);
        node->buf_2_1 = NULL;
    }

    if(node->buf_2_2) {
        LV_MEM_CUSTOM_FREE(node->buf_2_2);
        node->buf_2_2 = NULL;
    }   

    if(node->rotate_buf) { 
        LV_MEM_CUSTOM_FREE(node->rotate_buf);
        node->rotate_buf = NULL;
    }

    if(node->fb_mag) {
        tdl_disp_fb_manage_release(&node->fb_mag);
        node->fb_mag = NULL;
    }

    if(node->dev_hdl) {
        tdl_disp_dev_close(node->dev_hdl);
        node->dev_hdl = NULL;
    }

    if(node->mutex) {
        tal_mutex_release(node->mutex);
        node->mutex = NULL;
    }

    if(node) {
        tal_free(node);
        node = NULL;
    }
}

static void __disp_dev_enable_update(LV_DISP_NODE_T *node)
{
    tal_mutex_lock(node->mutex);

    if(node->disp_fb) {
        tdl_disp_dev_flush(node->dev_hdl, node->disp_fb);

        TDL_DISP_FRAME_BUFF_T *next_fb = tdl_disp_get_free_fb(node->fb_mag);
        if(next_fb &&  next_fb != node->disp_fb) {
            __disp_framebuffer_memcpy(&node->dev_info, next_fb->frame,\
                                       node->disp_fb->frame, node->disp_fb->len);
            node->disp_fb = next_fb;
        }
    }

    node->is_enable_flush = true;

    tal_mutex_unlock(node->mutex);
}

static void __disp_dev_disable_update(LV_DISP_NODE_T *node)
{
    tal_mutex_lock(node->mutex);

    node->is_enable_flush = false;

    tal_mutex_unlock(node->mutex);
}

static void __disp_dev_set_backlight(LV_DISP_NODE_T *node, uint8_t brightness)
{
    tdl_disp_set_brightness(node->dev_hdl, brightness);
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
                                            lv_color_format_t cf, TDL_DISP_FRAME_BUFF_T *fb, bool is_swap)
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
            if(is_swap) {
                lv_draw_sw_rgb565_swap(px_map, lv_area_get_width(area) * lv_area_get_height(area));
            }
        }

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
        tal_dma2d_wait_finish(sg_lvgl_dma2d_hdl, 1000);

        __dma2d_drawbuffer_memcpy_syn(area, px_map, cf, fb);
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

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    uint8_t *color_ptr = px_map;
    lv_area_t *target_area = (lv_area_t *)area;
    LV_DISP_NODE_T *node = __find_lv_disp_node_by_lv_disp(disp);

    if(NULL == node) {
        PR_ERR("lv display node not found");
        lv_display_flush_ready(disp);
        return;
    }

    tal_mutex_lock(node->mutex);

    if (node->is_enable_flush) {
        lv_color_format_t cf = lv_display_get_color_format(disp);
        lv_display_rotation_t rotation = lv_display_get_rotation(disp);

        if(rotation != LV_DISPLAY_ROTATION_0 && node->rotate_buf != NULL) {
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

            lv_draw_sw_rotate(px_map, node->rotate_buf, src_w, src_h, src_stride, dest_stride, rotation, cf);
            /*Use the rotated area and rotated buffer from now on*/

            color_ptr = node->rotate_buf;
            target_area = &rotated_area;
        }

        __disp_fill_display_framebuffer(target_area, color_ptr, cf, node->disp_fb, node->dev_info.is_swap);

        if (lv_display_flush_is_last(disp)) {
            tdl_disp_dev_flush(node->dev_hdl, node->disp_fb);

            TDL_DISP_FRAME_BUFF_T *next_fb = tdl_disp_get_free_fb(node->fb_mag);
            if(next_fb &&  next_fb != node->disp_fb) {
                __disp_framebuffer_memcpy(&node->dev_info, next_fb->frame, node->disp_fb->frame, node->disp_fb->len);
                node->disp_fb = next_fb;
            }
        }
    }

    lv_display_flush_ready(disp);

    tal_mutex_unlock(node->mutex);
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
