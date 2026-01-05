#include "tkl_pwm.h"
#include "tal_system.h"
#include "tal_log.h"
#include "tal_sw_timer.h"

#include "app_servo.h"
#define EMOJI_GPIO_NUM_18   TUYA_PWM_NUM_0
#define EMOJI_GPIO_NUM_24   TUYA_PWM_NUM_1
#define EMOJI_GPIO_NUM_32   TUYA_PWM_NUM_2
#define EMOJI_GPIO_NUM_34   TUYA_PWM_NUM_3
#define EMOJI_GPIO_NUM_36   TUYA_PWM_NUM_4
#define EMOJI_GPIO_NUM_9    TUYA_PWM_NUM_7


#define SERVO_PWM_VERTICAL           EMOJI_GPIO_NUM_34 //head
#define SERVO_PWM_HORIZONTAL         EMOJI_GPIO_NUM_9 //body


#define SERVO_PWM_FREQ               50      // 50Hz
#define SERVO_MIN_DUTY               250     // 0°, duty = 0.5ms/20ms * cycle = 250
#define SERVO_MAX_DUTY               1250    // 180°, duty = 2.5ms/20ms * cycle = 1250
#define SERVO_PWM_CYCLE              10000   // tkl_pwm cycle = 10000
#define SERVO_STEP_COUNT             50      // Number of steps for smooth movement (reduced for faster movement)
#define SERVO_MOVE_TIME_MS           400     // Total move time in ms (reduced for faster movement)
#define SERVO_FAST_MOVE_TIME_MS      400     // Fast movement time for quick actions (reduced speed)
#define SERVO_SLOW_MOVE_TIME_MS      1200    // Slow movement time for smooth actions (reduced speed)

// Servo action angle constants
#define SERVO_ANGLE_UP           0
#define SERVO_ANGLE_DOWN         90
#define SERVO_ANGLE_CENTER_VERT  45
#define SERVO_ANGLE_CENTER_HORI  90
#define SERVO_ANGLE_LEFT         30
#define SERVO_ANGLE_RIGHT        150

// Maintain current angles of horizontal and vertical servos
STATIC UINT_T s_servo_horizontal_angle = SERVO_ANGLE_CENTER_HORI;
STATIC UINT_T s_servo_vertical_angle   = SERVO_ANGLE_CENTER_VERT;

// Servo position states for smooth transition
typedef enum {
    SERVO_POS_UP = 0,
    SERVO_POS_CENTER_VERT,
    SERVO_POS_DOWN,
    SERVO_POS_LEFT,
    SERVO_POS_CENTER_HORI,
    SERVO_POS_RIGHT
} SERVO_POSITION_E;

STATIC SERVO_POSITION_E s_servo_vertical_state = SERVO_POS_CENTER_VERT;
STATIC SERVO_POSITION_E s_servo_horizontal_state = SERVO_POS_CENTER_HORI;

// Auto center timer variables
#define AUTO_CENTER_TIMEOUT_MS        5000   // 5 seconds timeout
STATIC TIMER_ID s_auto_center_timer = 0;
STATIC BOOL_T s_auto_center_enabled = TRUE;

STATIC UINT32_T angle_to_duty(INT_T angle)
{
    FLOAT_T pulse_ms = 1.0;

    // Clamp angle
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    pulse_ms = 0.5 + (angle / 180.0) * 2;
    
    // Convert to duty cycle value (20ms period corresponds to 10000 units, 1ms=500 units)
    return (UINT32_T)(pulse_ms * 500);
}

STATIC FLOAT_T ease_in_out_cubic(FLOAT_T t)
{
    if (t < 0.5) {
        return 4 * t * t * t;
    }

    return (1 - 4 * (1 - t) * (1 - t) * (1 - t));
}

// Forward declarations
STATIC VOID app_servo_center(VOID);
STATIC VOID app_servo_move_to_with_speed(TUYA_PWM_NUM_E ch_id, UINT_T *p_angle, INT_T target_angle, UINT_T move_time_ms);
STATIC VOID app_servo_auto_center_timer_cb(TIMER_ID timer_id, PVOID_T arg);
STATIC VOID app_servo_smooth_move_vertical(SERVO_ACTION_E action);
STATIC VOID app_servo_smooth_move_horizontal(SERVO_ACTION_E action);
STATIC CONST CHAR_T* app_servo_action_to_string(SERVO_ACTION_E action);

// Enhanced move function with speed control
STATIC VOID app_servo_move_to_with_speed(TUYA_PWM_NUM_E ch_id, UINT_T *p_angle, INT_T target_angle, UINT_T move_time_ms)
{
    // Add safety checks
    if (p_angle == NULL) {
        PR_ERR("p_angle is NULL");
        return;
    }
    
    // Clamp target angle to valid range
    if (target_angle < 0) target_angle = 0;
    if (target_angle > 180) target_angle = 180;
    
    INT_T start_angle = *p_angle;
    INT_T delta = target_angle - start_angle;
    INT_T abs_delta = delta > 0 ? delta : -delta;
    if (abs_delta == 0) return;

    UINT_T total_time = (move_time_ms * abs_delta) / 180;
    if (total_time == 0) {
        total_time = move_time_ms / SERVO_STEP_COUNT;
    } else if (total_time > move_time_ms) {
        total_time = move_time_ms;
    }

    UINT_T steps = total_time / (move_time_ms / SERVO_STEP_COUNT);
    if (steps == 0) steps = 1;
    if (steps > 1000) steps = 1000; // Prevent excessive steps
    UINT_T step_delay = total_time / steps;
    if (step_delay == 0) step_delay = 1; // Prevent zero delay

    PR_DEBUG("Moving servo from %d to %d, steps: %d, delay: %d", start_angle, target_angle, steps, step_delay);

    for (UINT_T i = 1; i <= steps; ++i) {
        FLOAT_T t = (FLOAT_T)i / steps;
        FLOAT_T ease = ease_in_out_cubic(t);
        INT_T cur_angle = start_angle + (INT_T)(delta * ease + 0.5f);
        UINT32_T duty = angle_to_duty(cur_angle);
        
        // Add error checking for PWM
        OPERATE_RET ret = tkl_pwm_duty_set(ch_id, duty);
        if (ret != OPRT_OK) {
            PR_ERR("PWM duty set failed: %d", ret);
            break;
        }
        
        tal_system_sleep(step_delay);
    }
    *p_angle = target_angle;
}

// Optimized: Add parameters to control horizontal and vertical channel angles separately
STATIC VOID app_servo_move_to(TUYA_PWM_NUM_E ch_id, UINT_T *p_angle, INT_T target_angle)
{
    app_servo_move_to_with_speed(ch_id, p_angle, target_angle, SERVO_MOVE_TIME_MS);
}

// Auto center timer callback function
STATIC VOID app_servo_auto_center_timer_cb(TIMER_ID timer_id, PVOID_T arg)
{
    if (!s_auto_center_enabled) return;
    
    PR_DEBUG("Auto centering after %d ms of inactivity", AUTO_CENTER_TIMEOUT_MS);
    app_servo_center();
}

// Start auto center timer
STATIC VOID app_servo_start_auto_center_timer(VOID)
{
    if (!s_auto_center_enabled) return;
    
    // Stop existing timer if any
    if (s_auto_center_timer != 0) {
        tal_sw_timer_stop(s_auto_center_timer);
        tal_sw_timer_delete(s_auto_center_timer);
        s_auto_center_timer = 0;
    }
    
    // Create new timer
    OPERATE_RET ret = tal_sw_timer_create(app_servo_auto_center_timer_cb, NULL, &s_auto_center_timer);
    if (ret == OPRT_OK && s_auto_center_timer != 0) {
        // Start timer with timeout and one-shot type
        ret = tal_sw_timer_start(s_auto_center_timer, AUTO_CENTER_TIMEOUT_MS, TAL_TIMER_ONCE);
        if (ret == OPRT_OK) {
            PR_DEBUG("Auto center timer started for %d ms", AUTO_CENTER_TIMEOUT_MS);
        } else {
            PR_ERR("Failed to start auto center timer: %d", ret);
            tal_sw_timer_delete(s_auto_center_timer);
            s_auto_center_timer = 0;
        }
    } else {
        PR_ERR("Failed to create auto center timer: %d", ret);
    }
}

// Stop auto center timer
STATIC VOID app_servo_stop_auto_center_timer(VOID)
{
    if (s_auto_center_timer != 0) {
        tal_sw_timer_stop(s_auto_center_timer);
        tal_sw_timer_delete(s_auto_center_timer);
        s_auto_center_timer = 0;
        PR_DEBUG("Auto center timer stopped");
    }
}

// Update action time - restart timer on new action
STATIC VOID app_servo_update_action_time(VOID)
{
    app_servo_start_auto_center_timer();
}

// Convert servo action enum to string for debugging
STATIC CONST CHAR_T* app_servo_action_to_string(SERVO_ACTION_E action)
{
    switch (action) {
        case SERVO_UP:
            return "UP";
        case SERVO_DOWN:
            return "DOWN";
        case SERVO_LEFT:
            return "LEFT";
        case SERVO_RIGHT:
            return "RIGHT";
        case SERVO_CENTER:
            return "CENTER";
        case SERVO_NOD:
            return "NOD";
        case SERVO_CLOCKWISE:
            return "CLOCKWISE";
        case SERVO_ANTICLOCKWISE:
            return "ANTICLOCKWISE";
        default:
            return "UNKNOWN";
    }
}

// Smooth vertical movement with center transition
STATIC VOID app_servo_smooth_move_vertical(SERVO_ACTION_E action)
{
    SERVO_POSITION_E target_state;
    INT_T target_angle;
    
    // Determine target state and angle
    if (action == SERVO_UP) {
        target_state = SERVO_POS_UP;
        target_angle = SERVO_ANGLE_UP;
    } else if (action == SERVO_DOWN) {
        target_state = SERVO_POS_DOWN;
        target_angle = SERVO_ANGLE_DOWN;
    } else {
        return; // Invalid action
    }
    
    // Check if we need to go through center first
    if (s_servo_vertical_state != SERVO_POS_CENTER_VERT && 
        s_servo_vertical_state != target_state) {
        
        PR_DEBUG("Vertical smooth transition: %d -> CENTER (stopping here, waiting for next gesture)", 
                 s_servo_vertical_state);
        
        // Move to center and stop there, wait for next gesture
        app_servo_move_to_with_speed(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, 
                                   SERVO_ANGLE_CENTER_VERT, SERVO_FAST_MOVE_TIME_MS);
        s_servo_vertical_state = SERVO_POS_CENTER_VERT;
        
        PR_DEBUG("Vertical movement stopped at center, waiting for next gesture");
        
    } else {
        // Direct movement (already at center or same target)
        PR_DEBUG("Vertical direct movement to %d", target_state);
        app_servo_move_to_with_speed(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, 
                                   target_angle, SERVO_FAST_MOVE_TIME_MS);
        s_servo_vertical_state = target_state;
    }
}

// Smooth horizontal movement with center transition
STATIC VOID app_servo_smooth_move_horizontal(SERVO_ACTION_E action)
{
    SERVO_POSITION_E target_state;
    INT_T target_angle;
    
    // Determine target state and angle
    if (action == SERVO_LEFT) {
        target_state = SERVO_POS_LEFT;
        target_angle = SERVO_ANGLE_LEFT;
    } else if (action == SERVO_RIGHT) {
        target_state = SERVO_POS_RIGHT;
        target_angle = SERVO_ANGLE_RIGHT;
    } else {
        return; // Invalid action
    }
    
    // Check if we need to go through center first
    if (s_servo_horizontal_state != SERVO_POS_CENTER_HORI && 
        s_servo_horizontal_state != target_state) {
        
        PR_DEBUG("Horizontal smooth transition: %d -> CENTER (stopping here, waiting for next gesture)", 
                 s_servo_horizontal_state);
        
        // Move to center and stop there, wait for next gesture
        app_servo_move_to_with_speed(SERVO_PWM_HORIZONTAL, &s_servo_horizontal_angle, 
                                   SERVO_ANGLE_CENTER_HORI, SERVO_FAST_MOVE_TIME_MS);
        s_servo_horizontal_state = SERVO_POS_CENTER_HORI;
        
        PR_DEBUG("Horizontal movement stopped at center, waiting for next gesture");
        
    } else {
        // Direct movement (already at center or same target)
        PR_DEBUG("Horizontal direct movement to %d", target_state);
        app_servo_move_to_with_speed(SERVO_PWM_HORIZONTAL, &s_servo_horizontal_angle, 
                                   target_angle, SERVO_FAST_MOVE_TIME_MS);
        s_servo_horizontal_state = target_state;
    }
}

// Vertical center (90°)
STATIC VOID app_servo_center(VOID)
{
    app_servo_move_to(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, SERVO_ANGLE_CENTER_VERT);
    app_servo_move_to(SERVO_PWM_HORIZONTAL, &s_servo_horizontal_angle, SERVO_ANGLE_CENTER_HORI);
    
    // Update states to center
    s_servo_vertical_state = SERVO_POS_CENTER_VERT;
    s_servo_horizontal_state = SERVO_POS_CENTER_HORI;
}

// Enhanced nod action: faster and with more amplitude
STATIC VOID app_servo_nod(VOID)
{
    UINT_T i;
    // Increased amplitude for more noticeable nodding
    INT_T nod_down = SERVO_ANGLE_CENTER_VERT + 35;  // More down movement
    INT_T nod_up = SERVO_ANGLE_CENTER_VERT - 25;    // More up movement

    PR_DEBUG("Starting enhanced nod action");
    
    // Start from center
    app_servo_move_to_with_speed(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, SERVO_ANGLE_CENTER_VERT, SERVO_FAST_MOVE_TIME_MS);
    
    // Perform 4 quick nods with increased amplitude
    for (i = 0; i < 4; ++i) {
        app_servo_move_to_with_speed(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, nod_down, SERVO_FAST_MOVE_TIME_MS);
        app_servo_move_to_with_speed(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, nod_up, SERVO_FAST_MOVE_TIME_MS);
    }
    
    // Return to center
    app_servo_move_to_with_speed(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, SERVO_ANGLE_CENTER_VERT, SERVO_FAST_MOVE_TIME_MS);
    
    PR_DEBUG("Enhanced nod action completed");
}

// Simple and stable clockwise rotation
STATIC VOID app_servo_clockwise(VOID)
{
    PR_DEBUG("Starting simple clockwise rotation");
    
    // Ensure we start from center
    app_servo_move_to(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, SERVO_ANGLE_CENTER_VERT);
    app_servo_move_to(SERVO_PWM_HORIZONTAL, &s_servo_horizontal_angle, SERVO_ANGLE_CENTER_HORI);
    tal_system_sleep(100);

    // Simple clockwise sequence: Right -> Down -> Left -> Up -> Center
    PR_DEBUG("Clockwise: Right");
    app_servo_move_to(SERVO_PWM_HORIZONTAL, &s_servo_horizontal_angle, SERVO_ANGLE_RIGHT);
    tal_system_sleep(200);

    PR_DEBUG("Clockwise: Down");
    app_servo_move_to(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, SERVO_ANGLE_DOWN);
    tal_system_sleep(200);

    PR_DEBUG("Clockwise: Left");
    app_servo_move_to(SERVO_PWM_HORIZONTAL, &s_servo_horizontal_angle, SERVO_ANGLE_LEFT);
    tal_system_sleep(200);

    PR_DEBUG("Clockwise: Up");
    app_servo_move_to(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, SERVO_ANGLE_UP);
    tal_system_sleep(200);

    // Return to center
    PR_DEBUG("Clockwise: Return to center");
    app_servo_move_to(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, SERVO_ANGLE_CENTER_VERT);
    app_servo_move_to(SERVO_PWM_HORIZONTAL, &s_servo_horizontal_angle, SERVO_ANGLE_CENTER_HORI);
    
    PR_DEBUG("Simple clockwise rotation completed");
}

// Simple and stable counter-clockwise rotation
STATIC VOID app_servo_anticlockwise(VOID)
{
    PR_DEBUG("Starting simple anticlockwise rotation");
    
    // Ensure we start from center
    app_servo_move_to(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, SERVO_ANGLE_CENTER_VERT);
    app_servo_move_to(SERVO_PWM_HORIZONTAL, &s_servo_horizontal_angle, SERVO_ANGLE_CENTER_HORI);
    tal_system_sleep(100);

    // Simple counter-clockwise sequence: Right -> Up -> Left -> Down -> Center
    PR_DEBUG("Anticlockwise: Right");
    app_servo_move_to(SERVO_PWM_HORIZONTAL, &s_servo_horizontal_angle, SERVO_ANGLE_RIGHT);
    tal_system_sleep(200);

    PR_DEBUG("Anticlockwise: Up");
    app_servo_move_to(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, SERVO_ANGLE_UP);
    tal_system_sleep(200);

    PR_DEBUG("Anticlockwise: Left");
    app_servo_move_to(SERVO_PWM_HORIZONTAL, &s_servo_horizontal_angle, SERVO_ANGLE_LEFT);
    tal_system_sleep(200);

    PR_DEBUG("Anticlockwise: Down");
    app_servo_move_to(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, SERVO_ANGLE_DOWN);
    tal_system_sleep(200);

    // Return to center
    PR_DEBUG("Anticlockwise: Return to center");
    app_servo_move_to(SERVO_PWM_VERTICAL, &s_servo_vertical_angle, SERVO_ANGLE_CENTER_VERT);
    app_servo_move_to(SERVO_PWM_HORIZONTAL, &s_servo_horizontal_angle, SERVO_ANGLE_CENTER_HORI);
    
    PR_DEBUG("Simple anticlockwise rotation completed");
}

OPERATE_RET app_servo_init(VOID)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_PWM_BASE_CFG_T cfg_x = {
        .frequency = SERVO_PWM_FREQ,
        .duty = angle_to_duty(SERVO_ANGLE_CENTER_HORI), // Center position
        .polarity = TUYA_PWM_POSITIVE,
    };

    TUYA_PWM_BASE_CFG_T cfg_y = {
        .frequency = SERVO_PWM_FREQ,
        .duty = angle_to_duty(SERVO_ANGLE_CENTER_VERT), // Center position
        .polarity = TUYA_PWM_NEGATIVE,
    };

    // Initialize horizontal PWM
    TUYA_CALL_ERR_RETURN(tkl_pwm_init(SERVO_PWM_HORIZONTAL, &cfg_x));
    TUYA_CALL_ERR_RETURN(tkl_pwm_start(SERVO_PWM_HORIZONTAL));

    // Initialize vertical PWM
    TUYA_CALL_ERR_RETURN(tkl_pwm_init(SERVO_PWM_VERTICAL, &cfg_y));
    TUYA_CALL_ERR_RETURN(tkl_pwm_start(SERVO_PWM_VERTICAL));

    PR_DEBUG("Servo initialized on channels %d (horizontal) and %d (vertical) with frequency %dHz", 
        SERVO_PWM_HORIZONTAL, SERVO_PWM_VERTICAL, SERVO_PWM_FREQ);

    s_servo_horizontal_angle = SERVO_ANGLE_CENTER_HORI;
    s_servo_vertical_angle = SERVO_ANGLE_CENTER_VERT;
    
    // Initialize servo states
    s_servo_vertical_state = SERVO_POS_CENTER_VERT;
    s_servo_horizontal_state = SERVO_POS_CENTER_HORI;
    
    // Initialize auto center timer
    s_auto_center_enabled = TRUE;
    app_servo_start_auto_center_timer();

    return OPRT_OK;
}

// Cleanup function to stop timer (can be called on exit)
VOID app_servo_cleanup(VOID)
{
    app_servo_stop_auto_center_timer();
}

VOID app_servo_move(SERVO_ACTION_E action)
{
    PR_DEBUG("servo action: %s (%d)", app_servo_action_to_string(action), action);

    // Add bounds checking
    if (action >= SERVO_MAX) {
        PR_ERR("Invalid servo action: %s (%d) (max: %d)", app_servo_action_to_string(action), action, SERVO_MAX - 1);
        return;
    }
    
    // Update action time for auto center functionality
    app_servo_update_action_time();

    switch (action) {
        case SERVO_UP:
            PR_DEBUG("Moving servo UP with smooth transition");
            app_servo_smooth_move_vertical(SERVO_UP);
            break;
        case SERVO_DOWN:
            PR_DEBUG("Moving servo DOWN with smooth transition");
            app_servo_smooth_move_vertical(SERVO_DOWN);
            break;
        case SERVO_LEFT:
            PR_DEBUG("Moving servo LEFT with smooth transition");
            app_servo_smooth_move_horizontal(SERVO_LEFT);
            break;
        case SERVO_RIGHT:
            PR_DEBUG("Moving servo RIGHT with smooth transition");
            app_servo_smooth_move_horizontal(SERVO_RIGHT);
            break;
        case SERVO_NOD:
            PR_DEBUG("Moving servo NOD");
            app_servo_nod();
            break;
        case SERVO_CLOCKWISE:
            PR_DEBUG("Moving servo CLOCKWISE");
            app_servo_clockwise();
            break;
        case SERVO_ANTICLOCKWISE:
            PR_DEBUG("Moving servo ANTICLOCKWISE");
            app_servo_anticlockwise();
            break;
        case SERVO_CENTER:
            PR_DEBUG("Moving servo CENTER");
            app_servo_center();
            break;
        default:
            PR_ERR("Unsupported servo action: %s (%d)", app_servo_action_to_string(action), action);
            break;
    }
    
    PR_DEBUG("Servo action %s (%d) completed", app_servo_action_to_string(action), action);
}