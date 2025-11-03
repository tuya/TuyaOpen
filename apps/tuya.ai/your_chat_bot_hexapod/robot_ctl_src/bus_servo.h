//
// 总线舵机控制核心模块
// 支持串口协议：#000P1500T1000!
//

#pragma once

#include <stdint.h>
#include <stdbool.h>

// 舵机控制结构体
typedef struct {
    uint8_t id;         // 舵机ID
    int current_pwm;    // 当前PWM值
    int offset;         // 校准偏移量
    float current_angle; // 当前角度
    float min_angle;    // 最小角度
    float max_angle;    // 最大角度
    bool reversed;      // 是否反向
} BusServo_t;

// 批量控制结构体
#define MAX_BATCH_COMMANDS 18 // 最多18个舵机
typedef struct {
    char buffer[512];   // 存储批量指令的缓冲区
    int current_len;    // 当前缓冲区长度
    int command_count;  // 当前指令数量
} BusServoBatch_t;

// 串口发送函数类型定义
typedef void (*serial_send_func_t)(const char* data);

// 全局串口发送函数指针
extern serial_send_func_t g_serial_send_func;

// 函数声明

/**
 * @brief 初始化总线舵机系统
 * @param serial_send_func 串口发送函数指针
 */
void bus_servo_init(serial_send_func_t serial_send_func);

/**
 * @brief 创建总线舵机对象
 * @param id 舵机ID
 * @return 创建的舵机对象
 */
BusServo_t bus_servo_create(uint8_t id);

/**
 * @brief 设置舵机角度
 * @param servo 舵机对象指针
 * @param angle 目标角度（-60到60度）
 * @param time 运动时间（毫秒）
 */
void bus_servo_set_angle(BusServo_t* servo, float angle, uint16_t time);

/**
 * @brief 获取当前舵机角度
 * @param servo 舵机对象指针
 * @return 当前角度值
 */
float bus_servo_get_angle(const BusServo_t* servo);

/**
 * @brief 获取舵机参数
 * @param servo 舵机对象指针
 * @param offset 输出参数：偏移量
 * @param min_angle 输出参数：最小角度
 * @param max_angle 输出参数：最大角度
 * @param reversed 输出参数：是否反向
 */
void bus_servo_get_parameter(const BusServo_t* servo, int* offset, 
                            float* min_angle, float* max_angle, bool* reversed);

/**
 * @brief 设置舵机参数
 * @param servo 舵机对象指针
 * @param offset 偏移量
 * @param update 是否立即更新舵机位置
 * @param time 运动时间（毫秒）
 */
void bus_servo_set_parameter(BusServo_t* servo, int offset, bool update, uint16_t time);

/**
 * @brief 将角度转换为PWM值
 * @param angle 角度值
 * @param offset 偏移量
 * @return PWM值
 */
int bus_servo_angle_to_pwm(float angle, int offset);

/**
 * @brief 将PWM值转换为角度
 * @param pwm PWM值
 * @param offset 偏移量
 * @return 角度值
 */
float bus_servo_pwm_to_angle(int pwm, int offset);

/**
 * @brief 发送单舵机控制指令
 * @param servo_id 舵机ID
 * @param pwm PWM值
 * @param time 运动时间
 */
void bus_servo_send_command(uint8_t servo_id, int pwm, uint16_t time);

/**
 * @brief 初始化批量控制对象
 * @param batch 批量控制对象指针
 */
void bus_servo_batch_init(BusServoBatch_t* batch);

/**
 * @brief 添加单舵机指令到批量控制
 * @param batch 批量控制对象指针
 * @param servo_id 舵机ID
 * @param pwm PWM值
 * @param time 运动时间
 * @return true 成功，false 失败（缓冲区满）
 */
bool bus_servo_batch_add_command(BusServoBatch_t* batch, uint8_t servo_id, int pwm, uint16_t time);

/**
 * @brief 发送批量控制指令
 * @param batch 批量控制对象指针
 */
void bus_servo_batch_send(BusServoBatch_t* batch);
