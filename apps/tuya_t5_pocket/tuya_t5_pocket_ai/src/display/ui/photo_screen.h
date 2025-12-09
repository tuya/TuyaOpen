/**
 * @file lbb_screen.h
 * @brief Header for LBB screen display
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef __PHOTO_SCREEN_H__
#define __PHOTO_SCREEN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "screen_manager.h"

extern Screen_t photo_screen;

/**
 * @brief Initialize the PHOTO screen
 */
void photo_screen_init(void);

/**
 * @brief Deinitialize the PHOTO screen
 */
void photo_screen_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /*__PHOTO_SCREEN_H__*/
