#include "servo_mapping.h"
#include "debug.h"
#include <stddef.h>

// 舵机ID映射表
// 格式：[腿部编号][关节编号] = 舵机ID
// 腿部编号：0=右前腿, 1=左前腿, 2=右中腿, 3=左中腿, 4=右后腿, 5=左后腿
// 关节编号：0=根部关节, 1=中间关节, 2=末端关节
const uint8_t SERVO_ID_MAP[NUM_LEGS][NUM_JOINTS_PER_LEG] = {
    // 右前腿 (LEG_1)
    {15, 14, 13},
    // 左前腿 (LEG_2)  
    {12, 11, 10},
    // 右中腿 (LEG_3)
    {18, 17, 16},
    // 左中腿 (LEG_4)
    {9, 8, 7},
    // 右后腿 (LEG_5)
    {3, 2, 1},
    // 左后腿 (LEG_6)
    {6, 5, 4}
};

// 根据腿部和关节编号获取舵机ID
uint8_t servo_mapping_get_id(uint8_t leg, uint8_t joint) {
    if (leg >= NUM_LEGS || joint >= NUM_JOINTS_PER_LEG) {
        LOG_ERROR("Invalid leg (%d) or joint (%d) index", leg, joint);
        return 0xFF;
    }
    
    return SERVO_ID_MAP[leg][joint];
}

// 检查舵机ID是否有效
bool servo_mapping_is_valid_id(uint8_t servo_id) {
    if (servo_id == 0 || servo_id > 18) {
        return false;
    }
    
    // 检查是否在映射表中
    for (int leg = 0; leg < NUM_LEGS; leg++) {
        for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
            if (SERVO_ID_MAP[leg][joint] == servo_id) {
                return true;
            }
        }
    }
    
    return false;
}

// 根据舵机ID获取腿部和关节编号
bool servo_mapping_get_leg_joint(uint8_t servo_id, uint8_t* leg, uint8_t* joint) {
    if (leg == NULL || joint == NULL) {
        return false;
    }
    
    for (int l = 0; l < NUM_LEGS; l++) {
        for (int j = 0; j < NUM_JOINTS_PER_LEG; j++) {
            if (SERVO_ID_MAP[l][j] == servo_id) {
                *leg = l;
                *joint = j;
                return true;
            }
        }
    }
    
    return false;
}

// 获取指定腿部的所有舵机ID
int servo_mapping_get_leg_servos(uint8_t leg, uint8_t servo_ids[3]) {
    if (servo_ids == NULL || leg >= NUM_LEGS) {
        return 0;
    }
    
    for (int i = 0; i < NUM_JOINTS_PER_LEG; i++) {
        servo_ids[i] = SERVO_ID_MAP[leg][i];
    }
    
    return NUM_JOINTS_PER_LEG;
}

// 获取所有舵机ID
int servo_mapping_get_all_servos(uint8_t servo_ids[18]) {
    if (servo_ids == NULL) {
        return 0;
    }
    
    int count = 0;
    for (int leg = 0; leg < NUM_LEGS; leg++) {
        for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
            servo_ids[count++] = SERVO_ID_MAP[leg][joint];
        }
    }
    
    return count;
}

// 获取腿部名称字符串
const char* servo_mapping_get_leg_name(uint8_t leg) {
    switch (leg) {
        case LEG_1: return "右前腿";
        case LEG_2: return "左前腿";
        case LEG_3: return "右中腿";
        case LEG_4: return "左中腿";
        case LEG_5: return "右后腿";
        case LEG_6: return "左后腿";
        default: return "未知腿部";
    }
}

// 获取关节名称字符串
const char* servo_mapping_get_joint_name(uint8_t joint) {
    switch (joint) {
        case JOINT_ROOT: return "根部关节";
        case JOINT_MIDDLE: return "中间关节";
        case JOINT_TIP: return "末端关节";
        default: return "未知关节";
    }
}
