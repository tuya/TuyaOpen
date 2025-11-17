/**
 * @file snake_game_screen.c
 * @brief Implementation of the snake game screen for the application
 *
 * This file contains the implementation of the snake game screen which provides
 * a classic snake game with collision detection, scoring, and game over handling.
 *
 * The snake game screen includes:
 * - Classic snake game mechanics with grid-based movement
 * - Food generation and collision detection
 * - Score tracking and game over handling
 * - Game state management with pause/resume functionality
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "snake_game_screen.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#if defined(ENABLE_LVGL_HARDWARE)
#include "tal_kv.h"
#include "tal_system.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/

// Snake game constants
#define SNAKE_GRID_SIZE     16    /* Size of each grid cell in pixels */
#define SNAKE_GRID_WIDTH    23    /* Number of grid cells horizontally */
#define SNAKE_GRID_HEIGHT   8     /* Number of grid cells vertically */
#define SNAKE_GAME_WIDTH    (SNAKE_GRID_WIDTH * SNAKE_GRID_SIZE)
#define SNAKE_GAME_HEIGHT   (SNAKE_GRID_HEIGHT * SNAKE_GRID_SIZE)
#define SNAKE_MAX_LENGTH    (SNAKE_GRID_WIDTH * SNAKE_GRID_HEIGHT)
#define SNAKE_INITIAL_X     (SNAKE_GRID_WIDTH / 2)
#define SNAKE_INITIAL_Y     (SNAKE_GRID_HEIGHT / 2)
#define SNAKE_TIMER_PERIOD  300   /* Game update interval in ms */
#define HIGH_SCORE          50    /* Historical high score */

// Simple LFSR random number generator
#define LFSR_SEED           0x1234
#define LFSR_POLYNOMIAL     0x8016

// KV storage key for high score
#define SNAKE_GAME_HIGH_SCORE_KV_KEY "snake_high_score"

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_snake_game_screen;
static lv_timer_t *game_timer;

// Game objects
static lv_obj_t *game_canvas = NULL;
static lv_obj_t *score_label = NULL;
static lv_obj_t *snake_segments[SNAKE_MAX_LENGTH];
static lv_obj_t *food_obj = NULL;

// Snake direction enumeration
typedef enum {
    SNAKE_DIR_UP = 0,
    SNAKE_DIR_DOWN,
    SNAKE_DIR_LEFT,
    SNAKE_DIR_RIGHT
} snake_direction_t;

// Snake point structure
typedef struct {
    uint8_t x;
    uint8_t y;
} snake_point_t;

// Game state structure
typedef struct {
    // Snake body
    snake_point_t body[SNAKE_MAX_LENGTH];
    uint8_t length;
    uint8_t direction : 2;
    uint8_t next_direction : 2;
    uint8_t game_over : 1;
    uint8_t initialized : 1;
    uint8_t paused : 1;
    uint8_t show_exit_dialog : 1;
    uint8_t exit_selection : 1;
    uint8_t show_game_over_dialog : 1;
    uint8_t game_over_selection : 1;
    uint8_t speed : 4;
    uint8_t _reserved : 1;

    // Food position
    snake_point_t food;

    // Game state
    uint16_t score;
} snake_game_state_t;

static snake_game_state_t g_gs;
static uint16_t g_lfsr_state = LFSR_SEED;
static uint16_t g_last_drawn_length = 0;
static uint16_t g_high_score = 0;  // High score loaded from KV storage

// Dialog UI elements
static lv_obj_t *exit_dialog = NULL;
static lv_obj_t *exit_msg_label = NULL;
static lv_obj_t *exit_yes_btn = NULL;
static lv_obj_t *exit_no_btn = NULL;

static lv_obj_t *game_over_dialog = NULL;
static lv_obj_t *game_over_high_score_label = NULL;
static lv_obj_t *game_over_current_score_label = NULL;
static lv_obj_t *game_over_msg_label = NULL;
static lv_obj_t *game_over_yes_btn = NULL;
static lv_obj_t *game_over_no_btn = NULL;

Screen_t snake_game_screen = {
    .init = snake_game_screen_init,
    .deinit = snake_game_screen_deinit,
    .screen_obj = &ui_snake_game_screen,
    .name = "snake_game",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void game_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);
static void snake_game_restart(void);
static void snake_game_show_exit_dialog(void);
static void snake_game_hide_exit_dialog(void);
static void snake_game_show_game_over_dialog(void);
static void snake_game_hide_game_over_dialog(void);
static void snake_game_update_exit_selection(void);
static void snake_game_update_game_over_selection(void);
static void snake_game_generate_food(void);
static void snake_game_draw_snake(void);
static void snake_game_draw_food(void);
static uint8_t snake_game_check_collision(void);
static uint8_t snake_game_check_food_collision(void);
static void snake_game_move_snake(void);
static inline uint16_t snake_game_lfsr_random(void);
static void snake_game_load_high_score(void);
static void snake_game_save_high_score(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Load high score from KV storage
 */
static void snake_game_load_high_score(void)
{
#if defined(ENABLE_LVGL_HARDWARE)
    uint8_t *stored_score = NULL;
    size_t score_length = 0;
    int ret = tal_kv_get(SNAKE_GAME_HIGH_SCORE_KV_KEY, &stored_score, &score_length);
    
    if (ret == 0 && stored_score != NULL && score_length == sizeof(uint16_t)) {
        // Successfully loaded high score from storage
        memcpy(&g_high_score, stored_score, sizeof(uint16_t));
        printf("[snake_game] High score loaded from KV storage: %d\n", g_high_score);
        tal_kv_free(stored_score);
    } else {
        // No stored score found or error occurred, use default
        g_high_score = 0;
        printf("[snake_game] No high score in KV storage (ret=%d), using default: 0\n", ret);
    }
#else
    // PC simulator mode - use default score
    g_high_score = 50;
    printf("[snake_game] PC simulator mode - using default high score: 50\n");
#endif
}

/**
 * @brief Save high score to KV storage
 */
static void snake_game_save_high_score(void)
{
#if defined(ENABLE_LVGL_HARDWARE)
    int ret = tal_kv_set(SNAKE_GAME_HIGH_SCORE_KV_KEY, (const uint8_t *)&g_high_score, sizeof(uint16_t));
    if (ret == 0) {
        printf("[snake_game] High score saved to KV storage: %d\n", g_high_score);
    } else {
        printf("[snake_game] Failed to save high score to KV storage, error: %d\n", ret);
    }
#else
    printf("[snake_game] KV storage not available (PC simulator mode), high score: %d\n", g_high_score);
#endif
}

/**
 * @brief LFSR random number generator
 */
static inline uint16_t snake_game_lfsr_random(void)
{
#if defined(ENABLE_LVGL_HARDWARE)
    // Use Tuya system random function on hardware
    return (uint16_t)tal_system_get_random(0xFFFF);
#else
    // Use LFSR for PC simulator
    uint8_t bit = ((g_lfsr_state >> 0) ^ (g_lfsr_state >> 2) ^
                   (g_lfsr_state >> 3) ^ (g_lfsr_state >> 5)) & 1;
    g_lfsr_state = (g_lfsr_state >> 1) | (bit << 15);
    return g_lfsr_state;
#endif
}

/**
 * @brief Generate food at random position
 */
static void snake_game_generate_food(void)
{
    uint8_t valid_position;

    do {
        valid_position = 1;
        g_gs.food.x = (uint8_t)(snake_game_lfsr_random() % SNAKE_GRID_WIDTH);
        g_gs.food.y = (uint8_t)(snake_game_lfsr_random() % SNAKE_GRID_HEIGHT);

        // Check if food position overlaps with snake
        for (uint8_t i = 0; i < g_gs.length; i++) {
            if (g_gs.food.x == g_gs.body[i].x && g_gs.food.y == g_gs.body[i].y) {
                valid_position = 0;
                break;
            }
        }
    } while (!valid_position);
}

/**
 * @brief Draw snake segments
 */
static void snake_game_draw_snake(void)
{
    // Initialize snake segments on first call or when length increases
    if (g_gs.length > g_last_drawn_length) {
        for (uint16_t i = g_last_drawn_length; i < g_gs.length; i++) {
            snake_segments[i] = lv_obj_create(game_canvas);
            lv_obj_set_size(snake_segments[i], SNAKE_GRID_SIZE, SNAKE_GRID_SIZE);
            lv_obj_set_style_bg_opa(snake_segments[i], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(snake_segments[i], 1, 0);
            lv_obj_set_style_border_color(snake_segments[i], lv_color_black(), 0);
            lv_obj_set_style_radius(snake_segments[i], 2, 0);
        }
        g_last_drawn_length = g_gs.length;
    }

    // Update positions and colors of existing segments
    for (uint8_t i = 0; i < g_gs.length; i++) {
        if (snake_segments[i]) {
            // Make sure segment is visible
            lv_obj_clear_flag(snake_segments[i], LV_OBJ_FLAG_HIDDEN);

            // Calculate pixel position normally
            lv_coord_t pixel_x = g_gs.body[i].x * SNAKE_GRID_SIZE;
            lv_coord_t pixel_y = g_gs.body[i].y * SNAKE_GRID_SIZE;

            lv_obj_set_pos(snake_segments[i], pixel_x, pixel_y);

            // Head is darker, body is lighter
            if (i == 0) {
                lv_obj_set_style_bg_color(snake_segments[i], lv_color_black(), 0);
            } else {
                lv_obj_set_style_bg_color(snake_segments[i], lv_color_make(0x80, 0x80, 0x80), 0);
            }
        }
    }

    // Hide unused segments if snake got shorter (shouldn't happen in this game, but safety)
    for (int i = g_gs.length; i < g_last_drawn_length; i++) {
        if (snake_segments[i]) {
            lv_obj_add_flag(snake_segments[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/**
 * @brief Draw food
 */
static void snake_game_draw_food(void)
{
    // Create food object only once
    if (!food_obj) {
        food_obj = lv_obj_create(game_canvas);
        lv_obj_set_size(food_obj, SNAKE_GRID_SIZE - 6, SNAKE_GRID_SIZE - 6);  // Smaller size for diamond shape
        lv_obj_set_style_bg_color(food_obj, lv_color_black(), 0);  // Black diamond
        lv_obj_set_style_bg_opa(food_obj, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(food_obj, 0, 0);  // No border for clean look
        lv_obj_set_style_radius(food_obj, 2, 0);  // Small radius

        // Transform to diamond shape by rotating 45 degrees
        lv_obj_set_style_transform_angle(food_obj, 450, 0);  // 45 degrees in 0.1 degree units
    }

    // Update food position
    lv_obj_set_pos(food_obj,
                  g_gs.food.x * SNAKE_GRID_SIZE + 3,
                  g_gs.food.y * SNAKE_GRID_SIZE + 3);
}

/**
 * @brief Move snake
 */
static void snake_game_move_snake(void)
{
    // Calculate new head position
    snake_point_t new_head = g_gs.body[0];

    switch (g_gs.direction) {
        case SNAKE_DIR_UP:
            if (new_head.y > 0) new_head.y--;
            break;
        case SNAKE_DIR_DOWN:
            new_head.y++;
            break;
        case SNAKE_DIR_LEFT:
            if (new_head.x > 0) new_head.x--;
            break;
        case SNAKE_DIR_RIGHT:
            new_head.x++;
            break;
    }

    // Move body segments
    for (uint8_t i = g_gs.length - 1; i > 0; i--) {
        g_gs.body[i] = g_gs.body[i - 1];
    }

    // Set new head
    g_gs.body[0] = new_head;
}

/**
 * @brief Check collision with walls or self
 */
static uint8_t snake_game_check_collision(void)
{
    snake_point_t head = g_gs.body[0];

    // Check wall collision with uint8_t boundary detection
    // Since we use uint8_t coordinates, no negative wrap-around possible
    // Valid coordinates: x in [0, SNAKE_GRID_WIDTH-1], y in [0, SNAKE_GRID_HEIGHT-1]
    if (head.x >= SNAKE_GRID_WIDTH || head.y >= SNAKE_GRID_HEIGHT) {
        return 1;
    }

    // Check self collision
    for (uint8_t i = 1; i < g_gs.length; i++) {
        if (head.x == g_gs.body[i].x && head.y == g_gs.body[i].y) {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Check food collision
 */
static uint8_t snake_game_check_food_collision(void)
{
    return (g_gs.body[0].x == g_gs.food.x && g_gs.body[0].y == g_gs.food.y);
}

/**
 * @brief Game timer callback
 */
static void game_timer_cb(lv_timer_t *timer)
{
    if (g_gs.game_over || !g_gs.initialized || g_gs.paused) {
        return;
    }

    // Update direction
    g_gs.direction = g_gs.next_direction;

    // Move snake
    snake_game_move_snake();

    // Check wall collision
    if (snake_game_check_collision()) {
        g_gs.game_over = 1;
        
        // Check and update high score
        if (g_gs.score > g_high_score) {
            g_high_score = g_gs.score;
            snake_game_save_high_score();
            printf("[snake_game] New high score: %d\n", g_high_score);
        }
        
        char buf[32];
        snprintf(buf, sizeof(buf), "GAME OVER: %d", g_gs.score);
        lv_label_set_text(score_label, buf);
        snake_game_show_game_over_dialog();
        return;
    }

    // Check food collision
    if (snake_game_check_food_collision()) {
        g_gs.score++;
        g_gs.length++;

        // Update score display
        char buf[32];
        snprintf(buf, sizeof(buf), "SCORE: %d", g_gs.score);
        lv_label_set_text(score_label, buf);

        // Generate new food
        snake_game_generate_food();

        // Increase speed slightly
        if (g_gs.score % 5 == 0 && SNAKE_TIMER_PERIOD > 100) {
            lv_timer_set_period(game_timer, SNAKE_TIMER_PERIOD - (g_gs.score / 5) * 20);
        }
    }

    // Redraw game
    snake_game_draw_snake();
    snake_game_draw_food();
}

/**
 * @brief Restart the game
 */
static void snake_game_restart(void)
{
    // Hide game over dialog first
    snake_game_hide_game_over_dialog();

    // Reset game state
    g_gs.length = 3;
    g_gs.direction = SNAKE_DIR_RIGHT;
    g_gs.next_direction = SNAKE_DIR_RIGHT;
    g_gs.score = 0;
    g_gs.speed = 1;
    g_gs.game_over = 0;
    g_gs.paused = 0;
    g_gs.show_game_over_dialog = 0;
    g_gs.game_over_selection = 0;

    // Reset snake position
    g_gs.body[0].x = SNAKE_INITIAL_X;
    g_gs.body[0].y = SNAKE_INITIAL_Y;
    g_gs.body[1].x = SNAKE_INITIAL_X - 1;
    g_gs.body[1].y = SNAKE_INITIAL_Y;
    g_gs.body[2].x = SNAKE_INITIAL_X - 2;
    g_gs.body[2].y = SNAKE_INITIAL_Y;

    // Reset timer speed
    lv_timer_set_period(game_timer, SNAKE_TIMER_PERIOD);

    // Generate new food
    snake_game_generate_food();

    // Update score display
    lv_label_set_text(score_label, "SCORE: 0");

    // Reset object pool for restart - hide excess snake segments
    uint16_t old_length = g_last_drawn_length;
    g_last_drawn_length = 3;  // Reset to initial snake length

    // Hide any excess snake segments from previous game
    for (uint16_t i = 3; i < old_length; i++) {
        if (snake_segments[i]) {
            lv_obj_add_flag(snake_segments[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Redraw game using object pool - no clearing needed
    snake_game_draw_snake();
    snake_game_draw_food();
}

/**
 * @brief Show exit confirmation dialog
 */
static void snake_game_show_exit_dialog(void)
{
    if (exit_dialog) return;

    g_gs.paused = 1;
    g_gs.show_exit_dialog = 1;
    g_gs.exit_selection = 0;  // Default to "No"

    // Create modal dialog background
    exit_dialog = lv_obj_create(ui_snake_game_screen);
    lv_obj_set_size(exit_dialog, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(exit_dialog, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(exit_dialog, LV_OPA_70, 0);
    lv_obj_set_pos(exit_dialog, 0, 0);

    // Create dialog box
    lv_obj_t *dialog_box = lv_obj_create(exit_dialog);
    lv_obj_set_size(dialog_box, 200, 120);
    lv_obj_center(dialog_box);
    lv_obj_set_style_bg_color(dialog_box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(dialog_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dialog_box, 2, 0);
    lv_obj_set_style_border_color(dialog_box, lv_color_black(), 0);

    // Message label
    exit_msg_label = lv_label_create(dialog_box);
    lv_label_set_text(exit_msg_label, "Exit Game?");
    lv_obj_set_style_text_font(exit_msg_label, &lv_font_montserrat_16, 0);
    lv_obj_align(exit_msg_label, LV_ALIGN_TOP_MID, 0, 15);

    // Calculate symmetric positions for buttons
    lv_coord_t btn_width = 70;
    lv_coord_t btn_height = 30;
    lv_coord_t btn_spacing = 50;

    // No button (default selected)
    exit_no_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(exit_no_btn, btn_width, btn_height);
    lv_obj_align(exit_no_btn, LV_ALIGN_BOTTOM_MID, -btn_spacing, -5);
    lv_obj_set_style_bg_color(exit_no_btn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(exit_no_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(exit_no_btn, 2, 0);
    lv_obj_set_style_border_color(exit_no_btn, lv_color_black(), 0);

    lv_obj_t *no_label = lv_label_create(exit_no_btn);
    lv_label_set_text(no_label, "NO");
    lv_obj_center(no_label);
    lv_obj_set_style_text_color(no_label, lv_color_white(), 0);

    // Yes button
    exit_yes_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(exit_yes_btn, btn_width, btn_height);
    lv_obj_align(exit_yes_btn, LV_ALIGN_BOTTOM_MID, btn_spacing, -5);
    lv_obj_set_style_bg_color(exit_yes_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(exit_yes_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(exit_yes_btn, 2, 0);
    lv_obj_set_style_border_color(exit_yes_btn, lv_color_black(), 0);

    lv_obj_t *yes_label = lv_label_create(exit_yes_btn);
    lv_label_set_text(yes_label, "YES");
    lv_obj_center(yes_label);
    lv_obj_set_style_text_color(yes_label, lv_color_black(), 0);
}

/**
 * @brief Hide exit dialog
 */
static void snake_game_hide_exit_dialog(void)
{
    if (!exit_dialog) return;

    lv_obj_del(exit_dialog);
    exit_dialog = NULL;
    exit_msg_label = NULL;
    exit_yes_btn = NULL;
    exit_no_btn = NULL;

    g_gs.paused = 0;
    g_gs.show_exit_dialog = 0;
}

/**
 * @brief Show game over dialog
 */
static void snake_game_show_game_over_dialog(void)
{
    if (game_over_dialog) return;

    g_gs.show_game_over_dialog = 1;
    g_gs.game_over_selection = 0;  // Default to "Yes" (play again)

    // Create modal dialog background
    game_over_dialog = lv_obj_create(ui_snake_game_screen);
    lv_obj_set_size(game_over_dialog, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(game_over_dialog, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(game_over_dialog, LV_OPA_70, 0);
    lv_obj_set_pos(game_over_dialog, 0, 0);

    // Create dialog box
    lv_obj_t *dialog_box = lv_obj_create(game_over_dialog);
    lv_obj_set_size(dialog_box, 220, 160);
    lv_obj_center(dialog_box);
    lv_obj_set_style_bg_color(dialog_box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(dialog_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dialog_box, 2, 0);
    lv_obj_set_style_border_color(dialog_box, lv_color_black(), 0);

    // High score label
    game_over_high_score_label = lv_label_create(dialog_box);
    char high_score_text[32];
    snprintf(high_score_text, sizeof(high_score_text), "Highest Score: %d", g_high_score);
    lv_label_set_text(game_over_high_score_label, high_score_text);
    lv_obj_set_style_text_font(game_over_high_score_label, &lv_font_montserrat_14, 0);
    lv_obj_align(game_over_high_score_label, LV_ALIGN_TOP_MID, 0, 10);

    // Current score label
    game_over_current_score_label = lv_label_create(dialog_box);
    char current_score_text[32];
    snprintf(current_score_text, sizeof(current_score_text), "Your Score: %d", g_gs.score);
    lv_label_set_text(game_over_current_score_label, current_score_text);
    lv_obj_set_style_text_font(game_over_current_score_label, &lv_font_montserrat_14, 0);
    lv_obj_align(game_over_current_score_label, LV_ALIGN_TOP_MID, 0, 30);

    // Message label
    game_over_msg_label = lv_label_create(dialog_box);
    lv_label_set_text(game_over_msg_label, "Play Again?");
    lv_obj_set_style_text_font(game_over_msg_label, &lv_font_montserrat_16, 0);
    lv_obj_align(game_over_msg_label, LV_ALIGN_TOP_MID, 0, 55);

    // Calculate symmetric positions for buttons
    lv_coord_t btn_width = 70;
    lv_coord_t btn_height = 30;
    lv_coord_t btn_spacing = 50;

    // Yes button (default selected)
    game_over_yes_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(game_over_yes_btn, btn_width, btn_height);
    lv_obj_align(game_over_yes_btn, LV_ALIGN_BOTTOM_MID, -btn_spacing, -5);
    lv_obj_set_style_bg_color(game_over_yes_btn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(game_over_yes_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(game_over_yes_btn, 2, 0);
    lv_obj_set_style_border_color(game_over_yes_btn, lv_color_black(), 0);

    lv_obj_t *yes_label = lv_label_create(game_over_yes_btn);
    lv_label_set_text(yes_label, "YES");
    lv_obj_center(yes_label);
    lv_obj_set_style_text_color(yes_label, lv_color_white(), 0);

    // No button
    game_over_no_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(game_over_no_btn, btn_width, btn_height);
    lv_obj_align(game_over_no_btn, LV_ALIGN_BOTTOM_MID, btn_spacing, -5);
    lv_obj_set_style_bg_color(game_over_no_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(game_over_no_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(game_over_no_btn, 2, 0);
    lv_obj_set_style_border_color(game_over_no_btn, lv_color_black(), 0);

    lv_obj_t *no_label = lv_label_create(game_over_no_btn);
    lv_label_set_text(no_label, "NO");
    lv_obj_center(no_label);
    lv_obj_set_style_text_color(no_label, lv_color_black(), 0);
}

/**
 * @brief Hide game over dialog
 */
static void snake_game_hide_game_over_dialog(void)
{
    if (!game_over_dialog) return;

    lv_obj_del(game_over_dialog);
    game_over_dialog = NULL;
    game_over_high_score_label = NULL;
    game_over_current_score_label = NULL;
    game_over_msg_label = NULL;
    game_over_yes_btn = NULL;
    game_over_no_btn = NULL;

    g_gs.show_game_over_dialog = 0;
}

/**
 * @brief Update exit dialog selection visual
 */
static void snake_game_update_exit_selection(void)
{
    if (!exit_dialog || !exit_yes_btn || !exit_no_btn) return;

    // Get label objects for text color changes
    lv_obj_t *no_label = lv_obj_get_child(exit_no_btn, 0);
    lv_obj_t *yes_label = lv_obj_get_child(exit_yes_btn, 0);

    if (g_gs.exit_selection == 0) {  // No selected
        // NO button: Black background, white text (selected)
        lv_obj_set_style_bg_color(exit_no_btn, lv_color_black(), 0);
        if (no_label) {
            lv_obj_set_style_text_color(no_label, lv_color_white(), 0);
        }

        // YES button: White background, black text (not selected)
        lv_obj_set_style_bg_color(exit_yes_btn, lv_color_white(), 0);
        if (yes_label) {
            lv_obj_set_style_text_color(yes_label, lv_color_black(), 0);
        }
    } else {  // Yes selected
        // NO button: White background, black text (not selected)
        lv_obj_set_style_bg_color(exit_no_btn, lv_color_white(), 0);
        if (no_label) {
            lv_obj_set_style_text_color(no_label, lv_color_black(), 0);
        }

        // YES button: Black background, white text (selected)
        lv_obj_set_style_bg_color(exit_yes_btn, lv_color_black(), 0);
        if (yes_label) {
            lv_obj_set_style_text_color(yes_label, lv_color_white(), 0);
        }
    }
}

/**
 * @brief Update game over dialog selection visual
 */
static void snake_game_update_game_over_selection(void)
{
    if (!game_over_dialog || !game_over_yes_btn || !game_over_no_btn) return;

    // Get label objects for text color changes
    lv_obj_t *yes_label = lv_obj_get_child(game_over_yes_btn, 0);
    lv_obj_t *no_label = lv_obj_get_child(game_over_no_btn, 0);

    if (g_gs.game_over_selection == 0) {  // Yes selected (play again)
        // YES button: Black background, white text (selected)
        lv_obj_set_style_bg_color(game_over_yes_btn, lv_color_black(), 0);
        if (yes_label) {
            lv_obj_set_style_text_color(yes_label, lv_color_white(), 0);
        }

        // NO button: White background, black text (not selected)
        lv_obj_set_style_bg_color(game_over_no_btn, lv_color_white(), 0);
        if (no_label) {
            lv_obj_set_style_text_color(no_label, lv_color_black(), 0);
        }
    } else {  // No selected (exit)
        // YES button: White background, black text (not selected)
        lv_obj_set_style_bg_color(game_over_yes_btn, lv_color_white(), 0);
        if (yes_label) {
            lv_obj_set_style_text_color(yes_label, lv_color_black(), 0);
        }

        // NO button: Black background, white text (selected)
        lv_obj_set_style_bg_color(game_over_no_btn, lv_color_black(), 0);
        if (no_label) {
            lv_obj_set_style_text_color(no_label, lv_color_white(), 0);
        }
    }
}

/**
 * @brief Keyboard event callback
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", snake_game_screen.name, key);

    // Handle exit dialog
    if (g_gs.show_exit_dialog) {
        if (key == KEY_LEFT || key == KEY_RIGHT) {
            g_gs.exit_selection = 1 - g_gs.exit_selection;
            snake_game_update_exit_selection();
        } else if (key == KEY_ENTER) {
            if (g_gs.exit_selection == 1) {  // Yes selected
                screen_back();
            } else {  // No selected
                snake_game_hide_exit_dialog();
            }
        } else if (key == KEY_ESC) {
            snake_game_hide_exit_dialog();
        }
        return;
    }

    // Handle game over dialog
    if (g_gs.show_game_over_dialog) {
        if (key == KEY_LEFT || key == KEY_RIGHT) {
            g_gs.game_over_selection = 1 - g_gs.game_over_selection;
            snake_game_update_game_over_selection();
        } else if (key == KEY_ENTER) {
            if (g_gs.game_over_selection == 0) {  // Yes (play again)
                snake_game_restart();
            } else {  // No (exit)
                screen_back();
            }
        }
        return;
    }

    // Handle game controls
    if (!g_gs.game_over) {
        switch (key) {
            case KEY_UP:
                if (g_gs.direction != SNAKE_DIR_DOWN) {
                    g_gs.next_direction = SNAKE_DIR_UP;
                }
                break;
            case KEY_DOWN:
                if (g_gs.direction != SNAKE_DIR_UP) {
                    g_gs.next_direction = SNAKE_DIR_DOWN;
                }
                break;
            case KEY_LEFT:
                if (g_gs.direction != SNAKE_DIR_RIGHT) {
                    g_gs.next_direction = SNAKE_DIR_LEFT;
                }
                break;
            case KEY_RIGHT:
                if (g_gs.direction != SNAKE_DIR_LEFT) {
                    g_gs.next_direction = SNAKE_DIR_RIGHT;
                }
                break;
            case KEY_ESC:
                snake_game_show_exit_dialog();
                break;
            default:
                break;
        }
    } else {
        if (key == 'r' || key == 'R') {
            snake_game_restart();
        } else if (key == KEY_ESC) {
            screen_back();
        }
    }
}

/**
 * @brief Initialize the snake game screen
 */
void snake_game_screen_init(void)
{
    ui_snake_game_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_snake_game_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_snake_game_screen, lv_color_white(), 0);

    // Load high score from KV storage
    snake_game_load_high_score();

    // Initialize game state
    memset(&g_gs, 0, sizeof(snake_game_state_t));
    g_gs.length = 3;
    g_gs.direction = SNAKE_DIR_RIGHT;
    g_gs.next_direction = SNAKE_DIR_RIGHT;
    g_gs.speed = 1;
    g_lfsr_state = LFSR_SEED ^ (lv_tick_get() & 0xFFFF);

    // Initialize snake body
    g_gs.body[0].x = SNAKE_INITIAL_X;
    g_gs.body[0].y = SNAKE_INITIAL_Y;
    g_gs.body[1].x = SNAKE_INITIAL_X - 1;
    g_gs.body[1].y = SNAKE_INITIAL_Y;
    g_gs.body[2].x = SNAKE_INITIAL_X - 2;
    g_gs.body[2].y = SNAKE_INITIAL_Y;

    // Score label
    score_label = lv_label_create(ui_snake_game_screen);
    lv_label_set_text(score_label, "SCORE: 0");
    lv_obj_align(score_label, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_style_text_font(score_label, &lv_font_montserrat_14, 0);

    // Game canvas
    game_canvas = lv_obj_create(ui_snake_game_screen);
    lv_obj_set_size(game_canvas, SNAKE_GAME_WIDTH, SNAKE_GAME_HEIGHT);
    lv_obj_align(game_canvas, LV_ALIGN_CENTER, 0, 2);
    lv_obj_set_style_border_width(game_canvas, 0, 0);
    lv_obj_set_style_pad_all(game_canvas, 0, 0);
    lv_obj_set_style_bg_color(game_canvas, lv_color_make(0xF5, 0xF5, 0xF5), 0);

    // Create visual border
    lv_obj_t *border = lv_obj_create(ui_snake_game_screen);
    lv_obj_set_size(border, SNAKE_GAME_WIDTH + 4, SNAKE_GAME_HEIGHT + 4);
    lv_obj_align(border, LV_ALIGN_CENTER, 0, 2);
    lv_obj_set_style_border_width(border, 2, 0);
    lv_obj_set_style_border_color(border, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(border, LV_OPA_TRANSP, 0);
    lv_obj_move_to_index(border, 0);

    // Initialize object pool
    for (uint16_t i = 0; i < SNAKE_MAX_LENGTH; i++) {
        snake_segments[i] = NULL;
    }
    g_last_drawn_length = 0;

    // Generate initial food and draw game
    snake_game_generate_food();
    snake_game_draw_snake();
    snake_game_draw_food();

    // Start game timer
    game_timer = lv_timer_create(game_timer_cb, SNAKE_TIMER_PERIOD, NULL);
    g_gs.initialized = 1;

    // Event handling
    lv_obj_add_event_cb(ui_snake_game_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_snake_game_screen);
    lv_group_focus_obj(ui_snake_game_screen);
}

/**
 * @brief Deinitialize the snake game screen
 */
void snake_game_screen_deinit(void)
{
    if (ui_snake_game_screen) {
        printf("deinit snake game screen\n");
        lv_obj_remove_event_cb(ui_snake_game_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_snake_game_screen);
    }
    if (game_timer) {
        lv_timer_del(game_timer);
        game_timer = NULL;
    }

    // Clean up dialogs
    if (exit_dialog) {
        lv_obj_del(exit_dialog);
        exit_dialog = NULL;
    }
    if (game_over_dialog) {
        lv_obj_del(game_over_dialog);
        game_over_dialog = NULL;
    }

    // Reset pointers
    game_canvas = NULL;
    score_label = NULL;
    food_obj = NULL;
    exit_msg_label = NULL;
    exit_yes_btn = NULL;
    exit_no_btn = NULL;
    game_over_high_score_label = NULL;
    game_over_current_score_label = NULL;
    game_over_msg_label = NULL;
    game_over_yes_btn = NULL;
    game_over_no_btn = NULL;

    for (uint16_t i = 0; i < SNAKE_MAX_LENGTH; i++) {
        snake_segments[i] = NULL;
    }
    g_last_drawn_length = 0;
}
