#pragma once

#include "base.h"
#include <stddef.h>

// 运动表函数声明
const MovementTable_t* get_standby_table(void);
const MovementTable_t* get_forward_table(void);
const MovementTable_t* get_backward_table(void);
const MovementTable_t* get_turn_left_table(void);
const MovementTable_t* get_turn_right_table(void);
const MovementTable_t* get_climb_table(void);
const MovementTable_t* get_descend_table(void);
const MovementTable_t* get_turn_left_fast_table(void);
const MovementTable_t* get_turn_right_fast_table(void);

// 机械参数常量
#define LEG_ROOT_TO_JOINT1 20.75f
#define LEG_JOINT1_TO_JOINT2 28.0f
#define LEG_JOINT2_TO_JOINT3 42.6f
#define LEG_JOINT3_TO_TIP 89.07f

// 腿部安装位置参数
#define LEG_MOUNT_LEFT_RIGHT_X 29.87f
#define LEG_MOUNT_OTHER_X 22.41f
#define LEG_MOUNT_OTHER_Y 55.41f
