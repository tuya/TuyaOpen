#ifndef __SERVO_H__
#define __SERVO_H__

#include "tuya_cloud_types.h"

#define SERVO_NUM                  2
#define PWM_UP_FRONT                  TUYA_PWM_NUM_0  // 上舵机
#define PWM_DOWN_FRONT                 TUYA_PWM_NUM_1  // 下舵机


typedef enum {
    SERVO_UP_FRONT  = 0,  // 上舵机
    SERVO_DOWN_FRONT = 1,  // 下舵机
    SERVO_ID_MAX
} SERVO_ID_E;

OPERATE_RET servo_hardware_init(void);
OPERATE_RET servo_set_angle(int servo_id, float angle);
void servo_action_shake(void);
void servo_action_nod(void);
void servo_action_shake_loop(void);
void servo_shake_stop(void);
VOID_T servo_shake_thread(VOID_T *arg);
VOID_T servo_shake_start(void);

void servo_headsmoothmove(int8_t startangle, int8_t endangle ,int16_t duration_ms , int8_t steps);
void servo_bodysmoothmove(int8_t startangle, int8_t endangle ,int16_t duration_ms , int8_t steps);
void head_down(void);
void head_up(void);
void head_left(void);
void head_right(void);

#endif