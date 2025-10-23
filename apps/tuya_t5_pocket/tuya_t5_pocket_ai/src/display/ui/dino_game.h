/**
 * @file dino_game.h
 * Dino Game Component for AI Pocket Pet
 */

#ifndef DINO_GAME_H
#define DINO_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
// #include "ai_pocket_pet_app.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Show the dino game screen
 */
void dino_game_show(void);

/**
 * Check if dino game is currently active
 * @return 1 if game is active, 0 otherwise
 */
int dino_game_is_active(void);

/**
 * Handle key input for the dino game
 * @param key The key code to handle
 */
void dino_game_key_input(int key);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*DINO_GAME_H*/
