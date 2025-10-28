/**
 * @file dino_game_screen.c
 * @brief Implementation of the dino game screen for the application
 *
 * This file contains the implementation of the dino game screen which provides
 * a Chrome-style dinosaur jumping game with obstacles, scoring, and physics.
 *
 * The dino game screen includes:
 * - Game physics and collision detection
 * - Obstacle generation and movement
 * - Score tracking and game over handling
 * - Keyboard event handling for game controls
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "dino_game_screen.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

// Declare the GIF image
LV_IMG_DECLARE(ducky_game);

/***********************************************************
************************macro define************************
***********************************************************/

// Game physics constants
#define DINO_JUMP_VY   13  /* positive = upward velocity magnitude */
#define DINO_GRAVITY    1  /* per-tick gravity (subtracts from vy) */
#define DINO_MAX_FALL_VY 20 /* maximum falling velocity */
#define DINO_MOVE_SPEED  3  /* horizontal movement speed when keys pressed */
#define DINO_AIR_CONTROL 0.8f /* air control factor */
#define DINO_FRICTION   0.85f /* ground friction factor */
#define HIGH_SCORE      100   /* historical high score */

// Simple LFSR random number generator
#define LFSR_SEED           0x1234
#define LFSR_POLYNOMIAL     0x8016

// Screen dimensions - use AI_PET constants
#define AI_PET_SCREEN_WIDTH  384
#define AI_PET_SCREEN_HEIGHT 168

// Hardware abstraction
#ifdef ENABLE_LVGL_HARDWARE
#include "board_bmi270_api.h"
#endif

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_dino_game_screen;
static lv_timer_t *game_timer;

// Game objects
static lv_obj_t *g_dino = NULL;
static lv_obj_t *g_obstacle = NULL;
static lv_obj_t *g_air_obstacle = NULL;
static lv_obj_t *g_score_label = NULL;

// Game state structure
typedef struct {
    // Physics properties
    float dino_vy;       /* Vertical velocity */
    float dino_y;        /* Vertical position */
    float dino_vx;       /* Horizontal velocity */
    float dino_x;        /* Horizontal position */

    // Obstacle positions
    int16_t obstacle_x;      /* Horizontal position of ground obstacle */
    int16_t air_obstacle_x;  /* Horizontal position of air obstacle */

    // Game counters
    uint16_t score;          /* Game score */
    uint8_t speed;           /* Game speed */

    // Boolean flags
    uint8_t on_ground : 1;           /* 1 when on ground, 0 when in air */
    uint8_t obstacle_type : 1;       /* 0 = ground obstacle, 1 = air obstacle */
    uint8_t game_over : 1;           /* 1 when game over */
    uint8_t initialized : 1;         /* 1 when game is fully initialized */
    uint8_t paused : 1;              /* 1 when game is paused */
    uint8_t show_exit_dialog : 1;    /* 1 when exit confirmation dialog is shown */
    uint8_t exit_selection : 1;      /* 0 = No, 1 = Yes */
    uint8_t show_game_over_dialog : 1; /* 1 when game over dialog is shown */
    uint8_t game_over_selection : 1;     /* 0 = Yes (play again), 1 = No (exit) */
} dino_game_state_t;

static dino_game_state_t g_gs;
static uint16_t g_lfsr_state = LFSR_SEED;

// Dialog UI elements
static lv_obj_t *g_exit_dialog = NULL;
static lv_obj_t *g_exit_msg_label = NULL;
static lv_obj_t *g_exit_yes_btn = NULL;
static lv_obj_t *g_exit_no_btn = NULL;

// Game over dialog UI elements
static lv_obj_t *g_game_over_dialog = NULL;
// static lv_obj_t *g_game_over_high_score_label = NULL;
// static lv_obj_t *g_game_over_current_score_label = NULL;
static lv_obj_t *g_game_over_msg_label = NULL;
static lv_obj_t *g_game_over_score_label = NULL;
static lv_obj_t *g_game_over_restart_btn = NULL;
static lv_obj_t *g_game_over_exit_btn = NULL;
// static lv_obj_t *g_game_over_yes_btn = NULL;
// static lv_obj_t *g_game_over_no_btn = NULL;

Screen_t dino_game_screen = {
    .init = dino_game_screen_init,
    .deinit = dino_game_screen_deinit,
    .screen_obj = &ui_dino_game_screen,
    .name = "dino_game",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void dino_game_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);
static void dino_game_restart(void);
static void dino_game_show_exit_dialog(void);
static void dino_game_hide_exit_dialog(void);
static void dino_game_show_game_over_dialog(void);
static void dino_game_hide_game_over_dialog(void);
static void dino_game_update_exit_selection(void);
static void dino_game_update_game_over_selection(void);
static inline uint16_t dino_game_lfsr_random(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief LFSR random number generator for embedded systems
 */
static inline uint16_t dino_game_lfsr_random(void)
{
    g_lfsr_state = (g_lfsr_state >> 1) ^ (-(g_lfsr_state & 1u) & LFSR_POLYNOMIAL);
    return g_lfsr_state;
}

/**
 * @brief Game timer callback for physics and rendering updates
 */
static void dino_game_timer_cb(lv_timer_t *timer)
{
    if (g_gs.paused || g_gs.game_over || !g_gs.initialized) {
        return;
    }

    // Apply friction to horizontal movement
    if (g_gs.on_ground) {
        // On ground: apply stronger friction
        g_gs.dino_vx *= DINO_FRICTION;
        // Stop tiny movements to prevent jitter
        if (fabs(g_gs.dino_vx) < 0.1f) {
            g_gs.dino_vx = 0.0f;
        }
    } else {
        // In air: apply lighter friction
        g_gs.dino_vx *= (DINO_FRICTION + 0.1f);
    }

    // Update horizontal position
    g_gs.dino_x += g_gs.dino_vx;

    // Clamp horizontal position to screen bounds
    lv_coord_t screen_width = lv_obj_get_width(ui_dino_game_screen);
    lv_coord_t dino_width = lv_obj_get_width(g_dino);
    if (g_gs.dino_x < 0) {
        g_gs.dino_x = 0;
        g_gs.dino_vx = 0;
    }
    if (g_gs.dino_x > screen_width - dino_width) {
        g_gs.dino_x = screen_width - dino_width;
        g_gs.dino_vx = 0;
    }

    // Vertical physics (jumping and gravity)
    if (!g_gs.on_ground) {
        // Apply gravity
        g_gs.dino_vy -= DINO_GRAVITY;

        // Limit maximum falling speed
        if (g_gs.dino_vy < -DINO_MAX_FALL_VY) {
            g_gs.dino_vy = -DINO_MAX_FALL_VY;
        }

        // Update vertical position
        g_gs.dino_y += g_gs.dino_vy;

        // Check if landed
        if (g_gs.dino_y <= 0) {
            g_gs.dino_y = 0;
            g_gs.dino_vy = 0;
            g_gs.on_ground = 1;
        }

        // Update dino visual position
        lv_coord_t base_y = lv_obj_get_height(ui_dino_game_screen) - 30;
        lv_coord_t dino_height = lv_obj_get_height(g_dino);
        lv_obj_set_y(g_dino, base_y - dino_height - (lv_coord_t)g_gs.dino_y);
    }

    // Update dino horizontal visual position
    lv_obj_set_x(g_dino, (lv_coord_t)g_gs.dino_x);

    // Handle obstacle movement based on current obstacle type
    if (g_gs.obstacle_type == 0) {
        // Ground obstacle is active
        g_gs.obstacle_x -= g_gs.speed;
        lv_obj_set_x(g_obstacle, g_gs.obstacle_x);

        if (g_gs.obstacle_x < -40) {
            // Reset obstacle and randomly choose next obstacle type
            g_gs.obstacle_type = dino_game_lfsr_random() % 2;  // 0 = ground, 1 = air

            if (g_gs.obstacle_type == 0) {
                // Next is ground obstacle
                g_gs.obstacle_x = lv_obj_get_width(ui_dino_game_screen) + 50 + (dino_game_lfsr_random() % 100);
                lv_obj_set_x(g_obstacle, g_gs.obstacle_x);
                lv_obj_clear_flag(g_obstacle, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(g_air_obstacle, LV_OBJ_FLAG_HIDDEN);
            } else {
                // Next is air obstacle
                g_gs.air_obstacle_x = lv_obj_get_width(ui_dino_game_screen) + 50 + (dino_game_lfsr_random() % 100);
                lv_obj_set_x(g_air_obstacle, g_gs.air_obstacle_x);
                lv_obj_add_flag(g_obstacle, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(g_air_obstacle, LV_OBJ_FLAG_HIDDEN);
            }

            g_gs.score += 1;
            if (g_gs.score % 2 == 0 && g_gs.speed < 30) g_gs.speed += 1;
        }
    } else {
        // Air obstacle is active
        g_gs.air_obstacle_x -= g_gs.speed;
        lv_obj_set_x(g_air_obstacle, g_gs.air_obstacle_x);

        if (g_gs.air_obstacle_x < -40) {
            // Reset obstacle and randomly choose next obstacle type
            g_gs.obstacle_type = dino_game_lfsr_random() % 2;  // 0 = ground, 1 = air

            if (g_gs.obstacle_type == 0) {
                // Next is ground obstacle
                g_gs.obstacle_x = lv_obj_get_width(ui_dino_game_screen) + 50 + (dino_game_lfsr_random() % 100);
                lv_obj_set_x(g_obstacle, g_gs.obstacle_x);
                lv_obj_clear_flag(g_obstacle, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(g_air_obstacle, LV_OBJ_FLAG_HIDDEN);
            } else {
                // Next is air obstacle
                g_gs.air_obstacle_x = lv_obj_get_width(ui_dino_game_screen) + 50 + (dino_game_lfsr_random() % 100);
                lv_obj_set_x(g_air_obstacle, g_gs.air_obstacle_x);
                lv_obj_add_flag(g_obstacle, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(g_air_obstacle, LV_OBJ_FLAG_HIDDEN);
            }

            g_gs.score += 1;
            if (g_gs.score % 2 == 0 && g_gs.speed < 30) g_gs.speed += 1;
        }
    }

    // Collision detection
    lv_area_t dino_coords;
    lv_area_t obs_coords;
    lv_obj_get_coords(g_dino, &dino_coords);

    // Check collision with active obstacle
    lv_obj_t *active_obstacle = (g_gs.obstacle_type == 0) ? g_obstacle : g_air_obstacle;
    lv_obj_get_coords(active_obstacle, &obs_coords);

    // Add a small buffer to prevent overly sensitive collision detection
    const int collision_buffer = 2;
    if (!(dino_coords.x2 < obs_coords.x1 + collision_buffer ||
          dino_coords.x1 > obs_coords.x2 - collision_buffer ||
          dino_coords.y2 < obs_coords.y1 + collision_buffer ||
          dino_coords.y1 > obs_coords.y2 - collision_buffer)) {
        g_gs.game_over = 1;
        dino_game_show_game_over_dialog();
    }

    // Update score display
    if (g_score_label) {
        char score_text[32];
        snprintf(score_text, sizeof(score_text), "SCORE: %d", g_gs.score);
        lv_label_set_text(g_score_label, score_text);
    }
}

/**
 * @brief Keyboard event callback
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", dino_game_screen.name, key);

    // Handle exit dialog
    if (g_gs.show_exit_dialog) {
        if (key == KEY_LEFT || key == KEY_RIGHT) {
            g_gs.exit_selection = 1 - g_gs.exit_selection;
            dino_game_update_exit_selection();
        } else if (key == KEY_ENTER) {
            if (g_gs.exit_selection == 1) {  // Yes selected
                screen_back();
            } else {  // No selected
                dino_game_hide_exit_dialog();
            }
        } else if (key == KEY_ESC) {
            dino_game_hide_exit_dialog();
        }
        return;
    }

    // Handle game over dialog
    if (g_gs.show_game_over_dialog) {
        if (key == KEY_LEFT || key == KEY_RIGHT) {
            g_gs.game_over_selection = 1 - g_gs.game_over_selection;
            dino_game_update_game_over_selection();
        } else if (key == KEY_ENTER) {
            if (g_gs.game_over_selection == 0) {  // Yes (play again)
                dino_game_restart();
            } else {  // No (exit)
                screen_back();
            }
        }
        return;
    }

    // Handle game controls
    switch (key) {
        case KEY_UP:
        case ' ':  // Space key
            if (g_gs.on_ground && !g_gs.game_over) {
                g_gs.dino_vy = DINO_JUMP_VY;
                g_gs.on_ground = 0;
            }
            break;
        case KEY_LEFT:
            if (!g_gs.game_over) {
                g_gs.dino_vx -= DINO_MOVE_SPEED * (g_gs.on_ground ? 1.0f : DINO_AIR_CONTROL);
            }
            break;
        case KEY_RIGHT:
            if (!g_gs.game_over) {
                g_gs.dino_vx += DINO_MOVE_SPEED * (g_gs.on_ground ? 1.0f : DINO_AIR_CONTROL);
            }
            break;
        case KEY_ESC:
            dino_game_show_exit_dialog();
            break;
        case 'r':
        case 'R':
            if (g_gs.game_over) {
                dino_game_restart();
            }
            break;
        default:
            break;
    }
}

/**
 * @brief Restart the game
 */
static void dino_game_restart(void)
{
    g_gs.game_over = 0;
    g_gs.paused = 0;
    g_gs.on_ground = 1;
    g_gs.dino_vy = 0.0f;
    g_gs.dino_y = 0.0f;
    g_gs.dino_vx = 0.0f;
    g_gs.dino_x = 20.0f;
    g_gs.obstacle_x = AI_PET_SCREEN_WIDTH + 50;
    g_gs.air_obstacle_x = AI_PET_SCREEN_WIDTH + 50;
    g_gs.obstacle_type = 0;
    g_gs.speed = 4;
    g_gs.score = 0;
    g_gs.show_game_over_dialog = 0;
    g_gs.game_over_selection = 0;

    dino_game_hide_game_over_dialog();
}

/**
 * @brief Show exit confirmation dialog
 */
static void dino_game_show_exit_dialog(void)
{
    if (g_exit_dialog) return;

    g_gs.paused = 1;
    g_gs.show_exit_dialog = 1;
    g_gs.exit_selection = 0;  // Default to "No"

    // Create modal dialog background
    g_exit_dialog = lv_obj_create(ui_dino_game_screen);
    lv_obj_set_size(g_exit_dialog, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(g_exit_dialog, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_exit_dialog, LV_OPA_70, 0);
    lv_obj_set_pos(g_exit_dialog, 0, 0);

    // Create dialog box
    lv_obj_t *dialog_box = lv_obj_create(g_exit_dialog);
    lv_obj_set_size(dialog_box, 200, 120);
    lv_obj_center(dialog_box);
    lv_obj_set_style_bg_color(dialog_box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(dialog_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dialog_box, 2, 0);
    lv_obj_set_style_border_color(dialog_box, lv_color_black(), 0);

    // Message label
    g_exit_msg_label = lv_label_create(dialog_box);
    lv_label_set_text(g_exit_msg_label, "Exit Game?");
    lv_obj_set_style_text_font(g_exit_msg_label, &lv_font_montserrat_16, 0);
    lv_obj_align(g_exit_msg_label, LV_ALIGN_TOP_MID, 0, 15);

    // Calculate symmetric positions for buttons
    lv_coord_t btn_width = 70;
    lv_coord_t btn_height = 30;
    lv_coord_t btn_spacing = 50;  // Space between buttons (center to center)

    // No button (default selected) - positioned to the left of center
    g_exit_no_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(g_exit_no_btn, btn_width, btn_height);
    lv_obj_align(g_exit_no_btn, LV_ALIGN_BOTTOM_MID, -btn_spacing, -5);
    lv_obj_set_style_bg_color(g_exit_no_btn, lv_color_black(), 0);  // Black background (selected)
    lv_obj_set_style_bg_opa(g_exit_no_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_exit_no_btn, 2, 0);
    lv_obj_set_style_border_color(g_exit_no_btn, lv_color_black(), 0);

    lv_obj_t *no_label = lv_label_create(g_exit_no_btn);
    lv_label_set_text(no_label, "NO");
    lv_obj_center(no_label);
    lv_obj_set_style_text_color(no_label, lv_color_white(), 0);  // White text on black background

    // Yes button - positioned to the right of center
    g_exit_yes_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(g_exit_yes_btn, btn_width, btn_height);
    lv_obj_align(g_exit_yes_btn, LV_ALIGN_BOTTOM_MID, btn_spacing, -5);
    lv_obj_set_style_bg_color(g_exit_yes_btn, lv_color_white(), 0);  // White background (not selected)
    lv_obj_set_style_bg_opa(g_exit_yes_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_exit_yes_btn, 2, 0);
    lv_obj_set_style_border_color(g_exit_yes_btn, lv_color_black(), 0);

    lv_obj_t *yes_label = lv_label_create(g_exit_yes_btn);
    lv_label_set_text(yes_label, "YES");
    lv_obj_center(yes_label);
    lv_obj_set_style_text_color(yes_label, lv_color_black(), 0);  // Black text on white background
}

/**
 * @brief Hide exit dialog
 */
static void dino_game_hide_exit_dialog(void)
{
    if (g_exit_dialog) {
        lv_obj_del(g_exit_dialog);
        g_exit_dialog = NULL;
    }
    g_gs.show_exit_dialog = 0;
    g_gs.paused = 0;
}

/**
 * @brief Show game over dialog
 */
static void dino_game_show_game_over_dialog(void)
{
    if (g_game_over_dialog) return;

    g_gs.paused = 1;
    g_gs.show_game_over_dialog = 1;
    g_gs.game_over_selection = 0;  // Default to "Restart"

    // Create modal dialog background
    g_game_over_dialog = lv_obj_create(ui_dino_game_screen);
    lv_obj_set_size(g_game_over_dialog, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(g_game_over_dialog, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_game_over_dialog, LV_OPA_70, 0);
    lv_obj_set_pos(g_game_over_dialog, 0, 0);

    // Create dialog box
    lv_obj_t *dialog_box = lv_obj_create(g_game_over_dialog);
    lv_obj_set_size(dialog_box, 220, 150);
    lv_obj_center(dialog_box);
    lv_obj_set_style_bg_color(dialog_box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(dialog_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dialog_box, 2, 0);
    lv_obj_set_style_border_color(dialog_box, lv_color_black(), 0);

    // Game Over label
    g_game_over_msg_label = lv_label_create(dialog_box);
    lv_label_set_text(g_game_over_msg_label, "Game Over!");
    lv_obj_set_style_text_font(g_game_over_msg_label, &lv_font_montserrat_16, 0);
    lv_obj_align(g_game_over_msg_label, LV_ALIGN_TOP_MID, 0, 10);

    // Score label
    g_game_over_score_label = lv_label_create(dialog_box);
    char score_text[32];
    snprintf(score_text, sizeof(score_text), "Score: %u", g_gs.score);
    lv_label_set_text(g_game_over_score_label, score_text);
    lv_obj_set_style_text_font(g_game_over_score_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_game_over_score_label, LV_ALIGN_TOP_MID, 0, 40);

    // Calculate symmetric positions for buttons
    lv_coord_t btn_width = 80;
    lv_coord_t btn_height = 30;
    lv_coord_t btn_spacing = 55;  // Space between buttons (center to center)

    // Restart button (default selected) - positioned to the left of center
    g_game_over_restart_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(g_game_over_restart_btn, btn_width, btn_height);
    lv_obj_align(g_game_over_restart_btn, LV_ALIGN_BOTTOM_MID, -btn_spacing, -5);
    lv_obj_set_style_bg_color(g_game_over_restart_btn, lv_color_black(), 0);  // Black background (selected)
    lv_obj_set_style_bg_opa(g_game_over_restart_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_game_over_restart_btn, 2, 0);
    lv_obj_set_style_border_color(g_game_over_restart_btn, lv_color_black(), 0);

    lv_obj_t *restart_label = lv_label_create(g_game_over_restart_btn);
    lv_label_set_text(restart_label, "Restart");
    lv_obj_center(restart_label);
    lv_obj_set_style_text_color(restart_label, lv_color_white(), 0);  // White text on black background

    // Exit button - positioned to the right of center
    g_game_over_exit_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(g_game_over_exit_btn, btn_width, btn_height);
    lv_obj_align(g_game_over_exit_btn, LV_ALIGN_BOTTOM_MID, btn_spacing, -5);
    lv_obj_set_style_bg_color(g_game_over_exit_btn, lv_color_white(), 0);  // White background (not selected)
    lv_obj_set_style_bg_opa(g_game_over_exit_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_game_over_exit_btn, 2, 0);
    lv_obj_set_style_border_color(g_game_over_exit_btn, lv_color_black(), 0);

    lv_obj_t *exit_label = lv_label_create(g_game_over_exit_btn);
    lv_label_set_text(exit_label, "Exit");
    lv_obj_center(exit_label);
    lv_obj_set_style_text_color(exit_label, lv_color_black(), 0);  // Black text on white background
}

/**
 * @brief Hide game over dialog
 */
static void dino_game_hide_game_over_dialog(void)
{
    if (g_game_over_dialog) {
        lv_obj_del(g_game_over_dialog);
        g_game_over_dialog = NULL;
    }
    g_gs.show_game_over_dialog = 0;
}

/**
 * @brief Update exit dialog selection visual
 */
static void dino_game_update_exit_selection(void)
{
    if (!g_exit_dialog || !g_exit_yes_btn || !g_exit_no_btn) return;

    if (g_gs.exit_selection == 0) {
        // "No" selected (default)
        lv_obj_set_style_bg_color(g_exit_no_btn, lv_color_black(), 0);
        lv_obj_set_style_bg_color(g_exit_yes_btn, lv_color_white(), 0);
        // Update text colors for labels
        lv_obj_t *no_label = lv_obj_get_child(g_exit_no_btn, 0);
        lv_obj_t *yes_label = lv_obj_get_child(g_exit_yes_btn, 0);
        if (no_label) lv_obj_set_style_text_color(no_label, lv_color_white(), 0);
        if (yes_label) lv_obj_set_style_text_color(yes_label, lv_color_black(), 0);
    } else {
        // "Yes" selected
        lv_obj_set_style_bg_color(g_exit_yes_btn, lv_color_black(), 0);
        lv_obj_set_style_bg_color(g_exit_no_btn, lv_color_white(), 0);
        // Update text colors for labels
        lv_obj_t *no_label = lv_obj_get_child(g_exit_no_btn, 0);
        lv_obj_t *yes_label = lv_obj_get_child(g_exit_yes_btn, 0);
        if (no_label) lv_obj_set_style_text_color(no_label, lv_color_black(), 0);
        if (yes_label) lv_obj_set_style_text_color(yes_label, lv_color_white(), 0);
    }
}

/**
 * @brief Update game over dialog selection visual
 */
static void dino_game_update_game_over_selection(void)
{
    if (!g_game_over_dialog || !g_game_over_restart_btn || !g_game_over_exit_btn) return;

    if (g_gs.game_over_selection == 0) {
        // "Restart" selected (default)
        lv_obj_set_style_bg_color(g_game_over_restart_btn, lv_color_black(), 0);
        lv_obj_set_style_bg_color(g_game_over_exit_btn, lv_color_white(), 0);
        // Update text colors for labels
        lv_obj_t *restart_label = lv_obj_get_child(g_game_over_restart_btn, 0);
        lv_obj_t *exit_label = lv_obj_get_child(g_game_over_exit_btn, 0);
        if (restart_label) lv_obj_set_style_text_color(restart_label, lv_color_white(), 0);
        if (exit_label) lv_obj_set_style_text_color(exit_label, lv_color_black(), 0);
    } else {
        // "Exit" selected
        lv_obj_set_style_bg_color(g_game_over_exit_btn, lv_color_black(), 0);
        lv_obj_set_style_bg_color(g_game_over_restart_btn, lv_color_white(), 0);
        // Update text colors for labels
        lv_obj_t *restart_label = lv_obj_get_child(g_game_over_restart_btn, 0);
        lv_obj_t *exit_label = lv_obj_get_child(g_game_over_exit_btn, 0);
        if (restart_label) lv_obj_set_style_text_color(restart_label, lv_color_black(), 0);
        if (exit_label) lv_obj_set_style_text_color(exit_label, lv_color_white(), 0);
    }
}

/**
 * @brief Initialize the dino game screen
 */
void dino_game_screen_init(void)
{
    ui_dino_game_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_dino_game_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_dino_game_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ui_dino_game_screen, LV_OPA_COVER, 0);

    // Initialize game state
    memset(&g_gs, 0, sizeof(dino_game_state_t));
    g_gs.on_ground = 1;
    g_gs.dino_x = 20.0f;
    g_gs.obstacle_x = AI_PET_SCREEN_WIDTH + 50;
    g_gs.air_obstacle_x = AI_PET_SCREEN_WIDTH + 50;
    g_gs.speed = 4;
    g_lfsr_state = LFSR_SEED ^ (lv_tick_get() & 0xFFFF);

    // Score label
    g_score_label = lv_label_create(ui_dino_game_screen);
    lv_label_set_text(g_score_label, "SCORE: 0");
    lv_obj_align(g_score_label, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_style_text_font(g_score_label, &lv_font_montserrat_14, 0);

    // Ground
    lv_obj_t *ground = lv_obj_create(ui_dino_game_screen);
    lv_obj_set_size(ground, lv_obj_get_width(ui_dino_game_screen), 6);
    lv_obj_set_style_bg_color(ground, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ground, LV_OPA_COVER, 0);
    lv_obj_align(ground, LV_ALIGN_BOTTOM_MID, 0, -30);

    // Create Dino using GIF widget
    g_dino = lv_gif_create(ui_dino_game_screen);
    lv_gif_set_src(g_dino, &ducky_game);
    // Remove clickable flag to prevent event conflicts
    lv_obj_clear_flag(g_dino, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_dino, LV_OBJ_FLAG_SCROLLABLE);

    /* Set size based on screen height */
    lv_coord_t scaled_h = 50;  /* cap at 50px high */
    lv_coord_t scaled_w = 50;  /* maintain aspect ratio */
    lv_obj_set_size(g_dino, scaled_w, scaled_h);

    /* Position the dino */
    lv_obj_set_x(g_dino, (lv_coord_t)g_gs.dino_x);
    // Use the same formula as in the physics engine for consistency
    lv_coord_t base_y = lv_obj_get_height(ui_dino_game_screen) - 30;
    lv_obj_set_y(g_dino, base_y - scaled_h - (lv_coord_t)g_gs.dino_y);

    // Ground Obstacle
    g_obstacle = lv_obj_create(ui_dino_game_screen);
    /* Make obstacles proportional to dino size */
    lv_coord_t obstacle_h = scaled_h * 0.50;  /* 50% of dino height */
    lv_coord_t obstacle_w = scaled_w * 0.25;  /* 25% of dino width */
    lv_obj_set_size(g_obstacle, obstacle_w, obstacle_h);
    lv_obj_set_style_bg_color(g_obstacle, lv_color_black(), 0);

    // Set obstacle to start off-screen to the right
    lv_obj_set_x(g_obstacle, g_gs.obstacle_x);
    lv_obj_set_y(g_obstacle, lv_obj_get_height(ui_dino_game_screen) - 30 - obstacle_h);

    // Air Obstacle (using GIF widget)
    g_air_obstacle = lv_gif_create(ui_dino_game_screen);
    lv_gif_set_src(g_air_obstacle, &ducky_game);
    // Remove clickable flag to prevent event conflicts
    lv_obj_clear_flag(g_air_obstacle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_air_obstacle, LV_OBJ_FLAG_SCROLLABLE);

    // Set air obstacle size (similar to dino size)
    lv_coord_t air_obstacle_h = 40;  /* slightly smaller than dino */
    lv_coord_t air_obstacle_w = 40;  /* maintain aspect ratio */
    lv_obj_set_size(g_air_obstacle, air_obstacle_w, air_obstacle_h);

    // Set air obstacle to start off-screen to the right, positioned in the air
    lv_obj_set_x(g_air_obstacle, g_gs.air_obstacle_x);
    lv_obj_set_y(g_air_obstacle, lv_obj_get_height(ui_dino_game_screen) - 30 - obstacle_h - 90); // 90px above ground obstacle

    // Initially hide air obstacle (will be shown based on obstacle_type)
    lv_obj_add_flag(g_air_obstacle, LV_OBJ_FLAG_HIDDEN);

    // Start game timer
    game_timer = lv_timer_create(dino_game_timer_cb, 20, NULL);
    g_gs.initialized = 1;

    // Event handling
    lv_obj_add_event_cb(ui_dino_game_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_dino_game_screen);
    lv_group_focus_obj(ui_dino_game_screen);
}

/**
 * @brief Deinitialize the dino game screen
 */
void dino_game_screen_deinit(void)
{
    if (ui_dino_game_screen) {
        printf("deinit dino game screen\n");
        lv_obj_remove_event_cb(ui_dino_game_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_dino_game_screen);
    }
    if (game_timer) {
        lv_timer_del(game_timer);
        game_timer = NULL;
    }

    // Clean up dialogs
    if (g_exit_dialog) {
        lv_obj_del(g_exit_dialog);
        g_exit_dialog = NULL;
    }
    if (g_game_over_dialog) {
        lv_obj_del(g_game_over_dialog);
        g_game_over_dialog = NULL;
    }

    // Reset pointers
    g_dino = NULL;
    g_obstacle = NULL;
    g_air_obstacle = NULL;
    g_score_label = NULL;
}
