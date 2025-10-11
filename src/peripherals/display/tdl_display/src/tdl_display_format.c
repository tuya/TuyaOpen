/**
 * @file tdl_display_format.c
 * @brief tdl_display_format module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tdl_display_draw.h"

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


/***********************************************************
***********************function define**********************
***********************************************************/
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
 * @brief Gets the bits per pixel for the specified display pixel format.
 *
 * @param pixel_fmt Display pixel format enumeration value.
 * @return Bits per pixel for the given format, or 0 if unsupported.
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
 * @brief Converts a color value from the source pixel format to the destination pixel format.
 *
 * @param color Color value to convert.
 * @param src_fmt Source pixel format.
 * @param dst_fmt Destination pixel format.
 * @param threshold Threshold for monochrome conversion (0-65535).
 * @return Converted color value in the destination format.
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
 * @brief Converts a 16-bit RGB565 color value to the specified pixel format.
 *
 * @param rgb565 16-bit RGB565 color value.
 * @param fmt Destination pixel format.
 * @param threshold Threshold for monochrome conversion (0-65535).
 * @return Converted color value in the destination format.
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