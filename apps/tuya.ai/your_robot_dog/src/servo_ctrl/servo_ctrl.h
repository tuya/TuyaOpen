#ifndef __SERVO_CTRL_H__
#define __SERVO_CTRL_H__

#include "tuya_cloud_types.h"
#include "tuya_robot_actions.h"

/* Servo count and PWM configuration */
#define SERVO_NUM                  4
#define PWM_LEFT_FRONT                  TUYA_PWM_NUM_0  // Front-left servo
#define PWM_RIGHT_FRONT                 TUYA_PWM_NUM_1  // Front-right servo
#define PWM_LEFT_REAR                   TUYA_PWM_NUM_2  // Rear-left servo
#define PWM_RIGHT_REAR                  TUYA_PWM_NUM_3  // Rear-right servo

#define ACTION_SPEED_FAST  150 // Fast
#define ACTION_SPEED_SLOW_FAST  300 // Medium-fast
#define ACTION_SPEED_MID   500 // Medium
#define ACTION_SPEED_SLOW  800 // Slow

#define SWING_FORWARD    15
#define SWING_BACKWARD   20
#define JUMP_REAR_LEG    40
#define JUMP_FRONT_LEG   20

#define SWING_ANGLE_L   50
#define SWING_ANGLE_M   30
#define SWING_ANGLE_S   10

typedef struct robot_action_node {
    TUYA_ROBOT_ACTION_E action;
    struct robot_action_node* next;
} robot_action_node_t;

typedef struct {
    robot_action_node_t* head;
    robot_action_node_t* tail;
} robot_action_list_t;


OPERATE_RET servo_hardware_init(VOID);
OPERATE_RET servo_set_angle(int servo_id, float angle);
OPERATE_RET robot_action_add_action(TUYA_ROBOT_ACTION_E action);
OPERATE_RET robot_action_thread_init(void);

#endif