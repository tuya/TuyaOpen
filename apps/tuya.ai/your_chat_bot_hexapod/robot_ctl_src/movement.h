#pragma once

#include "base.h"
#include "bus_leg.h"
#include "bus_servo.h"
#include <stdint.h>
#include <stdbool.h>

// 运动模式枚举
typedef enum {
    MOVEMENT_MODE_STANDBY = 0,    // 待机模式
    MOVEMENT_MODE_FORWARD,        // 前进模式
    MOVEMENT_MODE_BACKWARD,       // 后退模式
    MOVEMENT_MODE_TURN_LEFT,      // 左转模式
    MOVEMENT_MODE_TURN_RIGHT,     // 右转模式
    MOVEMENT_MODE_CLIMB,          // 攀爬模式
    MOVEMENT_MODE_DESCEND,        // 下降模式
    MOVEMENT_MODE_TURN_LEFT_FAST, // 快速左转模式
    MOVEMENT_MODE_TURN_RIGHT_FAST,// 快速右转模式
    MOVEMENT_MODE_COUNT           // 模式总数
} MovementMode_t;

// 运动控制结构体
typedef struct {
    MovementMode_t current_mode;           // 当前运动模式
    bool is_moving;                       // 是否正在运动
    uint32_t step_start_time;             // 当前步骤开始时间
    int current_step;                     // 当前步骤索引
    int current_entry;                    // 当前入口点索引
    BusLeg_t legs[NUM_LEGS];              // 六条腿的控制对象
    BusServoBatch_t batch_controller;     // 批量舵机控制器
    // uint32_t (*time_func)(void);          // 时间函数指针
} Movement_t;

// 运动表函数声明
const MovementTable_t* standby_table(void);
const MovementTable_t* forward_table(void);
const MovementTable_t* backward_table(void);
const MovementTable_t* turn_left_table(void);
const MovementTable_t* turn_right_table(void);
const MovementTable_t* climb_table(void);
const MovementTable_t* descend_table(void);
const MovementTable_t* turn_left_fast_table(void);
const MovementTable_t* turn_right_fast_table(void);

// 运动控制函数声明

/**
 * @brief 创建运动控制对象
 * @param time_func 时间函数指针
 * @return 创建的运动控制对象
 */
Movement_t movement_create(void);

/**
 * @brief 初始化运动控制对象
 * @param movement 运动控制对象指针
 * @return true 成功，false 失败
 */
bool movement_init(Movement_t* movement);

/**
 * @brief 设置运动模式
 * @param movement 运动控制对象指针
 * @param new_mode 新的运动模式
 * @return true 成功，false 失败
 */
bool movement_set_mode(Movement_t* movement, MovementMode_t new_mode);

/**
 * @brief 更新运动状态
 * @param movement 运动控制对象指针
 * @return true 成功，false 失败
 */
bool movement_update(Movement_t* movement);

/**
 * @brief 停止运动
 * @param movement 运动控制对象指针
 */
void movement_stop(Movement_t* movement);

/**
 * @brief 获取当前运动模式
 * @param movement 运动控制对象指针
 * @return 当前运动模式
 */
MovementMode_t movement_get_mode(const Movement_t* movement);

/**
 * @brief 检查是否正在运动
 * @param movement 运动控制对象指针
 * @return true 正在运动，false 未运动
 */
bool movement_is_moving(const Movement_t* movement);

/**
 * @brief 获取指定腿的控制对象
 * @param movement 运动控制对象指针
 * @param leg_index 腿索引
 * @return 腿控制对象指针
 */
BusLeg_t* movement_get_leg(Movement_t* movement, uint8_t leg_index);

/**
 * @brief 设置腿部根部位置
 * @param movement 运动控制对象指针
 * @param leg_index 腿索引
 * @param position 根部位置
 */
void movement_set_leg_root_position(Movement_t* movement, uint8_t leg_index, const Point3D_t* position);

/**
 * @brief 获取当前步骤索引
 * @param movement 运动控制对象指针
 * @return 当前步骤索引
 */
int movement_get_current_step(const Movement_t* movement);

/**
 * @brief 获取当前入口点索引
 * @param movement 运动控制对象指针
 * @return 当前入口点索引
 */
int movement_get_current_entry(const Movement_t* movement);
