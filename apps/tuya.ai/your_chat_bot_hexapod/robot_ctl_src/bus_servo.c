#include "bus_servo.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// 全局串口发送函数指针
serial_send_func_t g_serial_send_func = NULL;

// 初始化总线舵机系统
void bus_servo_init(serial_send_func_t serial_send_func) {
    g_serial_send_func = serial_send_func;
    LOG_INFO("BusServo initialized");
}

// 创建总线舵机对象
BusServo_t bus_servo_create(uint8_t id) {
    BusServo_t servo;
    servo.id = id;
    servo.current_pwm = 1500;  // 默认中心位置
    servo.offset = 0;
    servo.current_angle = 0.0f;
    servo.min_angle = -60.0f;
    servo.max_angle = 60.0f;
    servo.reversed = false;
    return servo;
}

// 将角度转换为PWM值
int bus_servo_angle_to_pwm(float angle, int offset) {
    // 角度范围：-60到60度
    // PWM范围：500到2500，中心值1500
    // 每度约等于16.67个PWM单位
    float pwm_float = 1500.0f + (angle * 16.67f) + offset;
    
    // 限制PWM范围
    if (pwm_float < 500.0f) pwm_float = 500.0f;
    if (pwm_float > 2500.0f) pwm_float = 2500.0f;
    
    return (int)roundf(pwm_float);
}

// 将PWM值转换为角度
float bus_servo_pwm_to_angle(int pwm, int offset) {
    // 移除偏移量
    float adjusted_pwm = pwm - offset;
    
    // 转换为角度
    float angle = (adjusted_pwm - 1500.0f) / 16.67f;
    
    // 限制角度范围
    if (angle < -60.0f) angle = -60.0f;
    if (angle > 60.0f) angle = 60.0f;
    
    return angle;
}

// 发送单舵机控制指令
void bus_servo_send_command(uint8_t servo_id, int pwm, uint16_t time) {
    if (g_serial_send_func == NULL) {
        LOG_ERROR("Serial send function not initialized");
        return;
    }
    
    char command[32];
    snprintf(command, sizeof(command), "#%03dP%04dT%04d!", 
             servo_id, pwm, time);
    
    g_serial_send_func(command);
}

// 设置舵机角度
void bus_servo_set_angle(BusServo_t* servo, float angle, uint16_t time) {
    if (servo == NULL) return;
    
    // 限制角度范围
    if (angle < servo->min_angle) angle = servo->min_angle;
    if (angle > servo->max_angle) angle = servo->max_angle;
    
    // 应用反向
    if (servo->reversed) {
        angle = -angle;
    }
    
    // 转换为PWM
    int pwm = bus_servo_angle_to_pwm(angle, servo->offset);
    
    // 发送指令
    bus_servo_send_command(servo->id, pwm, time);
    
    // 更新状态
    servo->current_pwm = pwm;
    servo->current_angle = angle;
    
    LOG_DEBUG("Servo %d: angle=%.2f, pwm=%d", servo->id, angle, pwm);
}

// 获取当前舵机角度
float bus_servo_get_angle(const BusServo_t* servo) {
    if (servo == NULL) return 0.0f;
    return servo->current_angle;
}

// 获取舵机参数
void bus_servo_get_parameter(const BusServo_t* servo, int* offset, 
                            float* min_angle, float* max_angle, bool* reversed) {
    if (servo == NULL) return;
    
    if (offset) *offset = servo->offset;
    if (min_angle) *min_angle = servo->min_angle;
    if (max_angle) *max_angle = servo->max_angle;
    if (reversed) *reversed = servo->reversed;
}

// 设置舵机参数
void bus_servo_set_parameter(BusServo_t* servo, int offset, bool update, uint16_t time) {
    if (servo == NULL) return;
    
    servo->offset = offset;
    
    if (update) {
        // 重新计算当前角度对应的PWM值
        int pwm = bus_servo_angle_to_pwm(servo->current_angle, servo->offset);
        bus_servo_send_command(servo->id, pwm, time);
        servo->current_pwm = pwm;
    }
    
    LOG_DEBUG("Servo %d parameters updated: offset=%d", servo->id, offset);
}

// 初始化批量控制对象
void bus_servo_batch_init(BusServoBatch_t* batch) {
    if (batch == NULL) return;
    
    batch->buffer[0] = '{';
    batch->buffer[1] = 'G';
    batch->buffer[2] = '0';
    batch->buffer[3] = '0';
    batch->buffer[4] = '0';
    batch->buffer[5] = '0';
    batch->current_len = 6;
    batch->command_count = 0;
}

// 添加单舵机指令到批量控制
bool bus_servo_batch_add_command(BusServoBatch_t* batch, uint8_t servo_id, int pwm, uint16_t time) {
    if (batch == NULL || batch->command_count >= MAX_BATCH_COMMANDS) {
        return false;
    }
    
    // 检查缓冲区空间
    char temp_command[32];
    int temp_len = snprintf(temp_command, sizeof(temp_command), "#%03dP%04dT%04d!", 
                           servo_id, pwm, time);
    
    if (batch->current_len + temp_len >= sizeof(batch->buffer) - 2) { // 预留结束符空间
        return false;
    }
    
    // 添加指令到缓冲区
    strcat(batch->buffer, temp_command);
    batch->current_len += temp_len;
    batch->command_count++;
    
    return true;
}

// 发送批量控制指令
void bus_servo_batch_send(BusServoBatch_t* batch) {
    if (batch == NULL || g_serial_send_func == NULL) {
        return;
    }
    
    // 添加结束符
    strcat(batch->buffer, "}");
    
    // 发送指令
    g_serial_send_func(batch->buffer);
    
    LOG_DEBUG("Batch command sent: %d servos", batch->command_count);
}
