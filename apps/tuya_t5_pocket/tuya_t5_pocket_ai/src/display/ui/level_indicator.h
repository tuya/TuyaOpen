/**
 * @file level_indicator.h
 * Level Indicator (Digital Spirit Level) Component for AI Pocket Pet
 * Provides a virtual bubble level with tilt detection and angle display
 */

#ifndef LEVEL_INDICATOR_H
#define LEVEL_INDICATOR_H

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"
#include "ai_pocket_pet_app.h"
#include <stdint.h>
#include <stdbool.h>

/*********************
 *      DEFINES
 *********************/
// Level indicator constants
#define LEVEL_INDICATOR_UPDATE_PERIOD    50    /* Update interval in ms */
#define LEVEL_INDICATOR_MAX_ANGLE        90.0f /* Maximum tilt angle in degrees */
#define LEVEL_INDICATOR_LEVEL_THRESHOLD  2.0f  /* Level tolerance in degrees */

// Visual constants - Circular level design
#define LEVEL_BALL_SIZE                 12     /* Ball diameter in pixels */
#define LEVEL_CIRCLE_RADIUS            70      /* Main circle radius */
#define LEVEL_CIRCLE_DIAMETER          140     /* Main circle diameter */
#define LEVEL_CROSS_LINE_WIDTH          2      /* Cross line width */
#define LEVEL_CROSS_ARM_LENGTH         70      /* Cross arm length from center (equals radius) */
#define LEVEL_CENTER_DEAD_ZONE         6      /* Center dead zone radius */

// Key input definitions
#ifndef KEY_ESC
#define KEY_ESC     27
#endif
#ifndef KEY_ENTER
#define KEY_ENTER   13
#endif
#ifndef KEY_UP
#define KEY_UP      17
#endif
#ifndef KEY_DOWN
#define KEY_DOWN    18
#endif
#ifndef KEY_LEFT
#define KEY_LEFT    19
#endif
#ifndef KEY_RIGHT
#define KEY_RIGHT   20
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**
 * Level indicator event types
 */
typedef enum {
    LEVEL_INDICATOR_EVENT_LEVEL_ACHIEVED,    /* Device is level within threshold */
    LEVEL_INDICATOR_EVENT_TILT_DETECTED,     /* Device is tilted beyond threshold */
    LEVEL_INDICATOR_EVENT_ANGLE_CHANGED,     /* Tilt angle has changed */
    LEVEL_INDICATOR_EVENT_CALIBRATION,       /* Calibration requested */
    LEVEL_INDICATOR_EVENT_EXIT              /* Exit level indicator */
} level_indicator_event_t;

/**
 * Tilt data structure
 */
typedef struct {
    float x_angle;     /* X-axis tilt angle in degrees */
    float y_angle;     /* Y-axis tilt angle in degrees */
    float magnitude;   /* Total tilt magnitude */
    bool is_level;     /* True if within level threshold */
} tilt_data_t;

/**
 * Level indicator callback function
 * @param event Event type
 * @param tilt_data Current tilt information
 * @param user_data User data pointer
 */
typedef void (*level_indicator_callback_t)(level_indicator_event_t event,
                                         const tilt_data_t *tilt_data,
                                         void *user_data);

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Show the level indicator interface
 * Creates and displays the digital spirit level UI
 */
void level_indicator_show(void);

/**
 * Hide the level indicator interface
 * Cleans up and returns to main menu
 */
void level_indicator_hide(void);

/**
 * Handle key input for level indicator
 * @param key Key code from input system
 */
void level_indicator_key_input(int key);

/**
 * Set event callback for level indicator
 * @param callback Callback function to handle events
 * @param user_data User data pointer passed to callback
 */
void level_indicator_set_callback(level_indicator_callback_t callback, void *user_data);

/**
 * Update tilt data (normally called by sensor system)
 * @param x_angle X-axis tilt angle in degrees
 * @param y_angle Y-axis tilt angle in degrees
 */
void level_indicator_update_tilt(float x_angle, float y_angle);

/**
 * Calibrate the level indicator
 * Sets current position as level reference
 */
void level_indicator_calibrate(void);

/**
 * Get current tilt data
 * @return Pointer to current tilt data structure
 */
const tilt_data_t* level_indicator_get_tilt_data(void);

/**
 * Check if level indicator is currently active
 * @return True if level indicator is active
 */
bool level_indicator_is_active(void);

/**
 * Set level threshold tolerance
 * @param threshold_degrees New threshold in degrees
 */
void level_indicator_set_threshold(float threshold_degrees);

/**
 * Get current level threshold
 * @return Current threshold in degrees
 */
float level_indicator_get_threshold(void);

#endif /* LEVEL_INDICATOR_H */
