#include "bus_leg.h"
#include "debug.h"
#include <math.h>
#include <string.h>

// 创建腿部控制对象
BusLeg_t bus_leg_create(uint8_t leg_index, const Point3D_t* root_position) {
    BusLeg_t leg;
    leg.leg_index = leg_index;
    leg.root_position = *root_position;
    leg.tip_position = *root_position;
    leg.tip_position_local = point3d_create(0, 0, 0);
    leg.is_initialized = false;
    
    // 初始化关节角度
    for (int i = 0; i < 3; i++) {
        leg.joint_angles[i] = 0.0f;
    }
    
    return leg;
}

// 初始化腿部控制对象
bool bus_leg_init(BusLeg_t* leg) {
    if (leg == NULL) {
        LOG_ERROR("Leg pointer is NULL");
        return false;
    }
    
    if (leg->leg_index >= NUM_LEGS) {
        LOG_ERROR("Invalid leg index: %d", leg->leg_index);
        return false;
    }
    
    // 创建三个舵机对象
    for (int i = 0; i < 3; i++) {
        uint8_t servo_id = servo_mapping_get_id(leg->leg_index, i);
        leg->servos[i] = bus_servo_create(servo_id);
    }
    
    leg->is_initialized = true;
    LOG_DEBUG("Leg %d initialized", leg->leg_index);
    return true;
}

// 将世界坐标转换为局部坐标
Point3D_t bus_leg_translate_to_local(const BusLeg_t* leg, const Point3D_t* world_pos) {
    if (leg == NULL || world_pos == NULL) {
        return point3d_create(0, 0, 0);
    }
    
    return point3d_subtract(world_pos, &leg->root_position);
}

// 将局部坐标转换为世界坐标
Point3D_t bus_leg_translate_to_world(const BusLeg_t* leg, const Point3D_t* local_pos) {
    if (leg == NULL || local_pos == NULL) {
        return point3d_create(0, 0, 0);
    }
    
    return point3d_add(local_pos, &leg->root_position);
}

// 正运动学计算
Point3D_t _forward_kinematics(const BusLeg_t* leg, const float angles[3]) {
    if (leg == NULL || angles == NULL) {
        return point3d_create(0, 0, 0);
    }
    
    float a1 = angles[0] * M_PI / 180.0f;  // 根部关节角度
    float a2 = angles[1] * M_PI / 180.0f;  // 中间关节角度
    float a3 = angles[2] * M_PI / 180.0f;  // 末端关节角度
    
    // 计算各关节位置
    float x1 = LEG_ROOT_TO_JOINT1 * cosf(a1);
    float y1 = LEG_ROOT_TO_JOINT1 * sinf(a1);
    float z1 = 0;
    
    float x2 = x1 + LEG_JOINT1_TO_JOINT2 * cosf(a1 + a2);
    float y2 = y1 + LEG_JOINT1_TO_JOINT2 * sinf(a1 + a2);
    float z2 = z1;
    
    float x3 = x2 + LEG_JOINT2_TO_JOINT3 * cosf(a1 + a2 + a3);
    float y3 = y2 + LEG_JOINT2_TO_JOINT3 * sinf(a1 + a2 + a3);
    float z3 = z2;
    
    float x4 = x3 + LEG_JOINT3_TO_TIP * cosf(a1 + a2 + a3);
    float y4 = y3 + LEG_JOINT3_TO_TIP * sinf(a1 + a2 + a3);
    float z4 = z3;
    
    return point3d_create(x4, y4, z4);
}

// 逆运动学计算
bool _inverse_kinematics(const BusLeg_t* leg, const Point3D_t* target_pos, float angles[3]) {
    if (leg == NULL || target_pos == NULL || angles == NULL) {
        return false;
    }
    
    float x = target_pos->x;
    float y = target_pos->y;
    float z = target_pos->z;
    
    // 计算距离
    float distance = sqrtf(x*x + y*y + z*z);
    
    // 检查是否在可达范围内
    float max_reach = LEG_ROOT_TO_JOINT1 + LEG_JOINT1_TO_JOINT2 + LEG_JOINT2_TO_JOINT3 + LEG_JOINT3_TO_TIP;
    if (distance > max_reach) {
        LOG_WARN("Target position out of reach: distance=%.2f, max=%.2f", distance, max_reach);
        return false;
    }
    
    // 根部关节角度
    angles[0] = atan2f(y, x) * 180.0f / M_PI;
    
    // 计算中间关节和末端关节角度
    float r = sqrtf(x*x + y*y);
    float s = z;
    
    float L1 = LEG_JOINT1_TO_JOINT2;
    float L2 = LEG_JOINT2_TO_JOINT3;
    float L3 = LEG_JOINT3_TO_TIP;
    
    // 使用余弦定理计算角度
    float cos_a2 = (L1*L1 + L2*L2 - (r*r + s*s)) / (2*L1*L2);
    if (cos_a2 < -1.0f || cos_a2 > 1.0f) {
        LOG_WARN("Invalid angle calculation: cos_a2=%.3f", cos_a2);
        return false;
    }
    
    angles[1] = acosf(cos_a2) * 180.0f / M_PI - 180.0f;
    
    float cos_a3 = (L2*L2 + L3*L3 - (r*r + s*s)) / (2*L2*L3);
    if (cos_a3 < -1.0f || cos_a3 > 1.0f) {
        LOG_WARN("Invalid angle calculation: cos_a3=%.3f", cos_a3);
        return false;
    }
    
    angles[2] = acosf(cos_a3) * 180.0f / M_PI - 180.0f;
    
    return true;
}

// 设置关节角度
bool bus_leg_set_joint_angle(BusLeg_t* leg, const float angles[3], uint16_t time) {
    if (leg == NULL || angles == NULL || !leg->is_initialized) {
        return false;
    }
    
    // 更新关节角度
    for (int i = 0; i < 3; i++) {
        leg->joint_angles[i] = angles[i];
    }
    
    // 计算新的足端位置
    leg->tip_position_local = _forward_kinematics(leg, angles);
    leg->tip_position = bus_leg_translate_to_world(leg, &leg->tip_position_local);
    
    // 设置舵机角度
    for (int i = 0; i < 3; i++) {
        bus_servo_set_angle(&leg->servos[i], angles[i], time);
    }
    
    LOG_DEBUG("Leg %d joint angles set: [%.2f, %.2f, %.2f]", 
              leg->leg_index, angles[0], angles[1], angles[2]);
    
    return true;
}

// 移动足端到指定位置
bool bus_leg_move_tip(BusLeg_t* leg, const Point3D_t* target_pos, uint16_t time) {
    if (leg == NULL || target_pos == NULL || !leg->is_initialized) {
        return false;
    }
    
    // 转换为局部坐标
    Point3D_t local_target = bus_leg_translate_to_local(leg, target_pos);
    
    // 计算逆运动学
    float angles[3];
    if (!_inverse_kinematics(leg, &local_target, angles)) {
        LOG_ERROR("Failed to calculate inverse kinematics for leg %d", leg->leg_index);
        return false;
    }
    
    // 设置关节角度
    return bus_leg_set_joint_angle(leg, angles, time);
}

// 获取当前足端位置（世界坐标系）
Point3D_t bus_leg_get_tip_position(const BusLeg_t* leg) {
    if (leg == NULL) {
        return point3d_create(0, 0, 0);
    }
    return leg->tip_position;
}

// 获取当前足端位置（局部坐标系）
Point3D_t bus_leg_get_tip_position_local(const BusLeg_t* leg) {
    if (leg == NULL) {
        return point3d_create(0, 0, 0);
    }
    return leg->tip_position_local;
}

// 获取指定关节的舵机对象
BusServo_t* bus_leg_get_servo(BusLeg_t* leg, uint8_t joint_index) {
    if (leg == NULL || joint_index >= 3) {
        return NULL;
    }
    return &leg->servos[joint_index];
}

// 强制重置足端位置（用于校准）
void bus_leg_force_reset_tip_position(BusLeg_t* leg, const Point3D_t* new_position) {
    if (leg == NULL || new_position == NULL) {
        return;
    }
    
    leg->tip_position = *new_position;
    leg->tip_position_local = bus_leg_translate_to_local(leg, new_position);
    
    LOG_DEBUG("Leg %d tip position reset to (%.2f, %.2f, %.2f)", 
              leg->leg_index, new_position->x, new_position->y, new_position->z);
}

// 执行腿部运动
bool _move(BusLeg_t* leg, const Point3D_t* target_pos, uint16_t time) {
    return bus_leg_move_tip(leg, target_pos, time);
}
