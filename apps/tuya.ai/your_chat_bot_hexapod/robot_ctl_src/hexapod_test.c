#include "bus_servo.h"
#include "bus_leg.h"
#include "servo_mapping.h"
#include "movement.h"
#include "base.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "hexapod_test.h"

#define USR_UART_NUM      TUYA_UART_NUM_0


// 模拟串口发送函数
void mock_serial_send(const char* data) {
    tal_uart_write(USR_UART_NUM, (const uint8_t*)data, strlen(data));
}


// 测试总线舵机功能
void test_bus_servo(void) {
    LOG_INFO("=== 测试总线舵机功能 ===");
    
    // 初始化总线舵机系统
    bus_servo_init(mock_serial_send);
    
    // 创建舵机对象
    BusServo_t servo = bus_servo_create(1);
    
    // 测试角度设置
    LOG_INFO("设置舵机1角度为30度");
    bus_servo_set_angle(&servo, 30.0f, 1000);
    
    // 测试角度获取
    float angle = bus_servo_get_angle(&servo);
    LOG_INFO("当前角度: %.2f度", angle);
    
    // 测试参数设置
    LOG_INFO("设置舵机偏移量为5");
    bus_servo_set_parameter(&servo, 5, true, 500);
    
    // 测试批量控制
    BusServoBatch_t batch;
    bus_servo_batch_init(&batch);
    
    LOG_INFO("添加批量控制指令");
    bus_servo_batch_add_command(&batch, 1, 1500, 1000);
    bus_servo_batch_add_command(&batch, 2, 1600, 1000);
    bus_servo_batch_add_command(&batch, 3, 1700, 1000);
    
    bus_servo_batch_send(&batch);
    
    LOG_INFO("总线舵机测试完成");
}

// 测试腿部控制功能
void test_bus_leg(void) {
    LOG_INFO("=== 测试腿部控制功能 ===");
    
    // 初始化总线舵机系统
    bus_servo_init(mock_serial_send);
    
    // 创建腿部控制对象
    Point3D_t root_pos = point3d_create(0, 0, 0);
    BusLeg_t leg = bus_leg_create(0, &root_pos);
    
    // 初始化腿部控制
    if (!bus_leg_init(&leg)) {
        LOG_ERROR("腿部控制初始化失败");
        return;
    }
    
    // 测试正运动学
    float angles[3] = {0, 30, -60};
    Point3D_t tip_pos = _forward_kinematics(&leg, angles);
    LOG_INFO("正运动学结果: (%.2f, %.2f, %.2f)", tip_pos.x, tip_pos.y, tip_pos.z);
    
    // 测试逆运动学
    Point3D_t target_pos = point3d_create(50, 30, -20);
    float calc_angles[3];
    if (_inverse_kinematics(&leg, &target_pos, calc_angles)) {
        LOG_INFO("逆运动学结果: [%.2f, %.2f, %.2f]", 
                 calc_angles[0], calc_angles[1], calc_angles[2]);
    } else {
        LOG_ERROR("逆运动学计算失败");
    }
    
    // 测试腿部移动
    LOG_INFO("移动腿部到目标位置");
    if (bus_leg_move_tip(&leg, &target_pos, 1000)) {
        LOG_INFO("腿部移动成功");
    } else {
        LOG_ERROR("腿部移动失败");
    }
    
    LOG_INFO("腿部控制测试完成");
}

// 测试运动控制功能
void test_movement(void) {
    LOG_INFO("=== 测试运动控制功能 ===");
    
    // 初始化总线舵机系统
    bus_servo_init(mock_serial_send);
    
    // 创建运动控制对象
    Movement_t movement = movement_create();
    
    // 初始化运动控制
    if (!movement_init(&movement)) {
        LOG_ERROR("运动控制初始化失败");
        return;
    }
    
    // 测试设置运动模式
    LOG_INFO("设置前进模式");
    if (movement_set_mode(&movement, MOVEMENT_MODE_FORWARD)) {
        LOG_INFO("前进模式设置成功");
    } else {
        LOG_ERROR("前进模式设置失败");
    }
    
    // 模拟运动更新
    LOG_INFO("模拟运动更新");
    for (int i = 0; i < 5; i++) {
        if (movement_update(&movement)) {
            LOG_INFO("运动更新 %d 成功", i + 1);
        } else {
            LOG_ERROR("运动更新 %d 失败", i + 1);
        }
        tal_system_sleep(100);
    }
    
    // 测试停止运动
    LOG_INFO("停止运动");
    movement_stop(&movement);
    
    LOG_INFO("运动控制测试完成");
}

// 测试舵机ID映射
void test_servo_mapping(void) {
    LOG_INFO("=== 测试舵机ID映射 ===");
    
    for (int leg = 0; leg < NUM_LEGS; leg++) {
        for (int joint = 0; joint < 3; joint++) {
            uint8_t servo_id = servo_mapping_get_id(leg, joint);
            LOG_INFO("腿%d关节%d -> 舵机ID%d", leg, joint, servo_id);
        }
    }
    
    LOG_INFO("舵机ID映射测试完成");
}

