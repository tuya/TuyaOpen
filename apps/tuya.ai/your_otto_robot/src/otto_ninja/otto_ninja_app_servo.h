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
#endif // OTTO_NINJA_APP_SERVO_H

