/**
 * @file tdl_display_format.h
 * @brief Display pixel format conversion module header file
 *
 * This header file provides function declarations and type definitions for converting
 * between different display pixel formats, including RGB565, RGB666, RGB888, monochrome,
 * and I2 formats. It also supports YUV422 to framebuffer conversion and monochrome
 * conversion with various dithering and error diffusion algorithms.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_DISPLAY_FORMAT_H__
#define __TDL_DISPLAY_FORMAT_H__

#include "tuya_cloud_types.h"
#include "tdl_display_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Binary conversion method enum
 */
typedef enum {
    TDL_DISP_MONO_MTH_FIXED = 0,       // Fixed threshold
    TDL_DISP_MONO_MTH_ADAPTIVE,        // Adaptive threshold
    TDL_DISP_MONO_MTH_OTSU,            // Otsu's method
    TDL_DISP_MONO_MTH_BAYER8_DITHER,   // 8-level grayscale Bayer dithering (3x3)
    TDL_DISP_MONO_MTH_BAYER4_DITHER,   // 4-level grayscale Bayer dithering (2x2)
    TDL_DISP_MONO_MTH_BAYER16_DITHER,  // 16-level grayscale Bayer dithering (4x4)
    TDL_DISP_MONO_MTH_FLOYD_STEINBERG, // Floyd-Steinberg error diffusion
    TDL_DISP_MONO_MTH_STUCKI,          // Stucki error diffusion
    TDL_DISP_MONO_MTH_JARVIS,          // Jarvis-Judice-Ninke error diffusion
    TDL_DISP_MONO_MTH_COUNT            // Total number of methods
} TDL_DISP_MONO_METHOD_E;

/**
 * @brief Binary conversion configuration
 */
typedef struct {
    TDL_DISP_MONO_METHOD_E method;
    uint8_t                fixed_threshold;
    uint8_t                invert_colors;             // 1: bit=1->white (LVGL), 0: bit=1->black (printer)
} TDL_DISP_MONO_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Gets the bits per pixel for the specified display pixel format.
 *
 * @param pixel_fmt Display pixel format enumeration value.
 * @return Bits per pixel for the given format, or 0 if unsupported.
 */
uint8_t tdl_disp_get_fmt_bpp(TUYA_DISPLAY_PIXEL_FMT_E pixel_fmt);

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
                                   TUYA_DISPLAY_PIXEL_FMT_E dst_fmt, uint32_t threshold);

/**
 * @brief Converts a 16-bit RGB565 color value to the specified pixel format.
 *
 * @param rgb565 16-bit RGB565 color value.
 * @param fmt Destination pixel format.
 * @param threshold Threshold for monochrome conversion (0-65535).
 * @return Converted color value in the destination format.
 */
uint32_t tdl_disp_convert_rgb565_to_color(uint16_t rgb565, TUYA_DISPLAY_PIXEL_FMT_E fmt, uint32_t threshold);

/**
 * @brief Converts YUV422 format buffer to the specified framebuffer pixel format.
 * @param in_buf Pointer to the input YUV422 buffer.
 * @param in_width Width of the input image in pixels.
 * @param in_height Height of the input image in pixels.
 * @param out_fb Pointer to the output framebuffer structure containing format and buffer information.
 * @return OPERATE_RET Returns OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tdl_disp_convert_yuv422_to_framebuffer(uint8_t *in_buf, uint16_t in_width,\
                                                   uint16_t in_height, \
                                                   TDL_DISP_FRAME_BUFF_T *out_fb);

/**
 * @brief Set monochrome conversion parameters
 * @param cfg Pointer to the monochrome conversion configuration structure
 * @return OPRT_OK on success, OPRT_INVALID_PARM if cfg is NULL or method is invalid
 */
 OPERATE_RET tdl_disp_set_mono_convert_param(TDL_DISP_MONO_CFG_T *cfg);


#ifdef __cplusplus
}
#endif

#endif /* __TDL_DISPLAY_FORMAT_H__ */
