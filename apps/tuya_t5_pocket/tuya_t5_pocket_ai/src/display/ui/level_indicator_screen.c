/**
 * @file level_indicator_screen.c
 * @brief Digital level indicator with ball physics and sensor integration
 * @author AI Assistant
 * @date 2024
 * @note Based on level_indicator.c with Screen_t template integration
 */

#include "level_indicator_screen.h"
#include "math.h"
#include "stdio.h"
#include "string.h"

#ifdef ENABLE_LVGL_HARDWARE
#include "board_bmi270_api.h"
#endif

/***********************************************************
***********************constants define*********************
***********************************************************/

// Screen dimensions
#define SCREEN_WIDTH 384
#define SCREEN_HEIGHT 168

// Level circle parameters
#define LEVEL_CIRCLE_DIAMETER 140
#define LEVEL_CIRCLE_RADIUS (LEVEL_CIRCLE_DIAMETER / 2)
#define LEVEL_CENTER_DEAD_ZONE 6
#define LEVEL_BALL_SIZE 12
#define LEVEL_CROSS_ARM_LENGTH 70
#define LEVEL_CROSS_LINE_WIDTH 2

// Physics and sensitivity
#define LEVEL_INDICATOR_ANGLE_SCALE 2.0f
#define LEVEL_INDICATOR_LEVEL_THRESHOLD 2.0f
#define LEVEL_INDICATOR_UPDATE_PERIOD 50
#define LEVEL_INDICATOR_MAX_ANGLE 90.0f
#define BALL_MOVE_SMOOTH_FACTOR 0.15f

/***********************************************************
***********************typedefs****************************
***********************************************************/

// Tilt data structure
typedef struct {
    float x_angle;     /* X-axis tilt angle in degrees */
    float y_angle;     /* Y-axis tilt angle in degrees */
    float magnitude;   /* Total tilt magnitude */
    uint8_t is_level;  /* 1 if within level threshold */
} tilt_data_t;

// Calibration data structure
typedef struct {
    float x_offset;    /* X-axis calibration offset */
    float y_offset;    /* Y-axis calibration offset */
    uint8_t is_calibrated; /* 1 if calibration has been performed */
} calibration_data_t;

// Level indicator data structure
typedef struct {
    // UI components
    lv_obj_t *main_container;
    lv_obj_t *level_circle;
    lv_obj_t *ball;
    lv_obj_t *center_cross;
    lv_obj_t *angle_x_label;
    lv_obj_t *angle_y_label;

    // Measurement state
    tilt_data_t current_tilt;
    calibration_data_t calibration;
    float level_threshold;

    // Hardware sensor support
#ifdef ENABLE_LVGL_HARDWARE
    void *bmi270_handle;
    uint8_t sensor_available : 1;
#endif
    uint8_t use_real_sensor : 1;

    // UI state
    uint8_t is_active : 1;
    uint8_t _reserved : 5;

    // Animation state
    float ball_x_target;
    float ball_y_target;
    float ball_x_current;
    float ball_y_current;
} level_indicator_data_t;

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_level_indicator_screen;
static lv_timer_t *update_timer;
static level_indicator_data_t g_level_data;

/***********************************************************
********************function declaration********************
***********************************************************/

static void level_indicator_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);
static void level_indicator_update_ui(void);
static void level_indicator_update_ball_position(void);
static void level_indicator_update_angle_display(void);
static void level_indicator_create_circle(void);
static void level_indicator_create_controls(void);
static void level_indicator_show_calibration_dialog(void);
static void calibration_confirm_cb(lv_event_t *e);
static void calibration_cancel_cb(lv_event_t *e);

#ifdef ENABLE_LVGL_HARDWARE
static void level_indicator_read_sensor(void);
static void level_indicator_init_sensor(void);
static float level_indicator_calculate_tilt_angle(float acc_x, float acc_y, float acc_z, int axis);
#endif

/***********************************************************
*********************Screen interface***********************
***********************************************************/

// Screen interface structure for template compliance
Screen_t level_indicator_screen = {
    .init = level_indicator_screen_init,
    .deinit = level_indicator_screen_deinit,
    .screen_obj = &ui_level_indicator_screen,
    .name = "level_indicator",
};

/***********************************************************
*********************public functions***********************
***********************************************************/

/**
 * @brief Update tilt angle values from external source
 * @param x_angle X-axis tilt angle in degrees
 * @param y_angle Y-axis tilt angle in degrees
 */
void level_indicator_update_tilt(float x_angle, float y_angle)
{
    if (!g_level_data.is_active) {
        return;
    }

    // Apply calibration offset
    x_angle -= g_level_data.calibration.x_offset;
    y_angle -= g_level_data.calibration.y_offset;

    // Clamp angles to maximum range
    if (x_angle > LEVEL_INDICATOR_MAX_ANGLE) x_angle = LEVEL_INDICATOR_MAX_ANGLE;
    if (x_angle < -LEVEL_INDICATOR_MAX_ANGLE) x_angle = -LEVEL_INDICATOR_MAX_ANGLE;
    if (y_angle > LEVEL_INDICATOR_MAX_ANGLE) y_angle = LEVEL_INDICATOR_MAX_ANGLE;
    if (y_angle < -LEVEL_INDICATOR_MAX_ANGLE) y_angle = -LEVEL_INDICATOR_MAX_ANGLE;

    // Update tilt data
    g_level_data.current_tilt.x_angle = x_angle;
    g_level_data.current_tilt.y_angle = y_angle;
    g_level_data.current_tilt.magnitude = sqrtf(x_angle * x_angle + y_angle * y_angle);

    // bool was_level = g_level_data.current_tilt.is_level;
    g_level_data.current_tilt.is_level = (g_level_data.current_tilt.magnitude <= g_level_data.level_threshold);

    // Debug output for final angles (every 75 updates to avoid spam)
    static int tilt_debug_counter = 0;
    if (++tilt_debug_counter % 75 == 0) {
        printf("Tilt Debug: final_angles(X:%.1f°, Y:%.1f°) -> magnitude:%.2f° -> is_level:%s\n",
               g_level_data.current_tilt.x_angle, g_level_data.current_tilt.y_angle,
               g_level_data.current_tilt.magnitude, g_level_data.current_tilt.is_level ? "YES" : "NO");
    }
}

/**
 * @brief Calibrate the level indicator using current sensor reading
 */
void level_indicator_calibrate(void)
{
    if (!g_level_data.is_active) {
        return;
    }

    // Read current sensor angles and set them as calibration offset
#ifdef ENABLE_LVGL_HARDWARE
    if (g_level_data.sensor_available && g_level_data.bmi270_handle) {
        float acc_x, acc_y, acc_z;
        int result = board_bmi270_read_accel(g_level_data.bmi270_handle, &acc_x, &acc_y, &acc_z);

        if (result == 0) {
            // Calculate current raw angles and use them as calibration offset
            g_level_data.calibration.x_offset = level_indicator_calculate_tilt_angle(acc_x, acc_y, acc_z, 0);
            g_level_data.calibration.y_offset = level_indicator_calculate_tilt_angle(acc_x, acc_y, acc_z, 1);
            g_level_data.calibration.is_calibrated = 1;
            printf("Calibration completed: offset set to X:%.1f°, Y:%.1f°\n",
                   g_level_data.calibration.x_offset, g_level_data.calibration.y_offset);
        } else {
            printf("Failed to read sensor for calibration\n");
            return;
        }
    } else
#endif
    {
        // Fallback for simulator: set current offset to make current position become center
        g_level_data.calibration.x_offset = g_level_data.current_tilt.x_angle + g_level_data.calibration.x_offset;
        g_level_data.calibration.y_offset = g_level_data.current_tilt.y_angle + g_level_data.calibration.y_offset;
        g_level_data.calibration.is_calibrated = 1;
        printf("Calibration completed (simulator mode): offset set to X:%.1f°, Y:%.1f°\n",
               g_level_data.calibration.x_offset, g_level_data.calibration.y_offset);
    }
}

/***********************************************************
*********************static functions***********************
***********************************************************/

static void level_indicator_cleanup(void)
{
    // Stop timer first
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }

    // Reset all object pointers
    g_level_data.main_container = NULL;
    g_level_data.level_circle = NULL;
    g_level_data.ball = NULL;
    g_level_data.center_cross = NULL;
    g_level_data.angle_x_label = NULL;
    g_level_data.angle_y_label = NULL;

    // Reset state
    memset(&g_level_data, 0, sizeof(level_indicator_data_t));
}

static void level_indicator_update_ui(void)
{
    if (!g_level_data.is_active) return;

    // Update ball position based on tilt
    level_indicator_update_ball_position();

    // Update angle displays
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
    lv_obj_t *status_label = lv_label_create(ui_level_indicator_screen);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(status_label, lv_color_make(100, 100, 100), 0);

#ifdef ENABLE_LVGL_HARDWARE
    if (g_level_data.sensor_available) {
        lv_label_set_text(status_label, "BMI270 Ready | ESC=Exit");
    } else {
        lv_label_set_text(status_label, "Simulation Mode | C=Cal | ESC=Exit");
    }
#else
    lv_label_set_text(status_label, "Simulator Mode | C=Cal | ESC=Exit");
#endif
}

#ifdef ENABLE_LVGL_HARDWARE
static void level_indicator_init_sensor(void)
{
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

static void level_indicator_read_sensor(void)
{
    if (!g_level_data.bmi270_handle || !g_level_data.sensor_available) {
        return;
    }

    float acc_x, acc_y, acc_z;
    int result = board_bmi270_read_accel(g_level_data.bmi270_handle, &acc_x, &acc_y, &acc_z);

    if (result == 0) {  // OPERATE_RET_OK
        // Calculate tilt angles from accelerometer data
        float x_angle = level_indicator_calculate_tilt_angle(acc_x, acc_y, acc_z, 0); // X-axis tilt
        float y_angle = level_indicator_calculate_tilt_angle(acc_x, acc_y, acc_z, 1); // Y-axis tilt

        // Apply calibration offset
        x_angle -= g_level_data.calibration.x_offset;
        y_angle -= g_level_data.calibration.y_offset;

        // Update current tilt data
        g_level_data.current_tilt.x_angle = x_angle;
        g_level_data.current_tilt.y_angle = y_angle;
        g_level_data.current_tilt.magnitude = sqrtf(x_angle * x_angle + y_angle * y_angle);
        g_level_data.current_tilt.is_level = (g_level_data.current_tilt.magnitude <= g_level_data.level_threshold) ? 1 : 0;

        // Debug output for sensor data (every 50 readings to avoid spam)
        static int sensor_debug_counter = 0;
        if (++sensor_debug_counter % 50 == 0) {
            printf("Sensor Debug: acc(%.3f, %.3f, %.3f) -> angles(X:%.1f°, Y:%.1f°)\n",
                   acc_x, acc_y, acc_z, x_angle, y_angle);
        }
    } else {
        printf("Failed to read BMI270 accelerometer data, error: %d\n", result);
        g_level_data.sensor_available = false;
    }
}
#endif

static void level_indicator_timer_cb(lv_timer_t *timer)
{
    (void)timer; // Suppress unused parameter warning

#ifdef ENABLE_LVGL_HARDWARE
    if (g_level_data.sensor_available && g_level_data.use_real_sensor) {
        level_indicator_read_sensor();
    } else {
        // Simulation mode - create gentle oscillating movement for demo
        static uint32_t sim_tick = 0;
        sim_tick++;
        g_level_data.current_tilt.x_angle = sinf(sim_tick * 0.01f) * 2.0f;
        g_level_data.current_tilt.y_angle = cosf(sim_tick * 0.015f) * 1.5f;
    }
#else
    // Simulation mode - create gentle oscillating movement for demo
    static uint32_t sim_tick = 0;
    sim_tick++;
    g_level_data.current_tilt.x_angle = sinf(sim_tick * 0.01f) * 2.0f;
    g_level_data.current_tilt.y_angle = cosf(sim_tick * 0.015f) * 1.5f;
#endif

    // Update UI with new tilt values
    level_indicator_update_ui();
}

static void calibration_confirm_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Perform calibration with current sensor reading
        level_indicator_calibrate();

        // Close the dialog
        lv_obj_t *dialog = lv_event_get_user_data(e);
        if (dialog) {
            lv_obj_del_async(dialog);
        }
    }
}

static void calibration_cancel_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Close the dialog without calibrating
        lv_obj_t *dialog = lv_event_get_user_data(e);
        if (dialog) {
            lv_obj_del_async(dialog);
        }
    }
}

static void level_indicator_show_calibration_dialog(void)
{
    // Create modal dialog
    lv_obj_t *dialog = lv_obj_create(ui_level_indicator_screen);
    lv_obj_set_size(dialog, 300, 180);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_border_color(dialog, lv_color_black(), 0);
    lv_obj_set_style_radius(dialog, 10, 0);
    lv_obj_set_style_shadow_width(dialog, 10, 0);
    lv_obj_set_style_shadow_color(dialog, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(dialog, LV_OPA_30, 0);

    // Title
    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, "Calibration");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);

    // Instructions
    lv_obj_t *msg = lv_label_create(dialog);
    lv_label_set_text(msg, "Place device on a level surface\nand click Calibrate to set zero\nreference point.");
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);

    // Buttons container
    lv_obj_t *btn_container = lv_obj_create(dialog);
    lv_obj_set_size(btn_container, 260, 40);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Calibrate button
    lv_obj_t *btn_cal = lv_btn_create(btn_container);
    lv_obj_set_size(btn_cal, 100, 35);
    lv_obj_add_event_cb(btn_cal, calibration_confirm_cb, LV_EVENT_CLICKED, dialog);
    lv_obj_t *btn_cal_label = lv_label_create(btn_cal);
    lv_label_set_text(btn_cal_label, "Calibrate");
    lv_obj_center(btn_cal_label);

    // Cancel button
    lv_obj_t *btn_cancel = lv_btn_create(btn_container);
    lv_obj_set_size(btn_cancel, 100, 35);
    lv_obj_add_event_cb(btn_cancel, calibration_cancel_cb, LV_EVENT_CLICKED, dialog);
    lv_obj_t *btn_cancel_label = lv_label_create(btn_cancel);
    lv_label_set_text(btn_cancel_label, "Cancel");
    lv_obj_center(btn_cancel_label);
}

static void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    switch (key) {
        case LV_KEY_ESC:
            // Exit to previous screen
            screen_back();
            break;
        case 'c':
        case 'C':
            // Show calibration dialog
            level_indicator_show_calibration_dialog();
            break;
#ifdef ENABLE_LVGL_HARDWARE
        case 's':
        case 'S':
            // Toggle between real sensor and simulation
            if (g_level_data.sensor_available) {
                g_level_data.use_real_sensor = !g_level_data.use_real_sensor;
                printf("Switched to %s mode\n",
                       g_level_data.use_real_sensor ? "Real BMI270" : "Simulation");
            } else {
                printf("BMI270 sensor not available, cannot switch to real sensor mode\n");
            }
            break;
#endif
        default:
            break;
    }
}

/**
 * @brief Initialize the level indicator screen
 */
void level_indicator_screen_init(void)
{
    ui_level_indicator_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_level_indicator_screen, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_level_indicator_screen, lv_color_make(240, 240, 240), 0);

    // Initialize level data
    memset(&g_level_data, 0, sizeof(level_indicator_data_t));
    g_level_data.is_active = 1;
    g_level_data.level_threshold = LEVEL_INDICATOR_LEVEL_THRESHOLD;

#ifdef ENABLE_LVGL_HARDWARE
    // Initialize BMI270 sensor
    level_indicator_init_sensor();
    if (g_level_data.sensor_available) {
        printf("BMI270 sensor initialized successfully\n");
    } else {
        printf("BMI270 sensor not available, using simulation mode\n");
    }
#else
    printf("Hardware support disabled, using simulation mode\n");
#endif

    // Create main container
    g_level_data.main_container = lv_obj_create(ui_level_indicator_screen);
    lv_obj_set_size(g_level_data.main_container, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 20);
    lv_obj_center(g_level_data.main_container);
    lv_obj_set_style_bg_opa(g_level_data.main_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_level_data.main_container, 0, 0);
    lv_obj_set_style_pad_all(g_level_data.main_container, 5, 0);

    // Create UI components
    level_indicator_create_circle();
    level_indicator_create_controls();

    // Start update timer
    update_timer = lv_timer_create(level_indicator_timer_cb, LEVEL_INDICATOR_UPDATE_PERIOD, &g_level_data);

    // Event handling
    lv_obj_add_event_cb(ui_level_indicator_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_level_indicator_screen);
    lv_group_focus_obj(ui_level_indicator_screen);

    // Initial update
    level_indicator_update_ui();
}

/**
 * @brief Deinitialize the level indicator screen
 */
void level_indicator_screen_deinit(void)
{
    if (ui_level_indicator_screen) {
        printf("Deinitializing level indicator screen\n");
        lv_obj_remove_event_cb(ui_level_indicator_screen, NULL);
        lv_group_remove_obj(ui_level_indicator_screen);
    }

    // Clean up resources
    level_indicator_cleanup();
}
