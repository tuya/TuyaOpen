/**
 * @file otto_robot_main.h
 * @brief Otto robot main control header with arm support
 * @version 0.1
 * @date 2025-06-23
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 * Extended with arm support - Otto with 6 servos
 */

#ifndef __OTTO_ROBOT_MAIN_H__
#define __OTTO_ROBOT_MAIN_H__

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tuya_iot_dp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Otto robot actions enumeration
 */
typedef enum {
    ACTION_NONE = 0,
    ACTION_WALK_F,      // Walk forward
    ACTION_WALK_B,      // Walk backward  
    ACTION_WALK_L,      // Turn left
    ACTION_WALK_R,      // Turn right
    ACTION_JUMP,        // Jump
    ACTION_BEND_L,      // Bend left
    ACTION_BEND_R,      // Bend right
    ACTION_SHAKE_L,     // Shake left leg
    ACTION_SHAKE_R,     // Shake right leg
    ACTION_UP_DOWN,     // Up and down movement
    ACTION_SWING,       // Swing movement
    ACTION_HANDS_UP,    // Hands up (new)
    ACTION_HANDS_DOWN,  // Hands down (new)
    ACTION_WAVE_L,      // Left hand wave (new)
    ACTION_WAVE_R,      // Right hand wave (new)
    ACTION_WAVE_BOTH,   // Both hands wave (using otto_hand_wave with BOTH)
    ACTION_MAX
} otto_action_e;


/**
 * @brief Main Otto robot task function
 * @param arg Task parameter pointer
 */
void otto_robot_task(void *arg);

/**
 * @brief Otto robot PWM control task
 * @param arg Task parameter pointer
 */
void otto_robot_pwm_task(void *arg);

/**
 * @brief Otto robot demonstration task
 * @param arg Task parameter pointer
 */
void otto_robot_demo_task(void *arg);

/**
 * @brief Execute specified Otto robot action
 * @param action Action type to execute
 */
void otto_robot_action(otto_action_e action);

/**
 * @brief Initialize Otto robot with arms
 * Initialize 6-servo Otto robot including arm control functionality
 */
void otto_robot_init_with_arms(void);

/**
 * @brief Initialize basic Otto robot
 * Initialize 4-servo basic Otto robot without arms
 */
void otto_robot_init_basic(void);

/**
 * @brief Demonstrate arm movements
 * Show Otto robot's arm control capabilities
 */
void otto_demo_arm_movements(void);

/**
 * @brief Demonstrate coordinated movements
 * Show Otto robot's coordinated motion capabilities
 */
void otto_demo_coordinated_movements(void);

/**
 * @brief Demonstrate basic movements
 * Show Otto robot's basic motion capabilities
 */
void otto_demo_basic_movements(void);

/**
 * @brief Main Otto robot demonstration program
 * Execute complete Otto robot functionality demonstration
 */
void otto_robot_main_demo(void);

/**
 * @brief Basic Otto robot demonstration program
 * Execute basic Otto robot functionality demonstration
 */
void otto_robot_basic_demo(void);

/**
 * @brief Otto robot power-on initialization
 * Initialize all hardware and software components of Otto robot
 */
void otto_power_on(void);

/**
 * @brief Process cloud data point commands
 * @param dpobj Pointer to received data point object
 */
void otto_robot_dp_proc(dp_obj_recv_t *dpobj);

/**
 * @brief Execute specified robot action
 * @param move_type Action type (numeric form)
 */
void otto_robot_execute_action(uint32_t move_type);

/**
 * @brief Process data point commands in thread
 * @param move_type Action type
 */
void otto_robot_dp_proc_thread(uint32_t move_type);

/**
 * @brief Process step count data point
 * @param steps Step count setting
 */
void otto_robot_step_dp_proc(uint32_t steps);

/**
 * @brief Process speed data point
 * @param speed_type Speed type (0:slow, 1:normal, 2:fast)
 */
void otto_robot_speed_dp_proc(uint32_t speed_type);

/**
 * @brief Process audio mode data point
 * @param mode Audio mode (0-3 for different audio interaction modes)
 */
void otto_robot_audio_mode_dp_proc(uint32_t mode);

/**
 * @brief Set Otto robot step count
 * @param steps Number of steps
 */
void set_otto_steps(uint32_t steps);

/**
 * @brief Get Otto robot current step count
 * @return Current step count
 */
uint32_t get_otto_steps(void);

/**
 * @brief Set Otto robot speed
 * @param speed Speed value
 */
void set_otto_speed(uint32_t speed);

/**
 * @brief Get Otto robot current speed
 * @return Current speed value
 */
uint32_t get_otto_speed(void);

#ifdef __cplusplus
}
#endif

#endif /* __OTTO_ROBOT_MAIN_H__ */ 