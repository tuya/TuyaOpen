/**
 * @file tdd_disp_sdl.h
 * @brief SDL2 based virtual display driver for LINUX/Raspberry_Pi.
 */

#ifndef __TDD_DISP_SDL_H__
#define __TDD_DISP_SDL_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register an SDL2 window as a TDL display device.
 *
 * @param name   Display device name, should match DISPLAY_NAME.
 * @param width  Window width in pixels.
 * @param height Window height in pixels.
 *
 * @return OPERATE_RET OPRT_OK on success.
 */
OPERATE_RET tdd_disp_sdl_register(char *name, uint16_t width, uint16_t height);

/**
 * @brief Register an SDL2 virtual touchpad as a TDL touchpad device.
 *
 * This is used on desktop (SDL window) to drive LVGL pointer input:
 * - Mouse left-button drag: press/move/release as single-touch
 * - Mouse wheel: generates a short swipe gesture for vertical scrolling
 *
 * @param name   Touchpad device name, typically the same as DISPLAY_NAME.
 * @param width  Window width in pixels.
 * @param height Window height in pixels.
 *
 * @return OPERATE_RET OPRT_OK on success.
 */
OPERATE_RET tdd_tp_sdl_register(char *name, uint16_t width, uint16_t height);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_SDL_H__ */
