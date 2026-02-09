/**
 * @file tdl_display_format.c
 * @brief Display pixel format conversion module
 *
 * This module provides functions for converting between different pixel formats,
 * including RGB565, RGB666, RGB888, monochrome, and I2 formats. It also supports
 * YUV422 to framebuffer conversion with hardware acceleration support via DMA2D.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tdl_display_manage.h"
#include "tdl_display_format.h"
/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef union {
    struct {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    };
    
    uint32_t whole;

}TDL_DISP_RGBA888_U;

typedef union {
    struct {
        uint16_t blue :  5;
        uint16_t green : 6;
        uint16_t red :   5;
    };

    uint16_t whole;
} TDL_DISP_RGB565_U;

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_DISP_MONO_CFG_T sg_disp_mono_config = {
    .method = TAL_IMAGE_MONO_MTH_FLOYD_STEINBERG,
    .fixed_threshold = 128,
    .invert_colors = 0,
};

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Convert color from specified pixel format to RGBA8888
 * @param fmt Source pixel format enumeration
 * @param color Color value in source format
 * @return RGBA8888 color value
 */
static TDL_DISP_RGBA888_U __disp_color_to_rgba888(TUYA_DISPLAY_PIXEL_FMT_E fmt, uint32_t color)
{
    TDL_DISP_RGBA888_U rgba;

    switch(fmt) {
        case TUYA_PIXEL_FMT_RGB565:
            rgba.r = (color & 0xF800) >>8;
            rgba.g = (color & 0x07E0) >> 3;
            rgba.b = (color & 0x001F) <<3;
            rgba.a = 0; 
        break;        
        case TUYA_PIXEL_FMT_RGB666:
            rgba.r = (color & 0x3F0000) >> 14;
            rgba.g = (color & 0x003F00) >> 6;
            rgba.b = (color & 0x00003F)<<2;
            rgba.a = 0;
        break;
        case TUYA_PIXEL_FMT_RGB888:     
            rgba.whole = color;
        break;
        case TUYA_PIXEL_FMT_MONOCHROME:{
            rgba.whole = (color) ? 0xFFFFFF : 0x00;
        }
        break;
        case TUYA_PIXEL_FMT_I2: {
            uint8_t level_idx = color & 0x03;
            uint8_t levels[4] = {0, 85, 170, 255};

            rgba.r = levels[level_idx];
            rgba.g = levels[level_idx];
            rgba.b = levels[level_idx];
            rgba.a = 0;
        }
        break;
        default:
            rgba.whole = color;
        break;
    }

    return rgba;
}

/**
 * @brief Convert RGBA8888 color to specified pixel format
 * @param fmt Destination pixel format enumeration
 * @param rgba Pointer to RGBA8888 color value
 * @param threshold Threshold value for monochrome conversion (0-65535)
 * @return Color value in destination format
 */
static uint32_t __disp_rgba888_to_color(TUYA_DISPLAY_PIXEL_FMT_E fmt, TDL_DISP_RGBA888_U *rgba, uint32_t threshold)
{
    uint32_t color = 0;

    switch(fmt) {
        case TUYA_PIXEL_FMT_RGB565:
            color = ((rgba->r & 0xF8) << 8) | ((rgba->g & 0xFC) << 3) | (rgba->b >> 3);
        break;
        case TUYA_PIXEL_FMT_RGB666:
            color = ((rgba->r & 0xFC) << 14) | ((rgba->g & 0xFC) << 6) | (rgba->b >> 2);
        break;
        case TUYA_PIXEL_FMT_RGB888:
            color = rgba->whole;
        break;
        case TUYA_PIXEL_FMT_MONOCHROME:
            color = (((rgba->r + rgba->g + rgba->b)/3) > threshold) ? 0x00 : 0x01;
        break;
        case TUYA_PIXEL_FMT_I2:{
            uint8_t gray = ~((rgba->r + rgba->g*2 + rgba->b) >>2);

            color = (uint32_t)gray;
        }
        break;
        default:
            color = rgba->whole;
            break;
    }

    return color;
}


/**
 * @brief Get the bits per pixel for the specified display pixel format
 * @param pixel_fmt Display pixel format enumeration value
 * @return Bits per pixel for the given format, or 0 if unsupported
 */
uint8_t tdl_disp_get_fmt_bpp(TUYA_DISPLAY_PIXEL_FMT_E pixel_fmt)
{
    uint8_t bpp = 0;

    switch (pixel_fmt) {
        case TUYA_PIXEL_FMT_RGB565:
            bpp = 16;
            break;
        case TUYA_PIXEL_FMT_RGB666:
        case TUYA_PIXEL_FMT_RGB888:
            bpp = 24;
            break;
        case TUYA_PIXEL_FMT_MONOCHROME:
            bpp = 1;
            break;
        case TUYA_PIXEL_FMT_I2:
            bpp = 2; // I2 format is typically 2 bits per pixel
            break;
        default:
            return 0;
    }

    return bpp;
}

/**
 * @brief Convert a color value from the source pixel format to the destination pixel format
 * @param color Color value to convert
 * @param src_fmt Source pixel format
 * @param dst_fmt Destination pixel format
 * @param threshold Threshold for monochrome conversion (0-65535)
 * @return Converted color value in the destination format
 */
uint32_t tdl_disp_convert_color_fmt(uint32_t color, TUYA_DISPLAY_PIXEL_FMT_E src_fmt,\
                                   TUYA_DISPLAY_PIXEL_FMT_E dst_fmt, uint32_t threshold)
{
    uint32_t converted_color = 0;
    TDL_DISP_RGBA888_U rgba;

    if (src_fmt == dst_fmt) {
        return color; // No conversion needed
    }

    rgba = __disp_color_to_rgba888(src_fmt, color);
    converted_color = __disp_rgba888_to_color(dst_fmt, &rgba, threshold);

    return converted_color;
}

/**
 * @brief Convert a 16-bit RGB565 color value to the specified pixel format
 * @param rgb565 16-bit RGB565 color value
 * @param fmt Destination pixel format
 * @param threshold Threshold for monochrome conversion (0-65535)
 * @return Converted color value in the destination format
 */
uint32_t tdl_disp_convert_rgb565_to_color(uint16_t rgb565, TUYA_DISPLAY_PIXEL_FMT_E fmt, uint32_t threshold)
{
    uint32_t color = 0;

    switch(fmt) {
        case TUYA_PIXEL_FMT_RGB666:
            color = (rgb565 & 0xF800) << 6 | (rgb565 & 0x07E0) <<3 | (rgb565 & 0x001F) <<1;
        break;
        case TUYA_PIXEL_FMT_RGB888:
            color = (rgb565 & 0xF800) << 8 | (rgb565 & 0x07E0) <<5 | (rgb565 & 0x001F) <<3;
        break;
        case TUYA_PIXEL_FMT_MONOCHROME: {
            color = (rgb565 >= threshold) ? 0x00 : 0x01;
        }
        break;
        case TUYA_PIXEL_FMT_I2:{
            TDL_DISP_RGB565_U rgb565_u = {
                .whole = rgb565,
            };

            uint8_t gray = ~(rgb565_u.red + rgb565_u.green*2 + rgb565_u.blue)>>2;

            color = (uint32_t)gray;
        }
        break;
        default:
            color = rgb565;
        break;
    }

    return color;
}

TDL_DISP_FRAME_BUFF_T *__create_rotate_tmp_fb(TDL_DISP_FRAME_BUFF_T *fb, TUYA_DISPLAY_ROTATION_E rotate)
{
    TDL_DISP_FRAME_BUFF_T *tmp_fb = NULL;

    if(rotate == TUYA_DISPLAY_ROTATION_0) {
        return NULL;
    }

    tmp_fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, fb->len);
    if(NULL == tmp_fb) {
        PR_ERR("Failed to create temporary frame buffer for rotation");
        return NULL;
    }

    tmp_fb->fmt = fb->fmt;
    if(rotate == TUYA_DISPLAY_ROTATION_90 || rotate == TUYA_DISPLAY_ROTATION_270) {
        tmp_fb->width  = fb->height;
        tmp_fb->height = fb->width;
    }else {
        tmp_fb->width  = fb->width;
        tmp_fb->height = fb->height;
    }

    return tmp_fb;
}

static OPERATE_RET __disp_fb_convert_yuv422_to_rgb565(uint8_t *in_buf, uint16_t in_width, uint16_t in_height, \
                                                      TDL_DISP_FRAME_BUFF_T *out_fb, bool is_swap, \
                                                      TUYA_DISPLAY_ROTATION_E rotate)
{
    OPERATE_RET rt = OPRT_OK;
    TAL_IMAGE_YUV422_TO_RGB_T conv_cfg = {0};
    TDL_DISP_FRAME_BUFF_T *p_tmp_fb = NULL;

    p_tmp_fb = __create_rotate_tmp_fb(out_fb, rotate);
    if(NULL == p_tmp_fb) {
        p_tmp_fb = out_fb;
    }

    memset(&conv_cfg, 0, sizeof(conv_cfg));

    conv_cfg.in_buf     = in_buf;
    conv_cfg.in_width   = in_width;
    conv_cfg.in_height  = in_height;
    conv_cfg.out_buf    = p_tmp_fb->frame;
    conv_cfg.out_width  = p_tmp_fb->width;
    conv_cfg.out_height = p_tmp_fb->height;

    rt = tal_image_convert_yuv422_to_rgb565(&conv_cfg);
    if(rt != OPRT_OK) {
        PR_ERR("YUV422 to RGB565 conversion failed, rt=%d", rt);
        if(p_tmp_fb != out_fb) {
            tdl_disp_free_frame_buff(p_tmp_fb);
        }
        return rt;
    }

    if(p_tmp_fb != out_fb) {
        rt = tdl_disp_draw_rotate(rotate, p_tmp_fb, out_fb, is_swap);
        tdl_disp_free_frame_buff(p_tmp_fb);
    } else if(is_swap) {
        rt = tdl_disp_dev_rgb565_swap((uint16_t *)out_fb->frame, out_fb->len / 2);
    }

    return rt;
}

static OPERATE_RET __disp_fb_convert_yuv422_to_rgb888(uint8_t *in_buf, uint16_t in_width, uint16_t in_height, \
                                                      TDL_DISP_FRAME_BUFF_T *out_fb,\
                                                      TUYA_DISPLAY_ROTATION_E rotate)
{
    OPERATE_RET rt = OPRT_OK;
    TAL_IMAGE_YUV422_TO_RGB_T conv_cfg = {0};
    TDL_DISP_FRAME_BUFF_T *p_tmp_fb = NULL;

    p_tmp_fb = __create_rotate_tmp_fb(out_fb, rotate);
    if(NULL == p_tmp_fb) {
        p_tmp_fb = out_fb;
    }

    memset(&conv_cfg, 0, sizeof(conv_cfg));

    conv_cfg.in_buf     = in_buf;
    conv_cfg.in_width   = in_width;
    conv_cfg.in_height  = in_height;
    conv_cfg.out_buf    = p_tmp_fb->frame;
    conv_cfg.out_width  = p_tmp_fb->width;
    conv_cfg.out_height = p_tmp_fb->height;

    rt = tal_image_convert_yuv422_to_rgb888(&conv_cfg);
    if(rt != OPRT_OK) {
        PR_ERR("YUV422 to RGB888 conversion failed, rt=%d", rt);
        if(p_tmp_fb != out_fb) {
            tdl_disp_free_frame_buff(p_tmp_fb);
        }
        return rt;
    }

    if(p_tmp_fb != out_fb) {
        rt = tdl_disp_draw_rotate(rotate, p_tmp_fb, out_fb, false);
        tdl_disp_free_frame_buff(p_tmp_fb);
    }

    return rt;
}

static OPERATE_RET __disp_fb_convert_yuv422_to_mono(uint8_t *in_buf, uint16_t in_width, uint16_t in_height, \
                                                    TDL_DISP_FRAME_BUFF_T *out_fb,
                                                    TUYA_DISPLAY_ROTATION_E rotate)
{
    OPERATE_RET rt = OPRT_OK;
    TAL_IMAGE_YUV422_TO_BINARY_T conv_cfg = {0};
    TDL_DISP_FRAME_BUFF_T *p_tmp_fb = NULL;

    p_tmp_fb = __create_rotate_tmp_fb(out_fb, rotate);
    if(NULL == p_tmp_fb) {
        p_tmp_fb = out_fb;
    }

    memset(&conv_cfg, 0, sizeof(conv_cfg));

    conv_cfg.method          = sg_disp_mono_config.method;
    conv_cfg.fixed_threshold = sg_disp_mono_config.fixed_threshold;
    conv_cfg.invert_colors   = sg_disp_mono_config.invert_colors;
    conv_cfg.in_buf          = in_buf;
    conv_cfg.in_width        = in_width;
    conv_cfg.in_height       = in_height;
    conv_cfg.out_buf         = p_tmp_fb->frame;
    conv_cfg.out_width       = p_tmp_fb->width;
    conv_cfg.out_height      = p_tmp_fb->height;

    rt = tal_image_format_yuv422_to_binary(&conv_cfg);
    if(rt != OPRT_OK) {
        PR_ERR("YUV422 to binary conversion failed, rt=%d", rt);
        if(p_tmp_fb != out_fb) {
            tdl_disp_free_frame_buff(p_tmp_fb);
        }
        return rt;
    }

    if(p_tmp_fb != out_fb) {
        rt = tdl_disp_draw_rotate(rotate, p_tmp_fb, out_fb, false);
        tdl_disp_free_frame_buff(p_tmp_fb);
    }

    return rt;
}

OPERATE_RET tdl_disp_convert_yuv422_to_fb(uint8_t *in_buf, uint16_t in_width, uint16_t in_height, \
                                          TDL_DISP_FRAME_BUFF_T *out_fb, bool is_swap, \
                                          TUYA_DISPLAY_ROTATION_E rotate)
{
    OPERATE_RET rt = OPRT_OK;

    if(in_buf == NULL || out_fb == NULL || \
       out_fb->frame == NULL || out_fb->len == 0) {
        return OPRT_INVALID_PARM;
    }

    switch(out_fb->fmt) {
    case TUYA_PIXEL_FMT_RGB565:
        rt = __disp_fb_convert_yuv422_to_rgb565(in_buf, in_width, in_height, out_fb, is_swap, rotate);
        break;
    case TUYA_PIXEL_FMT_RGB888:
        rt = __disp_fb_convert_yuv422_to_rgb888(in_buf, in_width, in_height, out_fb, rotate);
        break;
    case TUYA_PIXEL_FMT_MONOCHROME:
        rt = __disp_fb_convert_yuv422_to_mono(in_buf, in_width, in_height, out_fb, rotate);
        break;
    default:
        PR_ERR("Unsupported pixel format:%d", out_fb->fmt); 
        rt = OPRT_NOT_SUPPORTED;
        break;  
    }

    return rt;
}

/**
 * @brief Set monochrome conversion parameters
 * @param cfg Pointer to the monochrome conversion configuration structure
 * @return OPRT_OK on success, OPRT_INVALID_PARM if cfg is NULL or method is invalid
 */
OPERATE_RET tdl_disp_set_mono_convert_param(TDL_DISP_MONO_CFG_T *cfg)
{
    if(NULL == cfg || cfg->method >= TAL_IMAGE_MONO_MTH_COUNT) {
        return OPRT_INVALID_PARM;
    }

    memcpy(&sg_disp_mono_config, cfg, sizeof(TDL_DISP_MONO_CFG_T));

    return OPRT_OK;
}



