/*
 * Some code is adapted from OttoNinja_APP.ino
 * Original code URL: https://github.com/OttoDIY/OttoNinja/blob/master/examples/App/OttoNinja_APP/OttoNinja_APP.ino
 * Original authors: cparrapa Brian, etc.
 * Licensed under the original code's licenses: CC-BY-SA 4.0 and GPLv3
 * Redistribution of this code must include information about the Otto DIY website, and derivative works must adopt the same licenses and make all files publicly available
 * Ported to Tuya AI development board by [Robben], 2025
 */



#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include "otto_ninja_main.h"
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_pwm.h"                    // Tuya PWM definitions
#include "tkl_output.h"                 // Tuya GPIO definitions
#include "otto_ninja_app_servo.h"      // Pin definitions and function declarations


// ==================== Tuya Platform Interface Implementation ====================

#define SERVO_PWM_FREQUENCY    50      // Servo PWM frequency: 50Hz (20ms period)
#define SERVO_PWM_PERIOD_US    20000   // PWM period: 20000 microseconds (20ms)

#if ARM_HEAD_ENABLE == 1
    #define MAX_SERVO_COUNT        7      // Maximum number of servos
#else
    #define MAX_SERVO_COUNT        4       // Maximum number of servos
#endif

// PWM channel state management
typedef struct {
    bool initialized;
    TUYA_PWM_NUM_E pwm_id;
} pwm_channel_state_t;

static pwm_channel_state_t pwm_channels[MAX_SERVO_COUNT];

/**
 * @brief Get corresponding PWM channel based on pin number
 * 
 * Note: SERVO_*_PIN values are already TUYA_PWM_NUM_* enum values
 * So the pin parameter value is actually the PWM channel number and can be directly converted and used
 */
static TUYA_PWM_NUM_E pin_to_pwm_id(uint8_t pin)
{
    return (TUYA_PWM_NUM_E)pin;
}

/**
 * Value mapping function (corresponds to Arduino's map function)
 */
 static int map_value(int value, int in_min, int in_max, int out_min, int out_max)
 {
     return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
 }

/**
 * @brief Get system running time (milliseconds)
 */
uint32_t get_millis(void)
{
    return tal_system_get_millisecond();
}

/**
 * @brief Delay function (milliseconds)
 * 
 * Note: Tuya SDK (T5AI platform/bk_system) already provides delay_ms function
 * To avoid duplicate definition linking errors, weak symbol attribute is used here
 * If SDK provides delay_ms, linker will prioritize SDK's version
 * If SDK doesn't provide it, use the implementation here
 */
__attribute__((weak)) void delay_ms(uint32_t ms)
{
    tal_system_sleep(ms);
}

/**
 * @brief Set GPIO to output mode
 * Note: Tuya SDK's PWM initialization will automatically configure GPIO, this function is empty implementation
 */
void gpio_set_output(uint8_t pin)
{
    // Tuya SDK's PWM initialization will automatically configure GPIO
}

/**
 * @brief Initialize PWM channel
 */
bool pwm_init(uint8_t pin, uint32_t freq_hz)
{
    TUYA_PWM_NUM_E pwm_id = pin_to_pwm_id(pin);
    
    if (pwm_id >= TUYA_PWM_NUM_MAX) {
        return false;
    }
    
    // Check if already initialized
    for (int i = 0; i < MAX_SERVO_COUNT; i++) {
        if (pwm_channels[i].initialized && pwm_channels[i].pwm_id == pwm_id) {
            return true;  // Already initialized
        }
    }
    
    // Configure PWM parameters
    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .duty = 0,              // Initial duty cycle is 0
        .frequency = freq_hz,   // Frequency
        .polarity = TUYA_PWM_NEGATIVE,  // Polarity
    };
    
    // Initialize PWM
    OPERATE_RET ret = tkl_pwm_init(pwm_id, &pwm_cfg);
    if (ret != OPRT_OK) {
        return false;
    }
    
    // Record initialization state
    for (int i = 0; i < MAX_SERVO_COUNT; i++) {
        if (!pwm_channels[i].initialized) {
            pwm_channels[i].initialized = true;
            pwm_channels[i].pwm_id = pwm_id;
            break;
        }
    }
    
    return true;
}



/**
 * @brief Stop PWM output
 */
void pwm_stop(uint8_t pin)
{
    TUYA_PWM_NUM_E pwm_id = pin_to_pwm_id(pin);
    
    if (pwm_id >= TUYA_PWM_NUM_MAX) {
        return;
    }
    
    // Stop PWM output
    tkl_pwm_stop(pwm_id);
    
    // Optional: Set duty cycle to 0
    tkl_pwm_duty_set(pwm_id, 0);
}

/**
 * @brief Initialize platform interface
 * Called once at system startup to initialize PWM channel state management array
 */
void platform_tuya_init(void)
{
    // Initialize PWM channel state array
    for (int i = 0; i < MAX_SERVO_COUNT; i++) {
        pwm_channels[i].initialized = false;
        pwm_channels[i].pwm_id = TUYA_PWM_NUM_MAX;
    }
}

// ==================== PWM Parameters ====================
#define SERVO_MIN_PULSE        500
#define SERVO_MAX_PULSE        2500

// ==================== Calibration Parameters ====================
#define LFFWRS     20      // Left foot forward rotation speed (demo_ai_otto calibrated)
#define RFFWRS     12      // Right foot forward rotation speed (demo_ai_otto calibrated)
#define LFBWRS     5       // Left foot backward rotation speed (demo_ai_otto calibrated)
#define RFBWRS     5       // Right foot backward rotation speed (demo_ai_otto calibrated)

#define LA0        60      // Left leg standing position
#define RA0        120     // Right leg standing position
#define LA1        180     // Left leg roll position
#define RA1        0       // Right leg roll position
#define LATL       100     // Left leg left tilt walk position
#define RATL       175     // Right leg left tilt walk position
#define LATR       5       // Left leg right tilt walk position
#define RATR       80      // Right leg right tilt walk position

// ==================== Data Structures ====================
// Servo state structure
typedef struct {
    uint8_t pin;           // GPIO pin
    bool attached;          // Whether attached
    uint16_t current_angle; // Current angle
    uint16_t min_pulse;     // Minimum pulse width (microseconds)
    uint16_t max_pulse;     // Maximum pulse width (microseconds)
} servo_t;

// ==================== Global Variables ====================
// Global servo objects
static servo_t servos[MAX_SERVO_COUNT];

// Time control variable
static uint32_t currentmillis1 = 0;

// Rotate spot state variables
static bool sg_rotate_spot_active = false;
/** When true, robot_rotate_spot_update() runs the continuous foot motion (false during init delays). */
static bool sg_rotate_spot_update_ready = false;
static uint16_t sg_right_foot_angle = 90;
static uint32_t sg_last_foot_update = 0;
static uint32_t sg_angle_accumulator = 0;

// ==================== Utility Functions ====================

/**
 * Convert angle to PWM pulse width (microseconds)
 */
static uint16_t angle_to_pulse(uint16_t angle, uint16_t min_pulse, uint16_t max_pulse)
{
    if (angle > 180) angle = 180;
    return min_pulse + (angle * (max_pulse - min_pulse)) / 180;
}

/**
 * Get servo object index
 */
static int get_servo_index(uint8_t pin)
{
    for (int i = 0; i < MAX_SERVO_COUNT; i++) {
        if (servos[i].pin == pin) {
            return i;
        }
    }
    return -1;
}

// ==================== Servo Control Functions ====================

/**
 * Attach servo to specified pin (corresponds to Arduino's attach function)
 */
void servo_attach(uint8_t pin, uint16_t min_pulse, uint16_t max_pulse)
{
    int idx = get_servo_index(pin);
    if (idx < 0) {
        // Find free slot
        for (int i = 0; i < MAX_SERVO_COUNT; i++) {
            if (servos[i].pin == 0xFF) {  // 0xFF means unused
                idx = i;
                break;
            }
        }
        if (idx < 0) return;  // No free slot
    }
    
    servos[idx].pin = pin;
    servos[idx].min_pulse = min_pulse;
    servos[idx].max_pulse = max_pulse;
    servos[idx].attached = true;
    servos[idx].current_angle = 90;  // Default 90 degrees
    
    // Initialize PWM
    gpio_set_output(pin);
    pwm_init(pin, 50);  // 50Hz
}

/**
 * Set servo angle (corresponds to Arduino's write function)
 */
void servo_write(uint8_t pin, uint16_t angle)
{
    int idx = get_servo_index(pin);
    if (idx < 0 || !servos[idx].attached) {
        return;
    }
    
    servos[idx].current_angle = angle;
    uint16_t pulse_width = angle_to_pulse(angle, servos[idx].min_pulse, servos[idx].max_pulse);
    
    // Calculate duty cycle (percentage)
    //uint32_t duty_percent = (pulse_width * 100) / SERVO_PWM_PERIOD_US;
    
    // Print angle and duty cycle information
   // PR_NOTICE("servo_write: pin=%d, angle=%d, pulse_width=%d us, duty_percent=%d%%", 
           //   pin, angle, pulse_width, duty_percent);
    


    TUYA_PWM_NUM_E pwm_id = pin_to_pwm_id(pin);
    
    if (pwm_id >= TUYA_PWM_NUM_MAX) {
        return;
    }
    
    // Limit pulse width range
    if (pulse_width > SERVO_PWM_PERIOD_US) {
        pulse_width = SERVO_PWM_PERIOD_US;
    }
    
    // Convert pulse width (microseconds) to duty cycle
    // Tuya SDK's duty range is 1-10000, corresponding to 0.01%-100%
    // duty = (pulse_width_us / SERVO_PWM_PERIOD_US) * 10000
    uint32_t duty = (pulse_width * 10000) / SERVO_PWM_PERIOD_US;
    
    // Ensure duty is within valid range (1-10000)
    if (duty < 1) {
        duty = 1;
    } else if (duty > 10000) {
        duty = 10000;
    }
    
    // Set duty cycle
    tkl_pwm_duty_set(pwm_id, duty);

    // Calculate duty percentage (duty range 1-10000 corresponds to 0.01%-100%)
//     float duty_percent = (float)duty / 100.0f;
//    PR_NOTICE("servo_write: pin=%d, angle=%d, pulse_width=%d us, duty=%d (%.2f%%)", 
//             pin, angle, pulse_width, duty, duty_percent);
    // Ensure PWM is running
    tkl_pwm_start(pwm_id);
}

/**
 * Detach servo connection (corresponds to Arduino's detach function)
 */
void servo_detach(uint8_t pin)
{
    int idx = get_servo_index(pin);
    if (idx < 0) return;
    
    pwm_stop(pin);
    servos[idx].attached = false;
}

void servo_write_smooth(uint8_t pin, uint16_t target_angle, uint16_t step_delay_ms, uint16_t step_size)
{
    int idx = get_servo_index(pin);

    if (idx < 0) {
        servo_attach(pin, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
        idx = get_servo_index(pin);
        if (idx < 0) {
            return;
        }
    }

    if (!servos[idx].attached) {
        servo_attach(pin, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
    }

    if (target_angle > 180) {
        target_angle = 180;
    }

    uint16_t current_angle = servos[idx].current_angle;
    if (current_angle == target_angle) {
        return;
    }

    int16_t direction = (target_angle > current_angle) ? 1 : -1;
    int16_t angle_diff = (target_angle > current_angle) ? (target_angle - current_angle) : (current_angle - target_angle);
    uint16_t steps = (angle_diff + step_size - 1) / step_size;
    if (steps == 0) {
        steps = 1;
    }

    for (uint16_t i = 0; i < steps; i++) {
        uint16_t step_angle = current_angle + (direction * step_size * (i + 1));

        if (direction > 0 && step_angle > target_angle) {
            step_angle = target_angle;
        }
        if (direction < 0 && step_angle < target_angle) {
            step_angle = target_angle;
        }

        servo_write(pin, step_angle);
        if (i < steps - 1) {
            delay_ms(step_delay_ms);
        }
    }

    if (servos[idx].current_angle != target_angle) {
        servo_write(pin, target_angle);
    }
}


// ==================== Initialization Functions ====================

/**
 * Initialize servo control system
 */
 void servo_control_init(void)
 {
     PR_NOTICE("servo_control_init");
     // Initialize servo array
     for (int i = 0; i < MAX_SERVO_COUNT; i++) {
         servos[i].pin = 0xFF;  // 0xFF means unused
         servos[i].attached = false;
         servos[i].current_angle = 90;
         servos[i].min_pulse = SERVO_MIN_PULSE;
         servos[i].max_pulse = SERVO_MAX_PULSE;
     }
     
     currentmillis1 = 0;
 }
 



// ==================== Robot Motion Functions ====================

/**
 * Initialize robot to home position
 */
 void robot_home(void)
 {
     PR_NOTICE("robot_home");
   
  
 #if ARM_HEAD_ENABLE == 1
     // Attach arm servos
     servo_attach(SERVO_LEFT_ARM_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_attach(SERVO_RIGHT_ARM_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_attach(SERVO_HEAD_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
    

     servo_write(SERVO_LEFT_ARM_PIN, 180);
     servo_write(SERVO_RIGHT_ARM_PIN, 0);
     servo_write(SERVO_HEAD_PIN, 90);

     delay_ms(400);
#endif
     
     // Attach leg servos
     servo_attach(SERVO_LEFT_LEG_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_attach(SERVO_RIGHT_LEG_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     
      // Set leg positions
      servo_write(SERVO_LEFT_LEG_PIN, LA0);
      servo_write(SERVO_RIGHT_LEG_PIN, RA0); 
      delay_ms(1000);

     // Attach feet servos
     servo_attach(SERVO_LEFT_FOOT_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_attach(SERVO_RIGHT_FOOT_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);

     // Set foot positions
     servo_write(SERVO_LEFT_FOOT_PIN, 90);
     servo_write(SERVO_RIGHT_FOOT_PIN, 90);
     delay_ms(400);



     // Detach all servos
     servo_detach(SERVO_LEFT_FOOT_PIN);
     servo_detach(SERVO_RIGHT_FOOT_PIN);
     servo_detach(SERVO_LEFT_LEG_PIN);
     servo_detach(SERVO_RIGHT_LEG_PIN);

     #if ARM_HEAD_ENABLE == 1
     servo_detach(SERVO_LEFT_ARM_PIN);
     servo_detach(SERVO_RIGHT_ARM_PIN);
     servo_detach(SERVO_HEAD_PIN);
     #endif

 }
 
 /**
  * Set walk mode
  */
 void robot_set_walk(void)
 {
    PR_NOTICE("robot_set_walk");
#if ARM_HEAD_ENABLE == 1
     servo_attach(SERVO_LEFT_ARM_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_attach(SERVO_RIGHT_ARM_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_write_smooth(SERVO_LEFT_ARM_PIN, 90, 15, 2);
     servo_write_smooth(SERVO_RIGHT_ARM_PIN, 90, 15, 2);
     delay_ms(200);
#endif
     servo_attach(SERVO_LEFT_LEG_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_attach(SERVO_RIGHT_LEG_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_write_smooth(SERVO_LEFT_LEG_PIN, LA0, 15, 2);
     servo_write_smooth(SERVO_RIGHT_LEG_PIN, RA0, 15, 2);
     delay_ms(100);
     
#if ARM_HEAD_ENABLE == 1
     servo_attach(SERVO_LEFT_ARM_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_attach(SERVO_RIGHT_ARM_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_write_smooth(SERVO_LEFT_ARM_PIN, 180, 15, 2);
     servo_write_smooth(SERVO_RIGHT_ARM_PIN, 0, 15, 2);
#endif
 }
 
 /**
  * Set roll mode
  */
 void robot_set_roll(void)
 {
    PR_NOTICE("robot_set_roll");
#if ARM_HEAD_ENABLE == 1
     servo_attach(SERVO_LEFT_ARM_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_attach(SERVO_RIGHT_ARM_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_write_smooth(SERVO_LEFT_ARM_PIN, 90, 15, 2);
     servo_write_smooth(SERVO_RIGHT_ARM_PIN, 90, 15, 2);
     delay_ms(200);
#endif
     
     servo_attach(SERVO_LEFT_LEG_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_attach(SERVO_RIGHT_LEG_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_write_smooth(SERVO_LEFT_LEG_PIN, LA1, 20, 3);
     servo_write_smooth(SERVO_RIGHT_LEG_PIN, RA1, 20, 3);
     delay_ms(100);
     
#if ARM_HEAD_ENABLE == 1
     servo_attach(SERVO_LEFT_ARM_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_attach(SERVO_RIGHT_ARM_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_write_smooth(SERVO_LEFT_ARM_PIN, 180, 15, 2);
     servo_write_smooth(SERVO_RIGHT_ARM_PIN, 0, 15, 2);
#endif
 }
 
 /**
  * Walk stop
  */
 void robot_walk_stop(void)
 {
     servo_write(SERVO_LEFT_FOOT_PIN, 90);
     servo_write(SERVO_RIGHT_FOOT_PIN, 90);
     servo_write(SERVO_LEFT_LEG_PIN, LA0);
     servo_write(SERVO_RIGHT_LEG_PIN, RA0);
 }
 
 /**
  * Roll stop
  */
 void robot_roll_stop(void)
 {
     servo_write(SERVO_LEFT_FOOT_PIN, 90);
     servo_write(SERVO_RIGHT_FOOT_PIN, 90);
     servo_detach(SERVO_LEFT_FOOT_PIN);
     servo_detach(SERVO_RIGHT_FOOT_PIN);
 }
 
 /**
  * Forward walk control
  * @param joystick_x Joystick X value (-100 to 100)
  * @param joystick_y Joystick Y value (-100 to 100, should be >0 for forward)
  */
 void robot_walk_forward(int8_t joystick_x, int8_t joystick_y)
 {
     if (joystick_y <= 0) return;  // Only process forward
     
     // Calibrated timing from demo_ai_otto.
     int lt = map_value(joystick_x, 100, -100, 300, 500);
     int rt = map_value(joystick_x, 100, -100, 400, 600);
    // PR_NOTICE("robot_walk_forward: lt=%d, rt=%d", lt, rt);
     // Calculate time intervals
     int interval1 = 250;
     int interval2 = 250 + rt;
     int interval3 = 250 + rt + 250;
     int interval4 = 250 + rt + 250 + lt;
     int interval5 = 250 + rt + 250 + lt + 50;
     
     uint32_t current_time = get_millis();
     
     // Reset loop timer
     if (current_time > currentmillis1 + interval5) {
         currentmillis1 = current_time;
     }
     
     uint32_t elapsed = get_millis() - currentmillis1;
     
     // Phase 1: Set ankles to right tilt position
     if (elapsed <= interval1) {
       // PR_NOTICE("robot_walk_forward1: elapsed=%d,interval1=%d", elapsed, interval1);
         servo_attach(SERVO_LEFT_LEG_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
         servo_attach(SERVO_RIGHT_LEG_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
         servo_attach(SERVO_RIGHT_FOOT_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
         servo_attach(SERVO_LEFT_FOOT_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
         
         servo_write(SERVO_LEFT_LEG_PIN, LATR);
         servo_write(SERVO_RIGHT_LEG_PIN, RATR);
     }
     
     elapsed = get_millis() - currentmillis1;
     // Phase 2: Right foot rotation
     if (elapsed >= interval1 && elapsed <= interval2) {
        //PR_NOTICE("robot_walk_forward2: elapsed=%d,interval2=%d", elapsed, interval2);

         servo_write(SERVO_RIGHT_FOOT_PIN, 90 - RFFWRS);
     }
     
     // Phase 3: Right foot stop, set ankles to left tilt position
     elapsed = get_millis() - currentmillis1;
     if (elapsed >= interval2 && elapsed <= interval3) {
       // PR_NOTICE("robot_walk_forward3: elapsed=%d,interval3=%d", elapsed, interval3);
         servo_detach(SERVO_RIGHT_FOOT_PIN);
         servo_write(SERVO_LEFT_LEG_PIN, LATL);
         servo_write(SERVO_RIGHT_LEG_PIN, RATL);
     }
     
     // Phase 4: Left foot rotation
     elapsed = get_millis() - currentmillis1;
     if (elapsed >= interval3 && elapsed <= interval4) {
        //PR_NOTICE("robot_walk_forward4: elapsed=%d,interval4=%d", elapsed, interval4);
         servo_write(SERVO_LEFT_FOOT_PIN, 90 + LFFWRS);
     }
     
     // Phase 5: Left foot stop
     elapsed = get_millis() - currentmillis1;
     if (elapsed >= interval4 && elapsed <= interval5) {
        //PR_NOTICE("robot_walk_forward5: elapsed=%d,interval5=%d", elapsed, interval5);

         servo_detach(SERVO_LEFT_FOOT_PIN);
     }
 }
 
 /**
  * Backward walk control
  * @param joystick_x Joystick X value (-100 to 100)
  * @param joystick_y Joystick Y value (-100 to 100, should be <0 for backward)
  */
 void robot_walk_backward(int8_t joystick_x, int8_t joystick_y)
 {
     if (joystick_y >= 0) return;  // Only process backward
     
     // Calibrated timing from demo_ai_otto.
     int lt = map_value(joystick_x, 100, -100, 150, 650);
     int rt = map_value(joystick_x, 100, -100, 750, 250);
     
     // Calculate time intervals
     int interval1 = 250;
     int interval2 = 250 + rt;
     int interval3 = 250 + rt + 250;
     int interval4 = 250 + rt + 250 + lt;
     int interval5 = 250 + rt + 250 + lt + 50;
     
     uint32_t current_time = get_millis();
     
     // Reset loop timer
     if (current_time > currentmillis1 + interval5) {
         currentmillis1 = current_time;
     }
     
     uint32_t elapsed = current_time - currentmillis1;
     
     // Phase 1: Set ankles to right tilt position
     if (elapsed <= interval1) {
        //PR_NOTICE("robot_walk_backward1: elapsed=%d,interval1=%d", elapsed, interval1);
         servo_attach(SERVO_LEFT_LEG_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
         servo_attach(SERVO_RIGHT_LEG_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
         servo_attach(SERVO_RIGHT_FOOT_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
         servo_attach(SERVO_LEFT_FOOT_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
         
         servo_write(SERVO_LEFT_LEG_PIN, LATR);
         servo_write(SERVO_RIGHT_LEG_PIN, RATR);
     }
     
     // Phase 2: Right foot rotation (backward direction)
     elapsed = get_millis() - currentmillis1;
     if (elapsed >= interval1 && elapsed <= interval2) {
       // PR_NOTICE("robot_walk_backward2: elapsed=%d,interval2=%d", elapsed, interval2);
         servo_write(SERVO_RIGHT_FOOT_PIN, 90 + RFBWRS);
     }
     
     // Phase 3: Right foot stop, set ankles to left tilt position
     elapsed = get_millis() - currentmillis1;
     if (elapsed >= interval2 && elapsed <= interval3) {
       // PR_NOTICE("robot_walk_backward3: elapsed=%d,interval3=%d", elapsed, interval3);
         servo_detach(SERVO_RIGHT_FOOT_PIN);
         servo_write(SERVO_LEFT_LEG_PIN, LATL);
         servo_write(SERVO_RIGHT_LEG_PIN, RATL);
     }
     
     // Phase 4: Left foot rotation (backward direction)
     elapsed = get_millis() - currentmillis1;
     if (elapsed >= interval3 && elapsed <= interval4) {
       // PR_NOTICE("robot_walk_backward4: elapsed=%d,interval4=%d", elapsed, interval4);
         servo_write(SERVO_LEFT_FOOT_PIN, 90 - LFBWRS);
     }
     
     // Phase 5: Left foot stop
     elapsed = get_millis() - currentmillis1;
     if (elapsed >= interval4 && elapsed <= interval5) {
        //PR_NOTICE("robot_walk_backward5: elapsed=%d,interval5=%d", elapsed, interval5);
         servo_detach(SERVO_LEFT_FOOT_PIN);
     }
 }
 
 /**
  * Roll mode control
  * @param joystick_x Joystick X value (-100 to 100)
  * @param joystick_y Joystick Y value (-100 to 100)
  */
 void robot_roll_control(int8_t joystick_x, int8_t joystick_y)
 {
     //PR_NOTICE("robot_roll_control: joystick_x=%d, joystick_y=%d", joystick_x, joystick_y);
     // If joystick is in center position, stop
     if (joystick_x >= -10 && joystick_x <= 10 && 
         joystick_y >= -10 && joystick_y <= 10) {
         robot_roll_stop();
         //PR_NOTICE("robot_roll_stop");
         return;
     }
     
     // Attach foot servos
     servo_attach(SERVO_LEFT_FOOT_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_attach(SERVO_RIGHT_FOOT_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     
     // Calculate left/right wheel speed
     int left_wheel_speed = map_value(joystick_y, 100, -100, 135, 45);
     int right_wheel_speed = map_value(joystick_y, 100, -100, 45, 135);
     
     // Calculate left/right turn offset
     int left_wheel_delta = map_value(joystick_x, 100, -100, 45, 0);
     int right_wheel_delta = map_value(joystick_x, 100, -100, 0, -45);
     //PR_NOTICE("robot_roll_control: left_wheel_speed=%d, right_wheel_speed=%d, left_wheel_delta=%d, right_wheel_delta=%d", left_wheel_speed, right_wheel_speed, left_wheel_delta, right_wheel_delta);
     // Set servo angle

     servo_write(SERVO_LEFT_FOOT_PIN, left_wheel_speed + left_wheel_delta);
     //PR_NOTICE("robot_roll_control: left_wheel_speed+left_wheel_delta=%d", left_wheel_speed+left_wheel_delta);
     servo_write(SERVO_RIGHT_FOOT_PIN, right_wheel_speed + right_wheel_delta);
     //PR_NOTICE("robot_roll_control: right_wheel_speed+right_wheel_delta=%d", right_wheel_speed+right_wheel_delta);
 }

void robot_rotate_spot(bool direction)
{
    PR_NOTICE("robot_rotate_spot: direction=%s, starting rotation", direction ? "right" : "left");

    /*
     * Set active before init delays so main_loop does not call robot_walk_stop() / roll control
     * in parallel (DP may run on another thread). update_ready stays false until init completes.
     */
    sg_rotate_spot_active = true;
    sg_rotate_spot_update_ready = false;

    if (get_mode_counter() != 0) {
        PR_NOTICE("robot_rotate_spot: Not in walk mode, switching to walk mode");
        robot_set_walk();
        delay_ms(500);
    }

    servo_attach(SERVO_LEFT_LEG_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
    servo_attach(SERVO_RIGHT_LEG_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
    servo_attach(SERVO_RIGHT_FOOT_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
    servo_attach(SERVO_LEFT_FOOT_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);

    servo_write(SERVO_LEFT_LEG_PIN, LATR);
    servo_write(SERVO_RIGHT_LEG_PIN, RATR);
    delay_ms(250);

    uint16_t start_angle = 90 - RFFWRS;
    servo_write(SERVO_RIGHT_FOOT_PIN, start_angle);
    delay_ms(500);

    sg_right_foot_angle = start_angle;
    sg_last_foot_update = get_millis();

    if (sg_right_foot_angle < 60) {
        sg_right_foot_angle = 60;
    }
    if (sg_right_foot_angle > 180) {
        sg_right_foot_angle = 180;
    }
    if (sg_right_foot_angle <= 120) {
        sg_angle_accumulator = (sg_right_foot_angle - 60) * 9000 / 60;
    } else {
        sg_angle_accumulator = 9000 + (sg_right_foot_angle - 120) * 9000 / 60;
    }

    sg_rotate_spot_update_ready = true;
}

void robot_rotate_spot_stop(void)
{
    PR_NOTICE("robot_rotate_spot_stop: Stopping rotation");
    sg_rotate_spot_active = false;
    sg_rotate_spot_update_ready = false;

    servo_write(SERVO_LEFT_LEG_PIN, LA0);
    servo_write(SERVO_RIGHT_LEG_PIN, RA0);
    servo_write(SERVO_RIGHT_FOOT_PIN, 90);

    servo_detach(SERVO_LEFT_LEG_PIN);
    servo_detach(SERVO_RIGHT_LEG_PIN);
    servo_detach(SERVO_RIGHT_FOOT_PIN);
    servo_detach(SERVO_LEFT_FOOT_PIN);
}

void robot_rotate_spot_update(void)
{
    if (!sg_rotate_spot_active) {
        return;
    }
    if (!sg_rotate_spot_update_ready) {
        return;
    }

    uint32_t current_time = get_millis();
    uint32_t elapsed = current_time - sg_last_foot_update;

    if (elapsed >= 3) {
        sg_last_foot_update = current_time;

        uint32_t increment = (elapsed * 15) / 3;
        if (increment < 1) {
            increment = 1;
        }
        if (increment > 50) {
            increment = 50;
        }

        sg_angle_accumulator += increment;
        if (sg_angle_accumulator >= 36000) {
            sg_angle_accumulator %= 36000;
        }

        uint32_t phase = sg_angle_accumulator % 36000;
        uint32_t quarter = phase / 9000;
        uint32_t pos_in_quarter = phase % 9000;

        switch (quarter) {
        case 0:
            sg_right_foot_angle = 60 + (pos_in_quarter * 60) / 9000;
            break;
        case 1:
            sg_right_foot_angle = 120 + (pos_in_quarter * 60) / 9000;
            break;
        case 2:
            sg_right_foot_angle = 180 - (pos_in_quarter * 60) / 9000;
            break;
        case 3:
            sg_right_foot_angle = 120 - (pos_in_quarter * 60) / 9000;
            break;
        default:
            sg_right_foot_angle = 90;
            break;
        }

        if (sg_right_foot_angle < 60) {
            sg_right_foot_angle = 60;
        }
        if (sg_right_foot_angle > 180) {
            sg_right_foot_angle = 180;
        }

        servo_write(SERVO_RIGHT_FOOT_PIN, sg_right_foot_angle);
    }
}
 
 /**
  * Left arm up
  */
#if ARM_HEAD_ENABLE == 1
 void robot_left_arm_up(void)
 {
     servo_attach(SERVO_LEFT_ARM_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_write(SERVO_LEFT_ARM_PIN, 90);
 }
 
 /**
  * Left arm down
  */
 void robot_left_arm_down(void)
 {
     servo_write(SERVO_LEFT_ARM_PIN, 180);
 }

 
 /**
  * Right arm up
  */
 void robot_right_arm_up(void)
 {
     servo_attach(SERVO_RIGHT_ARM_PIN, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
     servo_write(SERVO_RIGHT_ARM_PIN, 90);
 }
 
 /**
  * Right arm down
  */
 void robot_right_arm_down(void)
 {
     servo_write(SERVO_RIGHT_ARM_PIN, 0);
 }
 
 #endif




/**
 * Main loop example (corresponds to Arduino's loop function)
 */
void main_loop(void)
{
    // Read joystick and button states
    int8_t joystick_x = get_joystick_x();
    int8_t joystick_y = get_joystick_y();

    // Mode switching, check if previous mode and current mode are consistent, switch mode if inconsistent
    if(get_sport_mode_change()) {
        set_sport_mode_change(false);//Reset mode switch flag
        if(get_mode_counter() == 0) {

            robot_set_walk();
            PR_NOTICE("robot_set_walk");
        }
        else if(get_mode_counter() == 1) {

            robot_set_roll();
            PR_NOTICE("robot_set_roll");
        }
    }

    robot_rotate_spot_update();

    /* While spot rotation is armed or running, do not run walk/roll (may run on another thread). */
    if (sg_rotate_spot_active) {
        return;
    }

    // Execute different motion control based on mode
    if (get_mode_counter() == 0) {
        // Walk mode

        if (joystick_x >= -10 && joystick_x <= 10 && 
            joystick_y >= -10 && joystick_y <= 10) {
            robot_walk_stop();
           // PR_NOTICE("robot_walk_stop");
        }

        // Forward
        else if (joystick_y > 0) {
            //PR_NOTICE("robot_walk_forward: joystick_x=%d, joystick_y=%d", joystick_x, joystick_y);
            robot_walk_forward(joystick_x, joystick_y);
            //ninja_walk_forward_test(joystick_x, joystick_y);
        }
        // Backward
        else if (joystick_y < 0) {
           // PR_NOTICE("robot_walk_backward: joystick_x=%d, joystick_y=%d", joystick_x, joystick_y);
            robot_walk_backward(joystick_x, joystick_y);
        }
    }
    else if (get_mode_counter() == 1) {
        // Roll mode
       // PR_NOTICE("robot_roll_control: joystick_x=%d, joystick_y=%d", joystick_x, joystick_y);
        robot_roll_control(joystick_x, joystick_y);
    }

}

/**
 * Initialization example (corresponds to Arduino's setup function)
 */
void main_init(void)
{
    PR_NOTICE("main_init");
    // Initialize platform interface
    platform_tuya_init();
    // Initialize servo control system
    servo_control_init();
    
    // Robot returns to initial position
    robot_home();
    
    // Other initialization code...
}

