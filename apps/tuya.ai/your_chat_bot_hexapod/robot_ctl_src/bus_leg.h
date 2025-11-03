//
// 腿部控制模块
// 包含正/逆运动学计算和坐标转换
//

#pragma once

#include "base.h"
#include "bus_servo.h"
#include "servo_mapping.h"
#include <stdint.h>
#include <stdbool.h>

// 腿部控制结构体
typedef struct {
    uint8_t leg_index;           // 腿部编号 (0-5)
    Point3D_t root_position;     // 腿部根部位置（世界坐标系）
    Point3D_t tip_position;      // 足端位置（世界坐标系）
    Point3D_t tip_position_local; // 足端位置（局部坐标系）
    float joint_angles[3];       // 三个关节角度
    BusServo_t servos[3];        // 三个舵机对象
    bool is_initialized;         // 是否已初始化
} BusLeg_t;

// 机械参数常量
#define LEG_ROOT_TO_JOINT1 20.75f    // 根部到关节1距离 (mm)
#define LEG_JOINT1_TO_JOINT2 28.0f   // 关节1到关节2距离 (mm)
#define LEG_JOINT2_TO_JOINT3 42.6f   // 关节2到关节3距离 (mm)
#define LEG_JOINT3_TO_TIP 89.07f     // 关节3到足端距离 (mm)

// 函数声明

/**
 * @brief 创建腿部控制对象
 * @param leg_index 腿部编号 (0-5)
 * @param root_position 腿部根部位置
 * @return 创建的腿部对象
 */
BusLeg_t bus_leg_create(uint8_t leg_index, const Point3D_t* root_position);

/**
 * @brief 初始化腿部控制对象
 * @param leg 腿部对象指针
 * @return true 成功，false 失败
 */
bool bus_leg_init(BusLeg_t* leg);

/**
 * @brief 将世界坐标转换为局部坐标
 * @param leg 腿部对象指针
 * @param world_pos 世界坐标
 * @return 局部坐标
 */
Point3D_t bus_leg_translate_to_local(const BusLeg_t* leg, const Point3D_t* world_pos);

/**
 * @brief 将局部坐标转换为世界坐标
 * @param leg 腿部对象指针
 * @param local_pos 局部坐标
 * @return 世界坐标
 */
Point3D_t bus_leg_translate_to_world(const BusLeg_t* leg, const Point3D_t* local_pos);

/**
 * @brief 设置关节角度
 * @param leg 腿部对象指针
 * @param angles 三个关节角度数组
 * @param time 运动时间（毫秒）
 * @return true 成功，false 失败
 */
bool bus_leg_set_joint_angle(BusLeg_t* leg, const float angles[3], uint16_t time);

/**
 * @brief 移动足端到指定位置
 * @param leg 腿部对象指针
 * @param target_pos 目标位置（世界坐标系）
 * @param time 运动时间（毫秒）
 * @return true 成功，false 失败
 */
bool bus_leg_move_tip(BusLeg_t* leg, const Point3D_t* target_pos, uint16_t time);

/**
 * @brief 获取当前足端位置（世界坐标系）
 * @param leg 腿部对象指针
 * @return 足端位置
 */
Point3D_t bus_leg_get_tip_position(const BusLeg_t* leg);

/**
 * @brief 获取当前足端位置（局部坐标系）
 * @param leg 腿部对象指针
 * @return 足端位置
 */
Point3D_t bus_leg_get_tip_position_local(const BusLeg_t* leg);

/**
 * @brief 获取指定关节的舵机对象
 * @param leg 腿部对象指针
 * @param joint_index 关节编号 (0-2)
 * @return 舵机对象指针，失败返回NULL
 */
BusServo_t* bus_leg_get_servo(BusLeg_t* leg, uint8_t joint_index);

/**
 * @brief 强制重置足端位置（用于校准）
 * @param leg 腿部对象指针
 * @param new_position 新的足端位置
 */
void bus_leg_force_reset_tip_position(BusLeg_t* leg, const Point3D_t* new_position);

// 内部函数（供模块内部使用）

/**
 * @brief 正运动学计算
 * @param leg 腿部对象指针
 * @param angles 关节角度数组
 * @return 足端位置（局部坐标系）
 */
Point3D_t _forward_kinematics(const BusLeg_t* leg, const float angles[3]);

/**
 * @brief 逆运动学计算
 * @param leg 腿部对象指针
 * @param target_pos 目标位置（局部坐标系）
 * @param angles 输出关节角度数组
 * @return true 成功，false 失败
 */
bool _inverse_kinematics(const BusLeg_t* leg, const Point3D_t* target_pos, float angles[3]);

/**
 * @brief 执行腿部运动
 * @param leg 腿部对象指针
 * @param target_pos 目标位置
 * @param time 运动时间
 * @return true 成功，false 失败
 */
bool _move(BusLeg_t* leg, const Point3D_t* target_pos, uint16_t time);
