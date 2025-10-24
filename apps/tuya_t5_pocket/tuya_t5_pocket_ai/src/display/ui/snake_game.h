/**
 * @file snake_game.h
 * Snake Game Component for AI Pocket Pet
 */

#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

/*********************
 *      INCLUDES
 *********************/
#include "ai_pocket_pet_app.h"

/*********************
 *      DEFINES
 *********************/

/*********************
 *      TYPEDEFS
 **********************/

/*********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief Show and start the Snake game
 * @param None
 * @return None
 *
 * Create the game UI and start the game loop
 */
void snake_game_show(void);

/**
 * @brief Check if the Snake game is active
 * @param None
 * @return 1 if the game is active, 0 otherwise
 */
int snake_game_is_active(void);

/**
 * @brief Handle keyboard input for the game
 * @param key The key code pressed
 * @return None
 *
 * Handle key input events for the game
 */
void snake_game_key_input(int key);

/*********************
 *      MACROS
 **********************/

#endif /* SNAKE_GAME_H */
