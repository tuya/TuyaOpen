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


void otto_robot_task(void *arg);
void otto_robot_pwm_task(void *arg);
void otto_robot_demo_task(void *arg);
void otto_robot_action(otto_action_e action);


void otto_robot_init_with_arms(void);
void otto_robot_init_basic(void);
void otto_demo_arm_movements(void);
void otto_demo_coordinated_movements(void);
void otto_demo_basic_movements(void);
void otto_robot_main_demo(void);
void otto_robot_basic_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* __OTTO_ROBOT_MAIN_H__ */ 