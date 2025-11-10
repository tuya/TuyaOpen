/**
 * @file snake_game.c
 * Snake Game Component for AI Pocket Pet
 * Optimized for embedded systems with minimal memory footprint
 */

/*********************
 *      INCLUDES
 *********************/
#include "snake_game.h"
#include "ai_pocket_pet_app.h"
#include "stdio.h"
#include "math.h"
#include "lvgl.h"

/*********************
 *      DEFINES
 *********************/

// Snake game constants
#define SNAKE_GRID_SIZE     16    /* Size of each grid cell in pixels - increased to reduce memory usage */
#define SNAKE_GRID_WIDTH    23    /* Number of grid cells horizontally - increased by 1 to allow edge touching */
#define SNAKE_GRID_HEIGHT   8     /* Number of grid cells vertically - increased by 1 to allow edge touching */
#define SNAKE_GAME_WIDTH    (SNAKE_GRID_WIDTH * SNAKE_GRID_SIZE)   /* Game area width - exactly matches grid */
#define SNAKE_GAME_HEIGHT   (SNAKE_GRID_HEIGHT * SNAKE_GRID_SIZE)  /* Game area height - exactly matches grid */
#define SNAKE_MAX_LENGTH    (SNAKE_GRID_WIDTH * SNAKE_GRID_HEIGHT)
#define SNAKE_INITIAL_X     (SNAKE_GRID_WIDTH / 2)
#define SNAKE_INITIAL_Y     (SNAKE_GRID_HEIGHT / 2)
#define SNAKE_TIMER_PERIOD  300   /* Game update interval in ms */
#define HIGH_SCORE          50    /* Historical high score */

// Simple LFSR random number generator for embedded systems
#define LFSR_SEED           0x1234  /* Initial seed for LFSR */
#define LFSR_POLYNOMIAL     0x8016  /* LFSR polynomial (x^16 + x^14 + x^13 + x^11 + 1) */

/**********************
 *      TYPEDEFS
 **********************/
typedef enum {
    SNAKE_DIR_UP = 0,
    SNAKE_DIR_DOWN,
    SNAKE_DIR_LEFT,
    SNAKE_DIR_RIGHT
} snake_direction_t;

typedef struct {
    uint8_t x;  // Max 23, fits in uint8_t
    uint8_t y;  // Max 8, fits in uint8_t
} snake_point_t;

typedef struct {
    // Snake body
    snake_point_t body[SNAKE_MAX_LENGTH];
    uint8_t length;  // Max 184 (23*8), fits in uint8_t
    uint8_t direction : 2;        // Only 4 directions, needs 2 bits
    uint8_t next_direction : 2;   // Only 4 directions, needs 2 bits
    uint8_t game_over : 1;
    uint8_t initialized : 1;
    uint8_t paused : 1;
    uint8_t show_exit_dialog : 1;

    uint8_t exit_selection : 1;      // 0 = No, 1 = Yes
    uint8_t show_game_over_dialog : 1;
    uint8_t game_over_selection : 1; // 0 = Yes (play again), 1 = No (exit)
    uint8_t speed : 4;               // Max 15 levels, fits in 4 bits
    uint8_t _reserved : 1;           // Padding for alignment

    // Food position
    snake_point_t food;

    // Game state
    uint16_t score;  // Keep as uint16_t for higher scores
} snake_game_state_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void snake_game_stop_and_cleanup(void);
static void snake_game_timer_cb(lv_timer_t *tmr);
static void snake_game_event_cb(lv_event_t *e);
static void snake_game_show_exit_dialog(void);
static void snake_game_hide_exit_dialog(void);
static void snake_game_update_exit_selection(void);
static void snake_game_show_game_over_dialog(void);
static void snake_game_hide_game_over_dialog(void);
static void snake_game_update_game_over_selection(void);
static void snake_game_restart(void);
static void snake_game_generate_food(void);
static void snake_game_draw_snake(void);
static void snake_game_draw_food(void);
static uint8_t snake_game_check_collision(void);
static uint8_t snake_game_check_food_collision(void);
static void snake_game_move_snake(void);

// Embedded-friendly random number generation
static inline uint16_t snake_game_lfsr_random(void) __attribute__((always_inline));

// Inline helpers for performance
static inline uint8_t snake_game_is_valid_position(uint8_t x, uint8_t y);

// Hot path functions optimization
static void snake_game_timer_cb(lv_timer_t *tmr);
static void snake_game_move_snake(void);
static uint8_t snake_game_check_collision(void);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t *g_game_screen = NULL;
static lv_obj_t *g_game_canvas = NULL;
static lv_obj_t *g_score_label = NULL;
static lv_timer_t *g_game_timer = NULL;
static snake_game_state_t g_gs;

// Object pool for snake segments and food to avoid frequent create/delete
static lv_obj_t *g_snake_segments[SNAKE_MAX_LENGTH];
static lv_obj_t *g_food_obj = NULL;
static uint16_t g_last_drawn_length = 0;

// LFSR random number state for embedded systems
static uint16_t g_lfsr_state = LFSR_SEED;

// Optimization flags to reduce unnecessary redraws (use struct for bitfields)
static struct {
    uint8_t need_redraw : 1;
    uint8_t _reserved : 7;
} g_redraw_flags = {1, 0};
static uint16_t g_last_score = 0;

// Exit dialog UI elements
static lv_obj_t *g_exit_dialog = NULL;
static lv_obj_t *g_exit_msg_label = NULL;
static lv_obj_t *g_exit_yes_btn = NULL;
static lv_obj_t *g_exit_no_btn = NULL;

// Game over dialog UI elements
static lv_obj_t *g_game_over_dialog = NULL;
static lv_obj_t *g_game_over_high_score_label = NULL;
static lv_obj_t *g_game_over_current_score_label = NULL;
static lv_obj_t *g_game_over_msg_label = NULL;
static lv_obj_t *g_game_over_yes_btn = NULL;
static lv_obj_t *g_game_over_no_btn = NULL;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void snake_game_show(void)
{
    if (g_game_screen) return;  // Already running

    // Reset all static variables to ensure clean state
    g_game_canvas = NULL;
    g_score_label = NULL;
    g_game_timer = NULL;
    g_exit_dialog = NULL;
    g_exit_msg_label = NULL;
    g_exit_yes_btn = NULL;
    g_exit_no_btn = NULL;
    g_game_over_dialog = NULL;
    g_game_over_high_score_label = NULL;
    g_game_over_current_score_label = NULL;
    g_game_over_msg_label = NULL;
    g_game_over_yes_btn = NULL;
    g_game_over_no_btn = NULL;

    // Initialize object pool
    for (uint16_t i = 0; i < SNAKE_MAX_LENGTH; i++) {
        g_snake_segments[i] = NULL;
    }
    g_food_obj = NULL;
    g_last_drawn_length = 0;

    // Initialize optimization flags
    g_redraw_flags.need_redraw = 1;
    g_last_score = 0;

    // Initialize LFSR with a different seed based on game invocation
    g_lfsr_state = LFSR_SEED ^ (lv_tick_get() & 0xFFFF);

    // Initialize game state
    g_gs.length = 3;
    g_gs.direction = SNAKE_DIR_RIGHT;
    g_gs.next_direction = SNAKE_DIR_RIGHT;
    g_gs.score = 0;
    g_gs.speed = 1;
    g_gs.game_over = 0;
    g_gs.initialized = 0;
    g_gs.paused = 0;
    g_gs.show_exit_dialog = 0;
    g_gs.exit_selection = 0;
    g_gs.show_game_over_dialog = 0;
    g_gs.game_over_selection = 0;

    // Initialize snake body
    g_gs.body[0].x = SNAKE_INITIAL_X;
    g_gs.body[0].y = SNAKE_INITIAL_Y;
    g_gs.body[1].x = SNAKE_INITIAL_X - 1;
    g_gs.body[1].y = SNAKE_INITIAL_Y;
    g_gs.body[2].x = SNAKE_INITIAL_X - 2;
    g_gs.body[2].y = SNAKE_INITIAL_Y;

    // Create game screen
    g_game_screen = lv_obj_create(NULL);
    lv_obj_set_size(g_game_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(g_game_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_game_screen, LV_OPA_COVER, 0);

    // Score label
    g_score_label = lv_label_create(g_game_screen);
    lv_label_set_text(g_score_label, "SCORE: 0");
    lv_obj_align(g_score_label, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_style_text_font(g_score_label, &lv_font_montserrat_14, 0);

    // Game canvas - use simple object instead of canvas
    g_game_canvas = lv_obj_create(g_game_screen);
    lv_obj_set_size(g_game_canvas, SNAKE_GAME_WIDTH, SNAKE_GAME_HEIGHT);
    lv_obj_align(g_game_canvas, LV_ALIGN_CENTER, 0, 2);  // Reduced offset for closer edge alignment
    lv_obj_set_style_border_width(g_game_canvas, 0, 0);  // Remove border completely
    lv_obj_set_style_pad_all(g_game_canvas, 0, 0);      // Remove all padding
    lv_obj_set_style_bg_color(g_game_canvas, lv_color_make(0xF5, 0xF5, 0xF5), 0);  // Light gray background
    lv_obj_set_style_bg_opa(g_game_canvas, LV_OPA_COVER, 0);

    // Create a visual border around the game area
    lv_obj_t *border = lv_obj_create(g_game_screen);
    lv_obj_set_size(border, SNAKE_GAME_WIDTH + 4, SNAKE_GAME_HEIGHT + 4);
    lv_obj_align(border, LV_ALIGN_CENTER, 0, 2);
    lv_obj_set_style_border_width(border, 2, 0);
    lv_obj_set_style_border_color(border, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(border, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_move_to_index(border, 0);  // Move border behind canvas

    // Generate initial food
    snake_game_generate_food();

    // Draw initial game state
    snake_game_draw_snake();
    snake_game_draw_food();

    // Events and group
    lv_obj_add_event_cb(g_game_screen, snake_game_event_cb, LV_EVENT_KEY, NULL);
    lv_group_t *grp = lv_group_get_default();
    if (grp) {
        lv_group_add_obj(grp, g_game_screen);
        lv_group_focus_obj(g_game_screen);
    }

    lv_screen_load(g_game_screen);

    // Start timer
    g_game_timer = lv_timer_create(snake_game_timer_cb, SNAKE_TIMER_PERIOD, NULL);

    // Mark game as fully initialized
    g_gs.initialized = 1;
}

int snake_game_is_active(void)
{
    return g_game_screen != NULL ? 1 : 0;
}

void snake_game_key_input(int key)
{
    if (!g_game_screen) return;

    // Handle exit dialog navigation first
    if (g_gs.show_exit_dialog) {
        if (key == KEY_LEFT || key == KEY_RIGHT) {
            g_gs.exit_selection = 1 - g_gs.exit_selection;
            snake_game_update_exit_selection();
        }
        else if (key == KEY_ENTER) {
            if (g_gs.exit_selection == 1) {  // Yes selected
                snake_game_stop_and_cleanup();
            } else {  // No selected
                snake_game_hide_exit_dialog();
            }
        }
        else if (key == KEY_ESC) {
            snake_game_hide_exit_dialog();
        }
        return;
    }

    // Handle game over dialog navigation
    if (g_gs.show_game_over_dialog) {
        if (key == KEY_LEFT || key == KEY_RIGHT) {
            g_gs.game_over_selection = 1 - g_gs.game_over_selection;
            snake_game_update_game_over_selection();
        }
        else if (key == KEY_ENTER) {
            if (g_gs.game_over_selection == 0) {  // Yes selected (play again)
                snake_game_restart();
            } else {  // No selected (exit)
                snake_game_stop_and_cleanup();
            }
        }
        return;
    }

    // Normal game keys - direction control
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
        }
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void snake_game_stop_and_cleanup(void)
{
    // Save reference to main screen before doing any deletion
    lv_obj_t *main_screen = lv_demo_ai_pocket_pet_get_main_screen();
    lv_group_t *group = lv_group_get_default();

    // Delete timer first
    if (g_game_timer) {
        lv_timer_del(g_game_timer);
        g_game_timer = NULL;
    }

    // Delete game screen and all related objects
    if (g_game_screen) {
        // Remove event callbacks
        lv_obj_remove_event_cb(g_game_screen, snake_game_event_cb);

        // Clear references to child objects to avoid dangling pointers
        g_game_canvas = NULL;
        g_score_label = NULL;
        g_exit_dialog = NULL;
        g_exit_msg_label = NULL;
        g_exit_yes_btn = NULL;
        g_exit_no_btn = NULL;
        g_game_over_dialog = NULL;
        g_game_over_high_score_label = NULL;
        g_game_over_current_score_label = NULL;
        g_game_over_msg_label = NULL;
        g_game_over_yes_btn = NULL;
        g_game_over_no_btn = NULL;

        // Delete the screen itself
        lv_obj_del(g_game_screen);
        g_game_screen = NULL;
    }

    // Load main screen after cleanup
    if (main_screen) {
        lv_screen_load(main_screen);
        // Ensure the main screen can receive keyboard focus
        if (group) {
            lv_group_focus_obj(main_screen);
        }
    }
}

static void snake_game_timer_cb(lv_timer_t *tmr)
{
    (void)tmr;
    if (!g_game_screen || g_gs.game_over || !g_gs.initialized || g_gs.paused) return;

    // Update direction
    g_gs.direction = g_gs.next_direction;

    // Move snake
    snake_game_move_snake();
    g_redraw_flags.need_redraw = 1;  // Snake moved, need redraw

    // Check wall collision
    if (snake_game_check_collision()) {
        g_gs.game_over = 1;
        char buf[32];
        snprintf(buf, sizeof(buf), "GAME OVER: %d", g_gs.score);
        lv_label_set_text(g_score_label, buf);
        snake_game_show_game_over_dialog();
        return;
    }

    // Check food collision
    if (snake_game_check_food_collision()) {
        g_gs.score++;
        g_gs.length++;

        // Update score display only if score changed
        if (g_gs.score != g_last_score) {
            char buf[32];
            snprintf(buf, sizeof(buf), "SCORE: %d", g_gs.score);
            lv_label_set_text(g_score_label, buf);
            g_last_score = g_gs.score;
        }
        // Generate new food
        snake_game_generate_food();

        // Increase speed slightly
        if (g_gs.score % 5 == 0 && SNAKE_TIMER_PERIOD > 100) {
            lv_timer_set_period(g_game_timer, SNAKE_TIMER_PERIOD - (g_gs.score / 5) * 20);
        }
    }

    // Only redraw if necessary
    if (g_redraw_flags.need_redraw) {
        snake_game_draw_snake();
        snake_game_draw_food();
        g_redraw_flags.need_redraw = 0;
    }
}

static void snake_game_event_cb(lv_event_t *e)
{
    if (!e) return;

    // Ignore events during initialization
    if (!g_gs.initialized) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY) {
        int key = lv_event_get_key(e);
        snake_game_key_input(key);
    }
}

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

static uint8_t snake_game_check_food_collision(void)
{
    return (g_gs.body[0].x == g_gs.food.x && g_gs.body[0].y == g_gs.food.y);
}

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

static void snake_game_draw_snake(void)
{
    // Initialize snake segments on first call or when length increases
    if (g_gs.length > g_last_drawn_length) {
        for (uint8_t i = g_last_drawn_length; i < g_gs.length; i++) {
            g_snake_segments[i] = lv_obj_create(g_game_canvas);
            lv_obj_set_size(g_snake_segments[i], SNAKE_GRID_SIZE, SNAKE_GRID_SIZE);
            lv_obj_set_style_bg_opa(g_snake_segments[i], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(g_snake_segments[i], 1, 0);
            lv_obj_set_style_border_color(g_snake_segments[i], lv_color_black(), 0);
            lv_obj_set_style_radius(g_snake_segments[i], 2, 0);
        }
        g_last_drawn_length = g_gs.length;
    }

    // Update positions and colors of existing segments
    for (uint8_t i = 0; i < g_gs.length; i++) {
        if (g_snake_segments[i]) {
            // Make sure segment is visible
            lv_obj_clear_flag(g_snake_segments[i], LV_OBJ_FLAG_HIDDEN);

            // Calculate pixel position normally
            lv_coord_t pixel_x = g_gs.body[i].x * SNAKE_GRID_SIZE;
            lv_coord_t pixel_y = g_gs.body[i].y * SNAKE_GRID_SIZE;

            lv_obj_set_pos(g_snake_segments[i], pixel_x, pixel_y);

            // Head is darker, body is lighter
            if (i == 0) {
                lv_obj_set_style_bg_color(g_snake_segments[i], lv_color_black(), 0);
            } else {
                lv_obj_set_style_bg_color(g_snake_segments[i], lv_color_make(0x80, 0x80, 0x80), 0);
            }
        }
    }

    // Hide unused segments if snake got shorter (shouldn't happen in this game, but safety)
    for (int i = g_gs.length; i < g_last_drawn_length; i++) {
        if (g_snake_segments[i]) {
            lv_obj_add_flag(g_snake_segments[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}static void snake_game_draw_food(void)
{
    // Create food object only once
    if (!g_food_obj) {
        g_food_obj = lv_obj_create(g_game_canvas);
        lv_obj_set_size(g_food_obj, SNAKE_GRID_SIZE - 6, SNAKE_GRID_SIZE - 6);  // Smaller size for diamond shape
        lv_obj_set_style_bg_color(g_food_obj, lv_color_black(), 0);  // Black diamond
        lv_obj_set_style_bg_opa(g_food_obj, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(g_food_obj, 0, 0);  // No border for clean look
        lv_obj_set_style_radius(g_food_obj, 2, 0);  // Small radius

        // Transform to diamond shape by rotating 45 degrees
        lv_obj_set_style_transform_angle(g_food_obj, 450, 0);  // 45 degrees in 0.1 degree units
    }

    // Update food position
    lv_obj_set_pos(g_food_obj,
                  g_gs.food.x * SNAKE_GRID_SIZE + 3,
                  g_gs.food.y * SNAKE_GRID_SIZE + 3);
}

static void snake_game_show_exit_dialog(void)
{
    if (g_exit_dialog) return;  // Already shown

    g_gs.paused = 1;
    g_gs.show_exit_dialog = 1;
    g_gs.exit_selection = 0;  // Default to "No"

    // Create modal dialog background
    g_exit_dialog = lv_obj_create(g_game_screen);
    lv_obj_set_size(g_exit_dialog, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(g_exit_dialog, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_exit_dialog, LV_OPA_70, 0);  // Semi-transparent
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
    lv_coord_t btn_spacing = 50;

    // No button (default selected)
    g_exit_no_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(g_exit_no_btn, btn_width, btn_height);
    lv_obj_align(g_exit_no_btn, LV_ALIGN_BOTTOM_MID, -btn_spacing, -5);
    lv_obj_set_style_bg_color(g_exit_no_btn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_exit_no_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_exit_no_btn, 2, 0);
    lv_obj_set_style_border_color(g_exit_no_btn, lv_color_black(), 0);

    lv_obj_t *no_label = lv_label_create(g_exit_no_btn);
    lv_label_set_text(no_label, "NO");
    lv_obj_center(no_label);
    lv_obj_set_style_text_color(no_label, lv_color_white(), 0);

    // Yes button
    g_exit_yes_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(g_exit_yes_btn, btn_width, btn_height);
    lv_obj_align(g_exit_yes_btn, LV_ALIGN_BOTTOM_MID, btn_spacing, -5);
    lv_obj_set_style_bg_color(g_exit_yes_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_exit_yes_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_exit_yes_btn, 2, 0);
    lv_obj_set_style_border_color(g_exit_yes_btn, lv_color_black(), 0);

    lv_obj_t *yes_label = lv_label_create(g_exit_yes_btn);
    lv_label_set_text(yes_label, "YES");
    lv_obj_center(yes_label);
    lv_obj_set_style_text_color(yes_label, lv_color_black(), 0);
}

static void snake_game_hide_exit_dialog(void)
{
    if (!g_exit_dialog) return;

    lv_obj_del(g_exit_dialog);
    g_exit_dialog = NULL;
    g_exit_msg_label = NULL;
    g_exit_yes_btn = NULL;
    g_exit_no_btn = NULL;

    g_gs.paused = 0;
    g_gs.show_exit_dialog = 0;
}

static void snake_game_update_exit_selection(void)
{
    if (!g_exit_dialog || !g_exit_yes_btn || !g_exit_no_btn) return;

    // Get label objects for text color changes
    lv_obj_t *no_label = lv_obj_get_child(g_exit_no_btn, 0);
    lv_obj_t *yes_label = lv_obj_get_child(g_exit_yes_btn, 0);

    if (g_gs.exit_selection == 0) {  // No selected
        // NO button: Black background, white text (selected)
        lv_obj_set_style_bg_color(g_exit_no_btn, lv_color_black(), 0);
        if (no_label) {
            lv_obj_set_style_text_color(no_label, lv_color_white(), 0);
        }

        // YES button: White background, black text (not selected)
        lv_obj_set_style_bg_color(g_exit_yes_btn, lv_color_white(), 0);
        if (yes_label) {
            lv_obj_set_style_text_color(yes_label, lv_color_black(), 0);
        }
    } else {  // Yes selected
        // NO button: White background, black text (not selected)
        lv_obj_set_style_bg_color(g_exit_no_btn, lv_color_white(), 0);
        if (no_label) {
            lv_obj_set_style_text_color(no_label, lv_color_black(), 0);
        }

        // YES button: Black background, white text (selected)
        lv_obj_set_style_bg_color(g_exit_yes_btn, lv_color_black(), 0);
        if (yes_label) {
            lv_obj_set_style_text_color(yes_label, lv_color_white(), 0);
        }
    }
}

static void snake_game_show_game_over_dialog(void)
{
    if (g_game_over_dialog) return;  // Already shown

    g_gs.paused = 1;
    g_gs.show_game_over_dialog = 1;
    g_gs.game_over_selection = 0;  // Default to "Yes" (play again)

    // Create modal dialog background
    g_game_over_dialog = lv_obj_create(g_game_screen);
    lv_obj_set_size(g_game_over_dialog, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(g_game_over_dialog, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_game_over_dialog, LV_OPA_70, 0);
    lv_obj_set_pos(g_game_over_dialog, 0, 0);

    // Create dialog box
    lv_obj_t *dialog_box = lv_obj_create(g_game_over_dialog);
    lv_obj_set_size(dialog_box, 220, 160);
    lv_obj_center(dialog_box);
    lv_obj_set_style_bg_color(dialog_box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(dialog_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dialog_box, 2, 0);
    lv_obj_set_style_border_color(dialog_box, lv_color_black(), 0);

    // High score label
    g_game_over_high_score_label = lv_label_create(dialog_box);
    char high_score_text[32];
    snprintf(high_score_text, sizeof(high_score_text), "Highest Score: %d", HIGH_SCORE);
    lv_label_set_text(g_game_over_high_score_label, high_score_text);
    lv_obj_set_style_text_font(g_game_over_high_score_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_game_over_high_score_label, LV_ALIGN_TOP_MID, 0, 10);

    // Current score label
    g_game_over_current_score_label = lv_label_create(dialog_box);
    char current_score_text[32];
    snprintf(current_score_text, sizeof(current_score_text), "Your Score: %d", g_gs.score);
    lv_label_set_text(g_game_over_current_score_label, current_score_text);
    lv_obj_set_style_text_font(g_game_over_current_score_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_game_over_current_score_label, LV_ALIGN_TOP_MID, 0, 30);

    // Message label
    g_game_over_msg_label = lv_label_create(dialog_box);
    lv_label_set_text(g_game_over_msg_label, "Play Again?");
    lv_obj_set_style_text_font(g_game_over_msg_label, &lv_font_montserrat_16, 0);
    lv_obj_align(g_game_over_msg_label, LV_ALIGN_TOP_MID, 0, 55);

    // Calculate symmetric positions for buttons
    lv_coord_t btn_width = 70;
    lv_coord_t btn_height = 30;
    lv_coord_t btn_spacing = 50;

    // Yes button (default selected)
    g_game_over_yes_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(g_game_over_yes_btn, btn_width, btn_height);
    lv_obj_align(g_game_over_yes_btn, LV_ALIGN_BOTTOM_MID, -btn_spacing, -5);
    lv_obj_set_style_bg_color(g_game_over_yes_btn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_game_over_yes_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_game_over_yes_btn, 2, 0);
    lv_obj_set_style_border_color(g_game_over_yes_btn, lv_color_black(), 0);

    lv_obj_t *yes_label = lv_label_create(g_game_over_yes_btn);
    lv_label_set_text(yes_label, "YES");
    lv_obj_center(yes_label);
    lv_obj_set_style_text_color(yes_label, lv_color_white(), 0);

    // No button
    g_game_over_no_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(g_game_over_no_btn, btn_width, btn_height);
    lv_obj_align(g_game_over_no_btn, LV_ALIGN_BOTTOM_MID, btn_spacing, -5);
    lv_obj_set_style_bg_color(g_game_over_no_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_game_over_no_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_game_over_no_btn, 2, 0);
    lv_obj_set_style_border_color(g_game_over_no_btn, lv_color_black(), 0);

    lv_obj_t *no_label = lv_label_create(g_game_over_no_btn);
    lv_label_set_text(no_label, "NO");
    lv_obj_center(no_label);
    lv_obj_set_style_text_color(no_label, lv_color_black(), 0);
}

static void snake_game_hide_game_over_dialog(void)
{
    if (!g_game_over_dialog) return;

    lv_obj_del(g_game_over_dialog);
    g_game_over_dialog = NULL;
    g_game_over_high_score_label = NULL;
    g_game_over_current_score_label = NULL;
    g_game_over_msg_label = NULL;
    g_game_over_yes_btn = NULL;
    g_game_over_no_btn = NULL;

    g_gs.paused = 0;
    g_gs.show_game_over_dialog = 0;
}

static void snake_game_update_game_over_selection(void)
{
    if (!g_game_over_dialog || !g_game_over_yes_btn || !g_game_over_no_btn) return;

    // Get label objects for text color changes
    lv_obj_t *yes_label = lv_obj_get_child(g_game_over_yes_btn, 0);
    lv_obj_t *no_label = lv_obj_get_child(g_game_over_no_btn, 0);

    if (g_gs.game_over_selection == 0) {  // Yes selected (play again)
        // YES button: Black background, white text (selected)
        lv_obj_set_style_bg_color(g_game_over_yes_btn, lv_color_black(), 0);
        if (yes_label) {
            lv_obj_set_style_text_color(yes_label, lv_color_white(), 0);
        }

        // NO button: White background, black text (not selected)
        lv_obj_set_style_bg_color(g_game_over_no_btn, lv_color_white(), 0);
        if (no_label) {
            lv_obj_set_style_text_color(no_label, lv_color_black(), 0);
        }
    } else {  // No selected (exit)
        // YES button: White background, black text (not selected)
        lv_obj_set_style_bg_color(g_game_over_yes_btn, lv_color_white(), 0);
        if (yes_label) {
            lv_obj_set_style_text_color(yes_label, lv_color_black(), 0);
        }

        // NO button: Black background, white text (selected)
        lv_obj_set_style_bg_color(g_game_over_no_btn, lv_color_black(), 0);
        if (no_label) {
            lv_obj_set_style_text_color(no_label, lv_color_white(), 0);
        }
    }
}

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
    lv_timer_set_period(g_game_timer, SNAKE_TIMER_PERIOD);

    // Generate new food
    snake_game_generate_food();

    // Update score display
    lv_label_set_text(g_score_label, "SCORE: 0");

    // Reset object pool for restart - hide excess snake segments
    uint16_t old_length = g_last_drawn_length;
    g_last_drawn_length = 3;  // Reset to initial snake length

    // Hide any excess snake segments from previous game
    for (uint16_t i = 3; i < old_length; i++) {
        if (g_snake_segments[i]) {
            lv_obj_add_flag(g_snake_segments[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Redraw game using object pool - no clearing needed
    snake_game_draw_snake();
    snake_game_draw_food();
}

/**
 * @brief Simple LFSR (Linear Feedback Shift Register) random number generator
 * Optimized for embedded systems - no dependencies on standard library
 * @return 16-bit pseudo-random number
 */
static inline uint16_t snake_game_lfsr_random(void)
{
    uint8_t bit = ((g_lfsr_state >> 0) ^ (g_lfsr_state >> 2) ^
                   (g_lfsr_state >> 3) ^ (g_lfsr_state >> 5)) & 1;
    g_lfsr_state = (g_lfsr_state >> 1) | (bit << 15);
    return g_lfsr_state;
}

/**
 * @brief Inline helper to validate grid position
 * @param x X coordinate to check
 * @param y Y coordinate to check
 * @return 1 if position is valid, 0 otherwise
 */
static inline uint8_t snake_game_is_valid_position(uint8_t x, uint8_t y)
{
    return (x < SNAKE_GRID_WIDTH && y < SNAKE_GRID_HEIGHT);
}
