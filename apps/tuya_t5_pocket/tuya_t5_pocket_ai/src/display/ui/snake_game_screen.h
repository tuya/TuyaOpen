/**
 * @file snake_game_screen.h
 * @brief Declaration of the snake game screen for the application
 *
 * This file contains the declarations for the snake game screen which provides
 * a classic snake game with collision detection, scoring, and game over handling.
 *
 * The snake game screen includes:
 * - Screen initialization and deinitialization functions
 * - Screen structure definition for the screen manager
 * - Game state management and controls
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef SNAKE_GAME_SCREEN_H
#define SNAKE_GAME_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

extern Screen_t snake_game_screen;

void snake_game_screen_init(void);
void snake_game_screen_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*SNAKE_GAME_SCREEN_H*/
