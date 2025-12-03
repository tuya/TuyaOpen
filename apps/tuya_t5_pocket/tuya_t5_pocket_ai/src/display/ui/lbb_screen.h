/**
 * @file lbb_screen.h
 * @brief Header for LBB screen display
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef __LBB_SCREEN_H__
#define __LBB_SCREEN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "screen_manager.h"

/**
 * @brief Initialize the LBB screen
 */
void lbb_screen_init(void);

/**
 * @brief Deinitialize the LBB screen
 */
void lbb_screen_deinit(void);

/**
 * @brief Create the LBB screen (alias for init)
 */
void lbb_screen_create(void);

#ifdef __cplusplus
}
#endif

#endif /*__LBB_SCREEN_H__*/
