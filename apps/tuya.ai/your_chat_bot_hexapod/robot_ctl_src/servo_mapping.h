//
// 舵机ID映射配置
// 六足机器人18个舵机的ID分配
//

#pragma once

#include <stdint.h>
#include <stdbool.h>

// 六足机器人配置
#define NUM_LEGS 6           // 六条腿
#define NUM_JOINTS_PER_LEG 3 // 每条腿3个关节
#define NUM_SERVOS (NUM_LEGS * NUM_JOINTS_PER_LEG) // 总共18个舵机

// 腿部编号定义
typedef enum {
    LEG_1 = 0,  // 右前腿
    LEG_2 = 1,  // 左前腿
    LEG_3 = 2,  // 右中腿
    LEG_4 = 3,  // 左中腿
    LEG_5 = 4,  // 右后腿
    LEG_6 = 5   // 左后腿
} leg_index_t;

// 关节编号定义
typedef enum {
    JOINT_ROOT = 0,    // 根部关节（髋关节）
    JOINT_MIDDLE = 1,  // 中间关节（膝关节）
    JOINT_TIP = 2      // 末端关节（踝关节）
} joint_index_t;

// 舵机ID映射表
// 格式：[腿部编号][关节编号] = 舵机ID
extern const uint8_t SERVO_ID_MAP[NUM_LEGS][NUM_JOINTS_PER_LEG];

// 函数声明

/**
 * @brief 根据腿部和关节编号获取舵机ID
 * @param leg 腿部编号 (0-5)
 * @param joint 关节编号 (0-2)
 * @return 舵机ID，如果无效则返回0xFF
 */
uint8_t servo_mapping_get_id(uint8_t leg, uint8_t joint);

/**
 * @brief 检查舵机ID是否有效
 * @param servo_id 舵机ID
 * @return true 有效，false 无效
 */
bool servo_mapping_is_valid_id(uint8_t servo_id);

/**
 * @brief 根据舵机ID获取腿部和关节编号
 * @param servo_id 舵机ID
 * @param leg 输出参数：腿部编号
 * @param joint 输出参数：关节编号
 * @return true 成功，false 失败（无效ID）
 */
bool servo_mapping_get_leg_joint(uint8_t servo_id, uint8_t* leg, uint8_t* joint);

/**
 * @brief 获取指定腿部的所有舵机ID
 * @param leg 腿部编号 (0-5)
 * @param servo_ids 输出数组，至少3个元素
 * @return 成功获取的舵机数量
 */
int servo_mapping_get_leg_servos(uint8_t leg, uint8_t servo_ids[3]);

/**
 * @brief 获取所有舵机ID
 * @param servo_ids 输出数组，至少18个元素
 * @return 舵机总数
 */
int servo_mapping_get_all_servos(uint8_t servo_ids[18]);

/**
 * @brief 获取腿部名称字符串
 * @param leg 腿部编号 (0-5)
 * @return 腿部名称字符串
 */
const char* servo_mapping_get_leg_name(uint8_t leg);

/**
 * @brief 获取关节名称字符串
 * @param joint 关节编号 (0-2)
 * @return 关节名称字符串
 */
const char* servo_mapping_get_joint_name(uint8_t joint);
