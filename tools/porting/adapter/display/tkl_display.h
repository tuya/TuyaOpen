/**
 * @file tkl_display.h
 * @brief Common process - display
 * @version 0.1
 * @date 2021-07-26
 *
 * @copyright Copyright 2019-2021 Tuya Inc. All Rights Reserved.
 *
 * 1. Support bit-block transfer of images.
 * 2. Support color filling.
 * 3. Need to support hotplug notifications for external devices such as HDMI/VGA/DP.
 * 4. Need to support querying and setting the format/resolution/refresh rate of external devices such as HDMI/VGA/DP.
 * 5. Need to support frame synchronization interfaces to avoid tearing during image refresh.
 * 6. Need to support multiple layers.
 * 7. Need to provide a memory management interface similar to gralloc, for directly mapping the kernel framebuffer or
 * dma-buf to applications, reducing copy operations.
 */

#ifndef __TKL_DISPLAY_H__
#define __TKL_DISPLAY_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TKL_DISP_LCD = 0, TKL_DISP_VGA, TKL_DISP_HDMI, TKL_DISP_DP, TKL_DISP_NUM } TKL_DISP_PORT_E;

typedef enum {
    TKL_DISP_PIXEL_FMT_ABGR = 0,
    TKL_DISP_PIXEL_FMT_RGBX,
    TKL_DISP_PIXEL_FMT_RGBA,
    TKL_DISP_PIXEL_FMT_ARGB,
    TKL_DISP_PIXEL_FMT_RGB565,
} TKL_DISP_PIXEL_FMT_E;

typedef enum {
    TKL_DISP_ROTATION_0 = 0,
    TKL_DISP_ROTATION_90,
    TKL_DISP_ROTATION_180,
    TKL_DISP_ROTATION_270
} TKL_DISP_ROTATION_E;

typedef enum { TKL_DISP_POWER_OFF = 0, TKL_DISP_POWER_ON, TKL_DISP_POWER_NUM } TKL_DISP_POWER_MODE_E;

typedef union {
    struct {
        uint16_t b : 5;
        uint16_t g : 6;
        uint16_t r : 5;
    } c16;

    struct {
        uint8_t b;
        uint8_t g;
        uint8_t r;
    } c24;

    struct {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    } c32;
    uint32_t full;
} TKL_DISP_COLOR_U;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} TKL_DISP_RECT_S;

typedef struct {
    void *buffer;
    TKL_DISP_RECT_S rect;
    TKL_DISP_PIXEL_FMT_E format;
    int priority;
} TKL_DISP_FRAMEBUFFER_S;

typedef void (*TKL_DISP_VSYNC_CB)(TKL_DISP_PORT_E port, int64_t timestamp);
typedef void (*TKL_DISP_HOTPLUG_CB)(TKL_DISP_PORT_E port, BOOL_T connected);
typedef struct {
    TKL_DISP_VSYNC_CB vsync_cb;
    TKL_DISP_HOTPLUG_CB hotplug_cb;
} TKL_DISP_EVENT_HANDLER_S;

typedef struct {
    int width;
    int height;
    int bpp;
    int dpi;
    int fps;
    TKL_DISP_PIXEL_FMT_E format;
    TKL_DISP_ROTATION_E rotation;
} TKL_DISP_INFO_S;

typedef struct {
    int device_id;
    void *device_info;
    TKL_DISP_PORT_E device_port;
} TKL_DISP_DEVICE_S;

/**
 * @brief Init and config display device
 *
 * @param display_device display device
 * @param cfg display configuration
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_disp_init(TKL_DISP_DEVICE_S *display_device, TKL_DISP_EVENT_HANDLER_S *event_handler);

/**
 * @brief Release display device
 *
 * @param display_device display device
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_disp_deinit(TKL_DISP_DEVICE_S *display_device);

/**
 * @brief Set display info
 *
 * @param display_device display device
 * @param info display device info
 * @return OPERATE_RET
 */
OPERATE_RET tkl_disp_set_info(TKL_DISP_DEVICE_S *display_device, TKL_DISP_INFO_S *info);

/**
 * @brief Get display info
 *
 * @param display_device display device
 * @param info display device info
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_disp_get_info(TKL_DISP_DEVICE_S *display_device, TKL_DISP_INFO_S *info);

/**
 * @brief
 *
 * @param display_device display device
 * @param buf framebuffer
 * @param rect destination area
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_disp_blit(TKL_DISP_DEVICE_S *display_device, TKL_DISP_FRAMEBUFFER_S *buf, TKL_DISP_RECT_S *rect);

/**
 * @brief Fill destination area with color
 *
 * @param display_device display device
 * @param rect destination area to fill
 * @param color color to fill
 * @return OPERATE_RET
 */
OPERATE_RET tkl_disp_fill(TKL_DISP_DEVICE_S *display_device, TKL_DISP_RECT_S *rect, TKL_DISP_COLOR_U color);

/**
 * @brief Flush buffers to display device
 *
 * @param display_device display device
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_disp_flush(TKL_DISP_DEVICE_S *display_device);

/**
 * @brief Wait for vsync signal
 *
 * @param display_device display device
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_disp_wait_vsync(TKL_DISP_DEVICE_S *display_device);

/**
 * @brief Set display brightness(Backlight or HSB)
 *
 * @param display_device display device
 * @param brightness brightness
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_disp_set_brightness(TKL_DISP_DEVICE_S *display_device, int brightness);

/**
 * @brief Get display brightness(Backlight or HSB)
 *
 * @param display_device display device
 * @param brightness brightness
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_disp_get_brightness(TKL_DISP_DEVICE_S *display_device, int *brightness);

/**
 * @brief Sets the display screen's power state
 *
 * @param display_device display device
 * @param power_mode power state
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_disp_set_power_mode(TKL_DISP_DEVICE_S *display_device, TKL_DISP_POWER_MODE_E power_mode);

/**
 * @brief Gets the display screen's power state
 *
 * @param display_device display device
 * @param power_mode power state
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_disp_get_power_mode(TKL_DISP_DEVICE_S *display_device, TKL_DISP_POWER_MODE_E *power_mode);

/**
 * @brief Alloc mapped framebuffer or layer
 *
 * @param display_device display device
 * @return void* Pointer to mapped memory
 */
TKL_DISP_FRAMEBUFFER_S *tkl_disp_alloc_framebuffer(TKL_DISP_DEVICE_S *display_device);

/**
 * @brief Free mapped framebuffer or layer
 *
 * @param display_device display device
 * @param buf Pointer to mapped memory
 * @return void
 */
void tkl_disp_free_framebuffer(TKL_DISP_DEVICE_S *display_device, TKL_DISP_FRAMEBUFFER_S *buf);

/**
 * @brief Get capabilities supported by display(For external display device. e.g. HDMI/VGA)
 *
 * @param display_device display device
 * @param cfg configurations
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_disp_get_capabilities(TKL_DISP_DEVICE_S *display_device, TKL_DISP_INFO_S **cfg);

/**
 * @brief Free capabilities get by tkl_disp_get_capabilities()
 *
 * @param display_device display device
 * @param cfg configurations
 * @return OPERATE_RET 0 on success. A negative error code on error.
 */
OPERATE_RET tkl_disp_free_capabilities(TKL_DISP_DEVICE_S *display_device, TKL_DISP_INFO_S *cfg);

#ifdef __cplusplus
}
#endif

#endif
