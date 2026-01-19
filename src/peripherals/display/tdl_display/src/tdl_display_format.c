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

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
#include "tal_dma2d.h"
#endif

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
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
static TAL_DMA2D_HANDLE_T sg_disp_dma2d_hdl = NULL;
#endif

static TDL_DISP_MONO_CFG_T sg_disp_mono_config = {
    .method = TDL_DISP_MONO_MTH_FLOYD_STEINBERG,
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

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
/**
 * @brief Convert YUV422 buffer to RGB format using DMA2D hardware acceleration
 * @param in_buf Pointer to the input YUV422 buffer
 * @param in_width Width of the input image in pixels
 * @param in_height Height of the input image in pixels
 * @param out_fb Pointer to the output framebuffer structure
 * @return OPRT_OK on success, error code otherwise
 */
static OPERATE_RET __disp_fb_convert_yuv422_to_rgb(uint8_t *in_buf, uint16_t in_width, uint16_t in_height, \
                                                    TDL_DISP_FRAME_BUFF_T *out_fb)
{
    OPERATE_RET rt = OPRT_OK;
    TKL_DMA2D_FRAME_INFO_T in_frame = {0};
    TKL_DMA2D_FRAME_INFO_T out_frame = {0};

    if (in_buf == NULL || out_fb == NULL) {
        return OPRT_INVALID_PARM;
    }

    memset(&in_frame, 0x00, sizeof(TKL_DMA2D_FRAME_INFO_T));
    memset(&out_frame, 0x00, sizeof(TKL_DMA2D_FRAME_INFO_T));

    // Perform memory copy based on color format
    switch (out_fb->fmt) {
        case TUYA_PIXEL_FMT_RGB565:
            out_frame.type = TUYA_FRAME_FMT_RGB565;
            break;
        case TUYA_PIXEL_FMT_RGB888:
            out_frame.type = TUYA_FRAME_FMT_RGB888;
            break;
        default:
            PR_ERR("Unsupported color format");
            return OPRT_INVALID_PARM;
    }

    if (NULL == sg_disp_dma2d_hdl) {
        TUYA_CALL_ERR_RETURN(tal_dma2d_init(&sg_disp_dma2d_hdl));
    }

    in_frame.type  = TUYA_FRAME_FMT_YUV422;
    in_frame.width  = in_width;
    in_frame.height = in_height;
    in_frame.pbuf   = in_buf;

    out_frame.width  = out_fb->width;
    out_frame.height = out_fb->height;
    out_frame.pbuf   = out_fb->frame;

    in_frame.width_cp  =  (in_width <= out_fb->width) ? in_width : out_fb->width;
    in_frame.height_cp =  (in_height <= out_fb->height) ? in_height : out_fb->height;
    
    TUYA_CALL_ERR_RETURN(tal_dma2d_convert(sg_disp_dma2d_hdl, &in_frame, &out_frame));

    TUYA_CALL_ERR_RETURN(tal_dma2d_wait_finish(sg_disp_dma2d_hdl, 2000));

    return rt;
}

#endif

extern OPERATE_RET tdl_disp_format_yuv422_to_binary(uint8_t *in_buf, \
                                                    uint16_t in_width,\
                                                    uint16_t in_height, 
                                                    TDL_DISP_MONO_CFG_T *cfg, \
                                                    TDL_DISP_FRAME_BUFF_T *out_fb);
/**
 * @brief Convert YUV422 buffer to monochrome format
 * @param in_buf Pointer to the input YUV422 buffer
 * @param in_width Width of the input image in pixels
 * @param in_height Height of the input image in pixels
 * @param out_fb Pointer to the output framebuffer structure
 * @return OPRT_OK on success, error code otherwise
 */
static OPERATE_RET __disp_fb_convert_yuv422_to_mono(uint8_t *in_buf, uint16_t in_width, \
                                                    uint16_t in_height, \
                                                    TDL_DISP_FRAME_BUFF_T *out_fb)
{
    return tdl_disp_format_yuv422_to_binary(in_buf, in_width, in_height, &sg_disp_mono_config, out_fb);
}

/**
 * @brief Convert YUV422 format buffer to the specified framebuffer pixel format
 * @param in_buf Pointer to the input YUV422 buffer
 * @param in_width Width of the input image in pixels
 * @param in_height Height of the input image in pixels
 * @param out_fb Pointer to the output framebuffer structure containing format and buffer information
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET tdl_disp_convert_yuv422_to_framebuffer(uint8_t *in_buf, uint16_t in_width,\
                                                   uint16_t in_height, \
                                                   TDL_DISP_FRAME_BUFF_T *out_fb)
{
    OPERATE_RET rt = OPRT_OK;

    if(in_buf == NULL || out_fb == NULL) {
        return OPRT_INVALID_PARM;
    }

    switch(out_fb->fmt) {
    case TUYA_PIXEL_FMT_RGB565:
    case TUYA_PIXEL_FMT_RGB888:
    #if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
        rt = __disp_fb_convert_yuv422_to_rgb(in_buf, in_width, in_height, out_fb);
    #else
        PR_ERR("DMA2D not enabled");
        rt = OPRT_NOT_SUPPORTED;
    #endif
        break;
    case TUYA_PIXEL_FMT_MONOCHROME:
        rt = __disp_fb_convert_yuv422_to_mono(in_buf, in_width, in_height, out_fb);
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
    if(NULL == cfg || cfg->method >= TDL_DISP_MONO_MTH_COUNT) {
        return OPRT_INVALID_PARM;
    }

    memcpy(&sg_disp_mono_config, cfg, sizeof(TDL_DISP_MONO_CFG_T));

    return OPRT_OK;
}



