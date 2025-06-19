//--------------------------------------------------------------
//-- Oscillator.c
//-- Generate sinusoidal oscillations in the servos
//--------------------------------------------------------------
//-- Original work (c) Juan Gonzalez-Gomez (Obijuan), Dec 2011
//-- GPL license
//-- Ported to Tuya AI development board by [txp666], 2025
//--------------------------------------------------------------
/**
 * @file example_pwm.c
 * @brief PWM driver example for Tuya IoT projects.
 *
 * This file provides an example implementation of a PWM (Pulse Width Modulation) driver using the Tuya SDK.
 * It demonstrates the configuration and usage of PWM for controlling a servo motor, showing the Otto robot
 * movements library. The example demonstrates various servo-based movements for a small humanoid robot.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_pwm.h"
#include "oscillator.h"
#include "otto_movements.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
// Otto机器人舵机引脚定义
#define PIN_LEFT_LEG   TUYA_PWM_NUM_0
#define PIN_RIGHT_LEG  TUYA_PWM_NUM_1
#define PIN_LEFT_FOOT  TUYA_PWM_NUM_2
#define PIN_RIGHT_FOOT TUYA_PWM_NUM_3

#define TASK_PWM_PRIORITY THREAD_PRIO_2
#define TASK_PWM_SIZE     4096

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

void otto_power_on()
{
    PR_DEBUG("开始初始化Otto机器人...");

    // 初始化Otto机器人
    otto_init(PIN_LEFT_LEG, PIN_RIGHT_LEG, PIN_LEFT_FOOT, PIN_RIGHT_FOOT);

    // 设置舵机微调值 (如果舵机零位不准确，可以在这里调整)
    otto_set_trims(0, 0, 0, 0);

    // 启用舵机速度限制，防止舵机运动过快
    otto_enable_servo_limit(SERVO_LIMIT_DEFAULT);

    // 回到初始位置
    otto_home();
    // tal_system_sleep(1000);

    PR_DEBUG("Otto初始化完成");
}
/**
 * @brief otto_Show
 *
 * @return none
 */
static void otto_Show()
{
    PR_DEBUG("开始初始化Otto机器人...");

    // 初始化Otto机器人
    otto_init(PIN_LEFT_LEG, PIN_RIGHT_LEG, PIN_LEFT_FOOT, PIN_RIGHT_FOOT);

    // 设置舵机微调值 (如果舵机零位不准确，可以在这里调整)
    otto_set_trims(0, 0, 0, 0);

    // 启用舵机速度限制，防止舵机运动过快
    otto_enable_servo_limit(SERVO_LIMIT_DEFAULT);

    // 回到初始位置
    otto_home();
    tal_system_sleep(1000);

    PR_DEBUG("Otto initialized,starting to show...");

    // 演示一系列动作
    PR_DEBUG("otto_walk");
    otto_walk(4, 1000, FORWARD); // 向前走4步
    tal_system_sleep(500);

    PR_DEBUG("otto_turn");
    otto_turn(4, 1000, LEFT); // 向左转4步
    tal_system_sleep(500);

    PR_DEBUG("otto_swing");
    otto_swing(4, 1000, 20); // 左右摇摆4次
    tal_system_sleep(500);

    PR_DEBUG("otto_up_down");
    otto_up_down(4, 1000, 20); // 上下运动4次
    tal_system_sleep(500);

    PR_DEBUG("otto_bend");
    otto_bend(2, 1000, LEFT); // 向左弯腰2次
    tal_system_sleep(500);

    PR_DEBUG("otto_jitter");
    otto_jitter(4, 500, 20); // 颤抖4次
    tal_system_sleep(500);

    // 演示其他动作
    PR_DEBUG("otto_moonwalker");
    otto_moonwalker(4, 1000, 20, LEFT);
    tal_system_sleep(500);

    PR_DEBUG("otto_jump");
    otto_jump(2, 1000);
    tal_system_sleep(500);

    // PR_DEBUG("演示舵机自由移动");
    //  通过MoveServos可以直接控制所有舵机到指定位置
    // int positions[SERVO_COUNT] = {110, 70, 120, 60};
    // otto_move_servos(1000, positions);
    tal_system_sleep(1000);

    // 回到初始位置
    PR_DEBUG("otto_home");
    otto_home();
    tal_system_sleep(1000);

    PR_DEBUG("oto_show complete.");

    return;
}

enum ActionType {
    ACTION_WALK_F = 0,
    ACTION_WALK_B,
    ACTION_WALK_L,
    ACTION_WALK_R,
    ACTION_NONE,
    ACTION_SWING = 5,
    ACTION_UP_DOWN = 6,
    ACTION_BEND = 7,
    ACTION_JITTER = 8,
    ACTION_MOONWALKER = 9,
    ACTION_JUMP = 10,
    ACTION_SHOW = 11,
};

void otto_robot_dp_proc(uint32_t move_type)
{
    switch (move_type) {
    case ACTION_WALK_F:
        PR_DEBUG("Walking forward");
        otto_walk(2, 1000, FORWARD); // Walk forward 1 step
        break;

    case ACTION_WALK_B:
        PR_DEBUG("Walking backward");
        otto_walk(2, 1000, BACKWARD); // Walk backward 1 step
        break;

    case ACTION_WALK_L:
        PR_DEBUG("Walking left");
        otto_turn(2, 1000, LEFT); // Turn left 1 step
        break;

    case ACTION_WALK_R:
        PR_DEBUG("Walking right");
        otto_turn(2, 1000, RIGHT); // Turn right 1 step
        break;

    case ACTION_SWING:
        PR_DEBUG("Swinging");
        otto_swing(4, 1000, 20); // Swing 4 times
        break;

    case ACTION_UP_DOWN:
        PR_DEBUG("Moving up and down");
        otto_up_down(4, 1000, 20); // Move up and down 4 times
        break;

    case ACTION_BEND:
        PR_DEBUG("Bending");
        otto_bend(2, 1000, LEFT); // Bend left 2 times
        break;

    case ACTION_JITTER:
        PR_DEBUG("Jittering");
        otto_jitter(4, 500, 20); // Jitter 4 times
        break;

    case ACTION_MOONWALKER:
        PR_DEBUG("Performing moonwalker");
        otto_moonwalker(4, 1000, 20, LEFT); // Moonwalk left 4 times
        break;

    case ACTION_JUMP:
        PR_DEBUG("Jumping");
        otto_jump(2, 1000); // Jump 2 times
        break;

    case ACTION_SHOW:
        PR_DEBUG("Performing Show");
        otto_Show();
        break;

    case ACTION_NONE:
        // 设置舵机微调值 (如果舵机零位不准确，可以在这里调整)
        otto_set_trims(0, 0, 0, 0);
        otto_home();
        // tal_system_sleep(1000);
        PR_DEBUG("otto_home");
        break;
    }
    // 设置舵机微调值 (如果舵机零位不准确，可以在这里调整)
    otto_set_trims(0, 0, 0, 0);
    otto_home();
    // tal_system_sleep(1000);
    PR_DEBUG("otto_home");
}
