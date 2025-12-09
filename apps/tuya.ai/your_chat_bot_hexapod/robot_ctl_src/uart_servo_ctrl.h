#ifndef __UART_SERVO_CTRL_H
#define __UART_SERVO_CTRL_H


#include "tuya_cloud_types.h"

OPERATE_RET servo_uart_init(VOID);

void ctrl_single_servo(UINT8_T id,UINT32_T pwm_duty,UINT32_T time);

OPERATE_RET read_all_servo_status(UINT8_T idx);

void release_all_servo_power(VOID);

void  robot_walking_status_set(UINT8_T status ,UINT32_T step);

void robot_move_step_set(UINT32_T step);

#endif