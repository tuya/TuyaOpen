/**
 * @file dino_game_screen.h
 * @brief Declaration of the dino game screen for the application
 *
 * This file contains the declarations for the dino game screen which provides
 * a Chrome-style dinosaur jumping game with obstacles, scoring, and physics.
 *
 * The dino game screen includes:
 * - Screen initialization and deinitialization functions
 * - Screen structure definition for the screen manager
 * - Game state management and controls
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef DINO_GAME_SCREEN_H
#define DINO_GAME_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

extern Screen_t dino_game_screen;

void dino_game_screen_init(void);
void dino_game_screen_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*DINO_GAME_SCREEN_H*/
