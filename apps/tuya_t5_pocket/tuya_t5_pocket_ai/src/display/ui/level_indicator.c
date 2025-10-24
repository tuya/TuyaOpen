/**
 * @file level_indicator.c
 * Level Indicator (Digital Spirit Level) Component for AI Pocket Pet
 * Optimized for embedded systems with smooth animations and precise angle display
 */

/*********************
 *      INCLUDES
 *********************/
#include "level_indicator.h"
#include "ai_pocket_pet_app.h"
#include "keyboard.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "lvgl.h"

// Forward declarations for BMI270 integration
#ifndef LVGL_SIMULATOR
#include "board_bmi270_api.h"
#endif

/*********************
 *      DEFINES
 *********************/

// Mathematical constants
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Animation and timing
#define BALL_MOVE_SMOOTH_FACTOR    0.15f  /* Ball movement smoothing */

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *main_container;
    lv_obj_t *level_circle;
    lv_obj_t *ball;
    lv_obj_t *center_cross;
    lv_obj_t *angle_x_label;
    lv_obj_t *angle_y_label;
    lv_obj_t *exit_dialog;

    // Measurement state
    tilt_data_t current_tilt;
    tilt_data_t calibration_offset;
    float level_threshold;

    // BMI270 sensor support
#ifndef LVGL_SIMULATOR
    void *bmi270_handle;
    uint8_t sensor_available : 1;
#endif
    uint8_t use_real_sensor : 1;

    // UI state
    uint8_t is_active : 1;
    uint8_t show_exit_dialog : 1;
    uint8_t exit_selection : 1;  // 0 = No, 1 = Yes
    uint8_t _reserved : 5;

    // Animation state
    float ball_x_target;
    float ball_y_target;
    float ball_x_current;
    float ball_y_current;

    // Callback system
    level_indicator_callback_t callback;
    void *callback_user_data;

    // Timer for updates
    lv_timer_t *update_timer;
} level_indicator_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void level_indicator_cleanup(void);
static void level_indicator_update_ui(void);
static void level_indicator_update_ball_position(void);
static void level_indicator_update_angle_display(void);
static void level_indicator_create_circle(void);
static void level_indicator_create_controls(void);
static void level_indicator_show_exit_dialog(void);
static void level_indicator_hide_exit_dialog(void);
static void level_indicator_update_exit_selection(void);
static void level_indicator_timer_cb(lv_timer_t *timer) __attribute__((hot));
static void level_indicator_simulate_tilt(void);
static void level_indicator_read_real_sensor(void);
static void level_indicator_init_sensor(void);
static float level_indicator_calculate_tilt_angle(float acc_x, float acc_y, float acc_z, int axis);
static inline float level_indicator_clamp(float value, float min, float max) __attribute__((always_inline));

/**********************
 *  STATIC VARIABLES
 **********************/
static level_indicator_data_t g_level_data = {0};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void level_indicator_show(void)
{

    if (g_level_data.is_active) {
        printf("level_indicator_show: already active, returning\n");
        return;  // Already active
    }

    // Check and cleanup keyboard if active
    if (keyboard_is_active()) {
        printf("level_indicator_show: keyboard was active, cleaning up\n");
        keyboard_cleanup();
    }

    // Reset state
    memset(&g_level_data, 0, sizeof(level_indicator_data_t));
    g_level_data.is_active = 1;
    g_level_data.level_threshold = LEVEL_INDICATOR_LEVEL_THRESHOLD;

    // Initialize sensor
    level_indicator_init_sensor();

    printf("level_indicator_show: is_active set to %d, sensor_available=%d\n", 
           g_level_data.is_active, 
#ifndef LVGL_SIMULATOR
           g_level_data.sensor_available
#else
           0
#endif
    );

    // Create main screen
    g_level_data.screen = lv_obj_create(NULL);
    lv_obj_set_size(g_level_data.screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(g_level_data.screen, lv_color_make(240, 240, 240), 0);
    lv_obj_set_style_bg_opa(g_level_data.screen, LV_OPA_COVER, 0);

    // Create main container
    g_level_data.main_container = lv_obj_create(g_level_data.screen);
    lv_obj_set_size(g_level_data.main_container, AI_PET_SCREEN_WIDTH - 20, AI_PET_SCREEN_HEIGHT - 20);
    lv_obj_center(g_level_data.main_container);
    lv_obj_set_style_bg_opa(g_level_data.main_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_level_data.main_container, 0, 0);
    lv_obj_set_style_pad_all(g_level_data.main_container, 5, 0);

    // Create UI components
    level_indicator_create_circle();
    level_indicator_create_controls();

    // Load the screen
    lv_screen_load(g_level_data.screen);

    // Start update timer
    g_level_data.update_timer = lv_timer_create(level_indicator_timer_cb,
                                              LEVEL_INDICATOR_UPDATE_PERIOD,
                                              &g_level_data);

    // Add to input group for keyboard navigation
    lv_group_t *group = lv_group_get_default();
    if (group) {
        lv_group_add_obj(group, g_level_data.main_container);
        lv_group_focus_obj(g_level_data.main_container);
    }

    // Initial update
    level_indicator_update_ui();
}

void level_indicator_hide(void)
{
    if (!g_level_data.is_active) {
        printf("level_indicator_hide: already inactive\n");
        return;
    }

    // Get main screen before cleanup
    lv_obj_t *main_screen = lv_demo_ai_pocket_pet_get_main_screen();
    if (!main_screen) {
        printf("level_indicator_hide: Error - main screen not found\n");
        return;
    }

    // Load main screen first, before cleanup
    lv_screen_load(main_screen);

    // Now it's safe to cleanup
    level_indicator_cleanup();

    // Set focus on main screen
    lv_group_t *group = lv_group_get_default();
    if (group) {
        lv_group_focus_obj(main_screen);
    }
}

void level_indicator_key_input(int key)
{
    if (!g_level_data.is_active) {
        printf("level_indicator: not active, returning\n");
        return;
    }

    // Handle exit dialog first
    if (g_level_data.show_exit_dialog) {
        if (key == KEY_LEFT || key == KEY_RIGHT) {
            g_level_data.exit_selection = 1 - g_level_data.exit_selection;
            level_indicator_update_exit_selection();
        }
        else if (key == KEY_ENTER) {
            if (g_level_data.exit_selection == 1) {  // Yes selected
                level_indicator_hide();
            } else {  // No selected
                level_indicator_hide_exit_dialog();
            }
        }
        else if (key == KEY_ESC) {
            level_indicator_hide_exit_dialog();
        }
        return;
    }

    // Handle normal navigation
    switch (key) {
        case KEY_ESC:
            // Direct exit to return to main menu
            printf("level_indicator: ESC key detected, calling level_indicator_hide()\n");
            level_indicator_hide();
            printf("level_indicator: level_indicator_hide() completed\n");
            return;  // Ensure we return immediately after hide

        case KEY_ENTER:
        case 'c':
        case 'C':
            level_indicator_calibrate();
            break;

        case 'q':
        case 'Q':
            // Show exit confirmation dialog
            level_indicator_show_exit_dialog();
            break;

        case 's':
        case 'S':
            // Toggle sensor mode (real sensor vs simulation)
#ifndef LVGL_SIMULATOR
            if (g_level_data.sensor_available) {
                g_level_data.use_real_sensor = !g_level_data.use_real_sensor;
                printf("Sensor mode switched to: %s\n", 
                       g_level_data.use_real_sensor ? "Real BMI270" : "Simulation");
            } else {
                printf("BMI270 sensor not available, cannot switch to real sensor mode\n");
            }
#else
            printf("Running in simulator, cannot use real sensor\n");
#endif
            break;

        default:
            // Other keys are ignored
            break;
    }
}

void level_indicator_set_callback(level_indicator_callback_t callback, void *user_data)
{
    g_level_data.callback = callback;
    g_level_data.callback_user_data = user_data;
}

void level_indicator_update_tilt(float x_angle, float y_angle)
{
    if (!g_level_data.is_active) {
        return;
    }

    // Apply calibration offset
    x_angle -= g_level_data.calibration_offset.x_angle;
    y_angle -= g_level_data.calibration_offset.y_angle;

    // Clamp angles to maximum range
    x_angle = level_indicator_clamp(x_angle, -LEVEL_INDICATOR_MAX_ANGLE, LEVEL_INDICATOR_MAX_ANGLE);
    y_angle = level_indicator_clamp(y_angle, -LEVEL_INDICATOR_MAX_ANGLE, LEVEL_INDICATOR_MAX_ANGLE);

    // Update tilt data
    g_level_data.current_tilt.x_angle = x_angle;
    g_level_data.current_tilt.y_angle = y_angle;
    g_level_data.current_tilt.magnitude = sqrtf(x_angle * x_angle + y_angle * y_angle);

    bool was_level = g_level_data.current_tilt.is_level;
    g_level_data.current_tilt.is_level = (g_level_data.current_tilt.magnitude <= g_level_data.level_threshold);

    // Debug output for final angles (every 75 updates to avoid spam)
    static int tilt_debug_counter = 0;
    if (++tilt_debug_counter % 75 == 0) {
        printf("Tilt Debug: final_angles(X:%.1f°, Y:%.1f°) -> magnitude:%.2f° -> is_level:%s\n",
               g_level_data.current_tilt.x_angle, g_level_data.current_tilt.y_angle,
               g_level_data.current_tilt.magnitude, g_level_data.current_tilt.is_level ? "YES" : "NO");
    }

    // Call callback if set
    if (g_level_data.callback) {
        if (g_level_data.current_tilt.is_level && !was_level) {
            g_level_data.callback(LEVEL_INDICATOR_EVENT_LEVEL_ACHIEVED,
                                &g_level_data.current_tilt,
                                g_level_data.callback_user_data);
        } else if (!g_level_data.current_tilt.is_level && was_level) {
            g_level_data.callback(LEVEL_INDICATOR_EVENT_TILT_DETECTED,
                                &g_level_data.current_tilt,
                                g_level_data.callback_user_data);
        } else {
            g_level_data.callback(LEVEL_INDICATOR_EVENT_ANGLE_CHANGED,
                                &g_level_data.current_tilt,
                                g_level_data.callback_user_data);
        }
    }
}

void level_indicator_calibrate(void)
{
    if (!g_level_data.is_active) {
        return;
    }

    // Read current sensor angles and set them as calibration offset
#ifndef LVGL_SIMULATOR
    if (g_level_data.sensor_available && g_level_data.bmi270_handle) {
        float acc_x, acc_y, acc_z;
        int result = board_bmi270_read_accel(g_level_data.bmi270_handle, &acc_x, &acc_y, &acc_z);
        if (result == 0) {
            // Calculate current raw angles and use them as calibration offset
            g_level_data.calibration_offset.x_angle = level_indicator_calculate_tilt_angle(acc_x, acc_y, acc_z, 0);
            g_level_data.calibration_offset.y_angle = level_indicator_calculate_tilt_angle(acc_x, acc_y, acc_z, 1);
            
            printf("Calibration completed: offset set to X:%.1f°, Y:%.1f°\n", 
                   g_level_data.calibration_offset.x_angle, g_level_data.calibration_offset.y_angle);
        } else {
            printf("Failed to read sensor for calibration\n");
            return;
        }
    } else 
#endif
    {
        // Fallback for simulator: set current offset to make current position become center
        g_level_data.calibration_offset.x_angle = g_level_data.current_tilt.x_angle + g_level_data.calibration_offset.x_angle;
        g_level_data.calibration_offset.y_angle = g_level_data.current_tilt.y_angle + g_level_data.calibration_offset.y_angle;
        printf("Calibration completed (simulator mode): offset set to X:%.1f°, Y:%.1f°\n", 
               g_level_data.calibration_offset.x_angle, g_level_data.calibration_offset.y_angle);
    }

    // Call callback
    if (g_level_data.callback) {
        g_level_data.callback(LEVEL_INDICATOR_EVENT_CALIBRATION,
                            &g_level_data.current_tilt,
                            g_level_data.callback_user_data);
    }
}

const tilt_data_t* level_indicator_get_tilt_data(void)
{
    if (!g_level_data.is_active) {
        return NULL;
    }

    return &g_level_data.current_tilt;
}

bool level_indicator_is_active(void)
{
    static int call_count = 0;
    call_count++;
    printf("level_indicator_is_active() called #%d, returning %d (timer=%p, screen=%p)\n",
           call_count, g_level_data.is_active, g_level_data.update_timer, g_level_data.screen);
    return g_level_data.is_active;
}

void level_indicator_set_threshold(float threshold_degrees)
{
    if (threshold_degrees > 0 && threshold_degrees <= 10.0f) {
        g_level_data.level_threshold = threshold_degrees;
    }
}

float level_indicator_get_threshold(void)
{
    return g_level_data.level_threshold;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void level_indicator_cleanup(void)
{
    // Stop timer
    if (g_level_data.update_timer) {
        lv_timer_del(g_level_data.update_timer);
        g_level_data.update_timer = NULL;
    }

    // Remove from input group
    lv_group_t *group = lv_group_get_default();
    if (group && g_level_data.main_container) {
        lv_group_remove_obj(g_level_data.main_container);
    }

    // Delete screen and all children
    if (g_level_data.screen) {
        lv_obj_del(g_level_data.screen);
    }

    // Reset state
    memset(&g_level_data, 0, sizeof(level_indicator_data_t));
}

static void level_indicator_update_ui(void)
{
    level_indicator_update_ball_position();
    level_indicator_update_angle_display();
}

static void level_indicator_update_ball_position(void)
{
    if (!g_level_data.ball) {
        return;
    }

    // Circle center coordinates (relative to level_circle container)
    // Ball always starts from center and moves based on tilt angles
    float circle_center_x = LEVEL_CIRCLE_RADIUS;
    float circle_center_y = LEVEL_CIRCLE_RADIUS;

    // Convert angles to pixel offsets from center
    // Use the final calibrated angles (current_tilt already has calibration applied)
    // Map angles to movement: X angle controls Y movement, Y angle controls X movement
    float angle_to_pixel_scale = 2.0f;  // Scale factor: degrees to pixels
    float offset_x = g_level_data.current_tilt.y_angle * angle_to_pixel_scale;   // Y angle controls X movement
    float offset_y = -g_level_data.current_tilt.x_angle * angle_to_pixel_scale;  // X angle controls Y movement (inverted)

    // Limit ball movement within circle (considering ball size)
    float max_radius = LEVEL_CIRCLE_RADIUS - LEVEL_BALL_SIZE / 2 - 5; // 5px margin from edge
    float distance = sqrtf(offset_x * offset_x + offset_y * offset_y);
    
    if (distance > max_radius) {
        // Scale down if outside circle
        float scale = max_radius / distance;
        offset_x *= scale;
        offset_y *= scale;
    }

    // Calculate final ball position: center + offset - ball_radius_adjustment
    g_level_data.ball_x_target = circle_center_x + offset_x - LEVEL_BALL_SIZE / 2;
    g_level_data.ball_y_target = circle_center_y + offset_y - LEVEL_BALL_SIZE / 2;

    // Smooth movement
    g_level_data.ball_x_current += (g_level_data.ball_x_target - g_level_data.ball_x_current) * BALL_MOVE_SMOOTH_FACTOR;
    g_level_data.ball_y_current += (g_level_data.ball_y_target - g_level_data.ball_y_current) * BALL_MOVE_SMOOTH_FACTOR;

    // Update ball position
    lv_obj_set_pos(g_level_data.ball,
                   (lv_coord_t)g_level_data.ball_x_current,
                   (lv_coord_t)g_level_data.ball_y_current);

    // Debug output for ball position (every 100 updates to avoid spam)
    static int ball_debug_counter = 0;
    if (++ball_debug_counter % 100 == 0) {
        printf("Ball Debug: angles(X:%.1f°, Y:%.1f°) -> offset(%.1f, %.1f) -> ball_pos(%.1f, %.1f)\n",
               g_level_data.current_tilt.x_angle, g_level_data.current_tilt.y_angle,
               offset_x, offset_y,
               g_level_data.ball_x_current, g_level_data.ball_y_current);
    }

    // Keep ball black for clear visibility
    lv_obj_set_style_bg_color(g_level_data.ball, lv_color_black(), 0);
}

static void level_indicator_update_angle_display(void)
{
    // Update X angle display (left side)
    if (g_level_data.angle_x_label) {
        char x_text[32];
        snprintf(x_text, sizeof(x_text), "X: %+.1f°", g_level_data.current_tilt.x_angle);
        lv_label_set_text(g_level_data.angle_x_label, x_text);
    }

    // Update Y angle display (right side)
    if (g_level_data.angle_y_label) {
        char y_text[32];
        snprintf(y_text, sizeof(y_text), "Y: %+.1f°", g_level_data.current_tilt.y_angle);
        lv_label_set_text(g_level_data.angle_y_label, y_text);
    }
}

static void level_indicator_create_circle(void)
{
    // Create main level circle container
    g_level_data.level_circle = lv_obj_create(g_level_data.main_container);
    lv_obj_set_size(g_level_data.level_circle, LEVEL_CIRCLE_DIAMETER, LEVEL_CIRCLE_DIAMETER);
    lv_obj_align(g_level_data.level_circle, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_level_data.level_circle, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_level_data.level_circle, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_level_data.level_circle, 3, 0);
    lv_obj_set_style_border_color(g_level_data.level_circle, lv_color_black(), 0);
    lv_obj_set_style_radius(g_level_data.level_circle, LEVEL_CIRCLE_RADIUS, 0);
    lv_obj_set_style_pad_all(g_level_data.level_circle, 0, 0);

    // Create extended horizontal cross line (goes beyond circle)
    lv_obj_t *cross_h = lv_obj_create(g_level_data.main_container);
    lv_obj_set_size(cross_h, LEVEL_CROSS_ARM_LENGTH * 2, LEVEL_CROSS_LINE_WIDTH);
    lv_obj_align(cross_h, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(cross_h, lv_color_make(128, 128, 128), 0);
    lv_obj_set_style_bg_opa(cross_h, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cross_h, 0, 0);
    lv_obj_set_style_radius(cross_h, 0, 0);

    // Create extended vertical cross line (goes beyond circle)
    lv_obj_t *cross_v = lv_obj_create(g_level_data.main_container);
    lv_obj_set_size(cross_v, LEVEL_CROSS_LINE_WIDTH, LEVEL_CROSS_ARM_LENGTH * 2);
    lv_obj_align(cross_v, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(cross_v, lv_color_make(128, 128, 128), 0);
    lv_obj_set_style_bg_opa(cross_v, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cross_v, 0, 0);
    lv_obj_set_style_radius(cross_v, 0, 0);

    // Create center dead zone circle
    g_level_data.center_cross = lv_obj_create(g_level_data.level_circle);
    lv_obj_set_size(g_level_data.center_cross, LEVEL_CENTER_DEAD_ZONE * 2, LEVEL_CENTER_DEAD_ZONE * 2);
    lv_obj_center(g_level_data.center_cross);
    lv_obj_set_style_bg_opa(g_level_data.center_cross, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_level_data.center_cross, 2, 0);
    lv_obj_set_style_border_color(g_level_data.center_cross, lv_color_make(0, 255, 0), 0);
    lv_obj_set_style_radius(g_level_data.center_cross, LEVEL_CENTER_DEAD_ZONE, 0);

    // Create ball
    g_level_data.ball = lv_obj_create(g_level_data.level_circle);
    lv_obj_set_size(g_level_data.ball, LEVEL_BALL_SIZE, LEVEL_BALL_SIZE);
    lv_obj_set_style_bg_color(g_level_data.ball, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_level_data.ball, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_level_data.ball, 1, 0);
    lv_obj_set_style_border_color(g_level_data.ball, lv_color_white(), 0);
    lv_obj_set_style_radius(g_level_data.ball, LEVEL_BALL_SIZE / 2, 0);
    lv_obj_set_style_shadow_width(g_level_data.ball, 3, 0);
    lv_obj_set_style_shadow_color(g_level_data.ball, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(g_level_data.ball, LV_OPA_50, 0);

    // Initialize ball position to center
    g_level_data.ball_x_current = LEVEL_CIRCLE_RADIUS - LEVEL_BALL_SIZE / 2;
    g_level_data.ball_y_current = LEVEL_CIRCLE_RADIUS - LEVEL_BALL_SIZE / 2;
    g_level_data.ball_x_target = g_level_data.ball_x_current;
    g_level_data.ball_y_target = g_level_data.ball_y_current;
    
    // Set initial position explicitly (don't use lv_obj_center which conflicts with manual positioning)
    lv_obj_set_pos(g_level_data.ball, 
                   (lv_coord_t)g_level_data.ball_x_current, 
                   (lv_coord_t)g_level_data.ball_y_current);
}

static void level_indicator_create_controls(void)
{
    // Create X angle display label (left side)
    g_level_data.angle_x_label = lv_label_create(g_level_data.main_container);
    lv_obj_align(g_level_data.angle_x_label, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_font(g_level_data.angle_x_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_level_data.angle_x_label, lv_color_black(), 0);
    lv_obj_set_style_text_align(g_level_data.angle_x_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(g_level_data.angle_x_label, "X: 0.0°");

    // Create Y angle display label (right side, same height as X)
    g_level_data.angle_y_label = lv_label_create(g_level_data.main_container);
    lv_obj_align(g_level_data.angle_y_label, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_font(g_level_data.angle_y_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_level_data.angle_y_label, lv_color_black(), 0);
    lv_obj_set_style_text_align(g_level_data.angle_y_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(g_level_data.angle_y_label, "Y: 0.0°");

    // Create sensor status label at the very bottom of screen
    lv_obj_t *status_label = lv_label_create(g_level_data.screen); 
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -2); 
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(status_label, lv_color_make(100, 100, 100), 0);

#ifndef LVGL_SIMULATOR
    if (g_level_data.sensor_available) {
        lv_label_set_text(status_label, "BMI270 Ready | ESC=Exit");
    } else {
        lv_label_set_text(status_label, "Simulation Mode | C=Cal | ESC=Exit");
    }
#else
    lv_label_set_text(status_label, "Simulator Mode | C=Cal | ESC=Exit");
#endif
}

static void level_indicator_show_exit_dialog(void)
{
    if (g_level_data.exit_dialog) {
        return;  // Already shown
    }

    g_level_data.show_exit_dialog = 1;
    g_level_data.exit_selection = 0;  // Default to "No"

    // Create modal dialog background
    g_level_data.exit_dialog = lv_obj_create(g_level_data.screen);
    lv_obj_set_size(g_level_data.exit_dialog, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(g_level_data.exit_dialog, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_level_data.exit_dialog, LV_OPA_70, 0);
    lv_obj_set_pos(g_level_data.exit_dialog, 0, 0);

    // Create dialog box
    lv_obj_t *dialog_box = lv_obj_create(g_level_data.exit_dialog);
    lv_obj_set_size(dialog_box, 200, 120);
    lv_obj_center(dialog_box);
    lv_obj_set_style_bg_color(dialog_box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(dialog_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dialog_box, 2, 0);
    lv_obj_set_style_border_color(dialog_box, lv_color_black(), 0);

    // Message label
    lv_obj_t *msg_label = lv_label_create(dialog_box);
    lv_label_set_text(msg_label, "Exit Level Indicator?");
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_14, 0);
    lv_obj_align(msg_label, LV_ALIGN_TOP_MID, 0, 15);

    // Buttons
    lv_coord_t btn_width = 60;
    lv_coord_t btn_height = 30;
    lv_coord_t btn_spacing = 40;

    // No button (default selected)
    lv_obj_t *no_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(no_btn, btn_width, btn_height);
    lv_obj_align(no_btn, LV_ALIGN_BOTTOM_MID, -btn_spacing, -10);
    lv_obj_set_style_bg_color(no_btn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(no_btn, LV_OPA_COVER, 0);

    lv_obj_t *no_label = lv_label_create(no_btn);
    lv_label_set_text(no_label, "NO");
    lv_obj_center(no_label);
    lv_obj_set_style_text_color(no_label, lv_color_white(), 0);

    // Yes button
    lv_obj_t *yes_btn = lv_obj_create(dialog_box);
    lv_obj_set_size(yes_btn, btn_width, btn_height);
    lv_obj_align(yes_btn, LV_ALIGN_BOTTOM_MID, btn_spacing, -10);
    lv_obj_set_style_bg_color(yes_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(yes_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(yes_btn, 2, 0);
    lv_obj_set_style_border_color(yes_btn, lv_color_black(), 0);

    lv_obj_t *yes_label = lv_label_create(yes_btn);
    lv_label_set_text(yes_label, "YES");
    lv_obj_center(yes_label);
    lv_obj_set_style_text_color(yes_label, lv_color_black(), 0);
}

static void level_indicator_hide_exit_dialog(void)
{
    if (!g_level_data.exit_dialog) {
        return;
    }

    lv_obj_del(g_level_data.exit_dialog);
    g_level_data.exit_dialog = NULL;
    g_level_data.show_exit_dialog = 0;
}

static void level_indicator_update_exit_selection(void)
{
    if (!g_level_data.exit_dialog) {
        return;
    }

    // Find the buttons (they are children of the dialog box)
    lv_obj_t *dialog_box = lv_obj_get_child(g_level_data.exit_dialog, 0);

    // Find NO and YES buttons (last two children)
    uint32_t child_count = lv_obj_get_child_cnt(dialog_box);
    if (child_count >= 2) {
        lv_obj_t *no_btn = lv_obj_get_child(dialog_box, child_count - 2);
        lv_obj_t *yes_btn = lv_obj_get_child(dialog_box, child_count - 1);

        if (g_level_data.exit_selection == 0) {  // No selected
            lv_obj_set_style_bg_color(no_btn, lv_color_black(), 0);
            lv_obj_set_style_bg_color(yes_btn, lv_color_white(), 0);
        } else {  // Yes selected
            lv_obj_set_style_bg_color(no_btn, lv_color_white(), 0);
            lv_obj_set_style_bg_color(yes_btn, lv_color_black(), 0);
        }
    }
}

static void level_indicator_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!g_level_data.is_active) {
        printf("Timer callback: not active, returning\n");
        return;
    }

    // Read sensor data or simulate
#ifndef LVGL_SIMULATOR
    if (g_level_data.sensor_available && g_level_data.use_real_sensor) {
        level_indicator_read_real_sensor();
    } else {
        level_indicator_simulate_tilt();
    }
#else
    level_indicator_simulate_tilt();
#endif

    // Update UI
    level_indicator_update_ui();
}

static void level_indicator_simulate_tilt(void)
{
    // Add small random variations to simulate sensor noise
    static float time_counter = 0;
    time_counter += 0.1f;

    float noise_x = 0.5f * sinf(time_counter * 0.7f) + 0.2f * sinf(time_counter * 2.3f);
    float noise_y = 0.5f * cosf(time_counter * 0.9f) + 0.2f * cosf(time_counter * 1.7f);

    // Apply noise only if not showing dialog
    if (!g_level_data.show_exit_dialog) {
        g_level_data.current_tilt.x_angle += noise_x * 0.1f;
        g_level_data.current_tilt.y_angle += noise_y * 0.1f;

        // Recalculate magnitude
        g_level_data.current_tilt.magnitude = sqrtf(
            g_level_data.current_tilt.x_angle * g_level_data.current_tilt.x_angle +
            g_level_data.current_tilt.y_angle * g_level_data.current_tilt.y_angle
        );

        g_level_data.current_tilt.is_level = (g_level_data.current_tilt.magnitude <= g_level_data.level_threshold);
    }
}

static void level_indicator_init_sensor(void)
{
#ifndef LVGL_SIMULATOR
    printf("Getting BMI270 sensor handle...\n");
    
    // BMI270 is already registered, just get the handle
    g_level_data.bmi270_handle = board_bmi270_get_handle();
    if (g_level_data.bmi270_handle) {
        g_level_data.sensor_available = 1;
        g_level_data.use_real_sensor = 1;
        printf("BMI270 sensor handle obtained successfully\n");
    } else {
        printf("Failed to get BMI270 handle - sensor may not be registered\n");
        g_level_data.sensor_available = 0;
        g_level_data.use_real_sensor = 0;
    }
#else
    printf("Running in simulator mode, using simulated sensor data\n");
#endif
}

static void level_indicator_read_real_sensor(void)
{
#ifndef LVGL_SIMULATOR
    if (!g_level_data.bmi270_handle || !g_level_data.sensor_available) {
        return;
    }

    float acc_x, acc_y, acc_z;
    int result = board_bmi270_read_accel(g_level_data.bmi270_handle, &acc_x, &acc_y, &acc_z);
    
    if (result == 0) {  // OPERATE_RET_OK
        // Calculate tilt angles from accelerometer data
        float x_angle = level_indicator_calculate_tilt_angle(acc_x, acc_y, acc_z, 0); // X-axis tilt
        float y_angle = level_indicator_calculate_tilt_angle(acc_x, acc_y, acc_z, 1); // Y-axis tilt
        
        // Debug output for sensor data (every 50 readings to avoid spam)
        static int sensor_debug_counter = 0;
        if (++sensor_debug_counter % 50 == 0) {
            printf("Sensor Debug: acc(%.3f, %.3f, %.3f) -> raw_angles(X:%.1f°, Y:%.1f°) -> offset(X:%.1f°, Y:%.1f°)\n",
                   acc_x, acc_y, acc_z, x_angle, y_angle,
                   g_level_data.calibration_offset.x_angle, g_level_data.calibration_offset.y_angle);
        }
        
        // Update the level indicator with sensor data (calibration offset will be applied in level_indicator_update_tilt)
        level_indicator_update_tilt(x_angle, y_angle);
    } else {
        printf("Failed to read BMI270 accelerometer data, error: %d\n", result);
        // Fall back to simulation if sensor reading fails
        level_indicator_simulate_tilt();
    }
#endif
}

static float level_indicator_calculate_tilt_angle(float acc_x, float acc_y, float acc_z, int axis)
{
    // Calculate tilt angles using accelerometer data
    // For a device lying flat (horizontal), acc_z should be ~±9.8, acc_x and acc_y should be ~0
    
    const float rad_to_deg = 180.0f / M_PI;
    
    // Normalize the acceleration vector
    float magnitude = sqrtf(acc_x * acc_x + acc_y * acc_y + acc_z * acc_z);
    if (magnitude < 0.1f) {
        return 0.0f; // Avoid division by zero
    }
    
    if (axis == 0) {
        // X-axis tilt (pitch): how much the device is tilted forward/backward
        // Use asin for small angle approximation when device is mostly horizontal
        float normalized_y = acc_y / magnitude;
        // Clamp to valid asin range
        if (normalized_y > 1.0f) normalized_y = 1.0f;
        if (normalized_y < -1.0f) normalized_y = -1.0f;
        return asinf(normalized_y) * rad_to_deg;
    } else {
        // Y-axis tilt (roll): how much the device is tilted left/right  
        // Use asin for small angle approximation when device is mostly horizontal
        float normalized_x = acc_x / magnitude;
        // Clamp to valid asin range
        if (normalized_x > 1.0f) normalized_x = 1.0f;
        if (normalized_x < -1.0f) normalized_x = -1.0f;
        return -asinf(normalized_x) * rad_to_deg; // Negative for intuitive direction
    }
}

static inline float level_indicator_clamp(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}
