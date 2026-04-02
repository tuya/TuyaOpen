#ifndef OTTO_NINJA_APP_SERVO_H
#define OTTO_NINJA_APP_SERVO_H

#include <stdint.h>
#include <stdbool.h>
#include "tkl_pwm.h"  // Tuya PWM definitions

#ifndef ARM_HEAD_ENABLE
#define ARM_HEAD_ENABLE 0
#endif

// Note: These values are actually TUYA_PWM_NUM_* enum values and can be used directly as PWM channel numbers
#define SERVO_LEFT_LEG_PIN     TUYA_PWM_NUM_0      // Left ankle servo -> TUYA_PWM_NUM_0
#define SERVO_RIGHT_LEG_PIN    TUYA_PWM_NUM_1      // Right ankle servo -> TUYA_PWM_NUM_1
#define SERVO_LEFT_FOOT_PIN    TUYA_PWM_NUM_2      // Left foot servo -> TUYA_PWM_NUM_2
#define SERVO_RIGHT_FOOT_PIN   TUYA_PWM_NUM_3      // Right foot servo -> TUYA_PWM_NUM_3
#if ARM_HEAD_ENABLE == 1
#define SERVO_LEFT_ARM_PIN     TUYA_PWM_NUM_4      // Left arm servo -> TUYA_PWM_NUM_4
#define SERVO_RIGHT_ARM_PIN    TUYA_PWM_NUM_7      // Right arm servo -> TUYA_PWM_NUM_7
#define SERVO_HEAD_PIN         TUYA_PWM_NUM_5      // Head servo -> TUYA_PWM_NUM_5 todo
#endif

// ==================== Platform Interface Functions ====================
/**
 * Get system running time (milliseconds)
 */
uint32_t get_millis(void);

/**
 * Delay function (milliseconds)
 */
void delay_ms(uint32_t ms);

// ==================== Initialization ====================
/**
 * Initialize Tuya platform interface
 * Called once at system startup to initialize PWM channel state management array
 * Must be called before calling other PWM-related functions
 */
void platform_tuya_init(void);

/**
 * Initialize servo control system
 */
void servo_control_init(void);

/**
 * @brief Call platform_tuya_init() then servo_control_init() (same first step as main_init).
 */
void otto_ninja_servo_pwm_stack_init(void);

/**
 * @brief Attach one servo PWM (50Hz), same implementation as otto_ninja_app_servo.c.
 */
void servo_attach(uint8_t pin, uint16_t min_pulse, uint16_t max_pulse);

/**
 * @brief Set angle 0..180; drives tkl_pwm_info_set + tkl_pwm_start like Otto ninja.
 */
void servo_write(uint8_t pin, uint16_t angle);

/**
 * @brief Same as servo_write but stops PWM first (recover after WiFi/RF mux glitches).
 * @note Prefer for fixed-duty outputs (e.g. N20 on one pin); avoid for fast multi-servo motion.
 */
void servo_pwm_force_update(uint8_t pin, uint16_t angle);

/**
 * @brief Stop PWM on pin (tkl_pwm_stop + duty 0).
 */
void servo_detach(uint8_t pin);

/**
 * Smooth servo angle transition
 * Gradually transition from current angle to target angle to avoid sudden changes
 * 
 * @param pin Servo pin
 * @param target_angle Target angle (0-180 degrees)
 * @param step_delay_ms Delay time per step (milliseconds), controls speed
 * @param step_size Angle increment per step, controls smoothness
 */
void servo_write_smooth(uint8_t pin, uint16_t target_angle, uint16_t step_delay_ms, uint16_t step_size);

void main_init(void);
void main_loop(void);

void robot_set_roll(void);
void robot_set_walk(void);
#if ARM_HEAD_ENABLE == 1
void robot_left_arm_up(void);
void robot_left_arm_down(void);
#endif
void robot_right_arm_up(void);
void robot_right_arm_down(void);
void robot_roll_control(int8_t joystick_x, int8_t joystick_y);
void robot_walk_forward(int8_t joystick_x, int8_t joystick_y);
void robot_walk_backward(int8_t joystick_x, int8_t joystick_y);
void robot_rotate_spot(bool direction);
void robot_rotate_spot_stop(void);
void robot_rotate_spot_update(void);
#endif // OTTO_NINJA_APP_SERVO_H

