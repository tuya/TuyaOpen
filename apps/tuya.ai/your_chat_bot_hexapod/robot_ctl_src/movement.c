#include "movement.h"
#include "movement_table.h"
#include "debug.h"
#include <string.h>
#include <math.h>

// 运动表函数实现（已移至movement_table.c）

// 运动控制函数实现

/**
 * @brief 创建运动控制对象
 * @param time_func 时间函数指针
 * @return 创建的运动控制对象
 */
Movement_t movement_create(void) {
    Movement_t movement;
    movement.current_mode = MOVEMENT_MODE_STANDBY;
    movement.is_moving = false;
    movement.step_start_time = 0;
    movement.current_step = 0;
    movement.current_entry = 0;
    
    
    // 初始化批量控制器
    bus_servo_batch_init(&movement.batch_controller);
    
    // 初始化腿部控制对象
    for (int i = 0; i < NUM_LEGS; i++) {
        Point3D_t root_pos = point3d_create(0, 0, 0);
        movement.legs[i] = bus_leg_create(i, &root_pos);
    }
    
    return movement;
}

/**
 * @brief 初始化运动控制对象
 * @param movement 运动控制对象指针
 * @return true 成功，false 失败
 */
bool movement_init(Movement_t* movement) {
    if (movement == NULL) {
        LOG_ERROR("Movement pointer is NULL");
        return false;
    }
    
    // 初始化所有腿部控制对象
    for (int i = 0; i < NUM_LEGS; i++) {
        if (!bus_leg_init(&movement->legs[i])) {
            LOG_ERROR("Failed to initialize leg %d", i);
            return false;
        }
    }
    
    LOG_INFO("Movement system initialized successfully");
    return true;
}

/**
 * @brief 设置运动模式
 * @param movement 运动控制对象指针
 * @param new_mode 新的运动模式
 * @return true 成功，false 失败
 */
bool movement_set_mode(Movement_t* movement, MovementMode_t new_mode) {
    if (movement == NULL) {
        LOG_ERROR("Movement pointer is NULL");
        return false;
    }
    
    if (new_mode >= MOVEMENT_MODE_COUNT) {
        LOG_ERROR("Invalid movement mode: %d", new_mode);
        return false;
    }
    
    // 停止当前运动
    movement_stop(movement);
    
    // 设置新的运动模式
    movement->current_mode = new_mode;
    movement->current_step = 0;
    movement->current_entry = 0;
    movement->is_moving = true;
    movement->step_start_time = 0; // 将在第一次更新时设置
    
    LOG_INFO("Movement mode set to: %d", new_mode);
    return true;
}

/**
 * @brief 更新运动状态
 * @param movement 运动控制对象指针
 * @return true 成功，false 失败
 */
bool movement_update(Movement_t* movement) {
    if (movement == NULL) {
        LOG_ERROR("Movement pointer is NULL");
        return false;
    }
    
    if (!movement->is_moving) {
        return true;
    }
    
    // 获取当前运动表
    const MovementTable_t* table = NULL;
    switch (movement->current_mode) {
        case MOVEMENT_MODE_STANDBY:
            table = standby_table();
            break;
        case MOVEMENT_MODE_FORWARD:
            table = forward_table();
            break;
        case MOVEMENT_MODE_BACKWARD:
            table = backward_table();
            break;
        case MOVEMENT_MODE_TURN_LEFT:
            table = turn_left_table();
            break;
        case MOVEMENT_MODE_TURN_RIGHT:
            table = turn_right_table();
            break;
        case MOVEMENT_MODE_CLIMB:
            table = climb_table();
            break;
        case MOVEMENT_MODE_DESCEND:
            table = descend_table();
            break;
        case MOVEMENT_MODE_TURN_LEFT_FAST:
            table = turn_left_fast_table();
            break;
        case MOVEMENT_MODE_TURN_RIGHT_FAST:
            table = turn_right_fast_table();
            break;
        default:
            LOG_ERROR("Unknown movement mode: %d", movement->current_mode);
            return false;
    }
    
    if (table == NULL) {
        LOG_ERROR("Failed to get movement table for mode: %d", movement->current_mode);
        return false;
    }
    
    // 检查是否到达表末尾
    if (movement->current_step >= table->length) {
        movement->is_moving = false;
        LOG_DEBUG("Movement completed for mode: %d", movement->current_mode);
        return true;
    }
    
    // 获取当前步骤的位置数据
    if (movement->current_step >= table->length) {
        LOG_ERROR("Current step %d exceeds table length %d", movement->current_step, table->length);
        movement->is_moving = false;
        return false;
    }
    
    const Locations_t* current_locations = &table->table[movement->current_step];
    if (current_locations == NULL) {
        LOG_ERROR("Current locations is NULL at step %d", movement->current_step);
        movement->is_moving = false;
        return false;
    }
    
    // 检查是否需要进入新的入口点
    if (movement->current_entry < table->entriesCount) {
        int entry_step = table->entries[movement->current_entry];
        if (movement->current_step == entry_step) {
            LOG_DEBUG("Entering entry point %d at step %d", movement->current_entry, entry_step);
            movement->current_entry++;
        }
    }
    
    // 设置所有腿的位置
    for (int i = 0; i < NUM_LEGS; i++) {
        Point3D_t target_pos = current_locations->points[i];
        
        LOG_DEBUG("Moving leg %d to position (%.2f, %.2f, %.2f)", 
                  i, target_pos.x, target_pos.y, target_pos.z);
        
        // 移动腿到目标位置
        if (!bus_leg_move_tip(&movement->legs[i], &target_pos, table->stepDuration)) {
            LOG_WARN("Failed to move leg %d to target position", i);
        }
    }
    
    // 更新步骤
    movement->current_step++;
    
    LOG_DEBUG("Movement step %d/%d completed for mode %d", 
              movement->current_step, table->length, movement->current_mode);
    
    return true;
}

/**
 * @brief 停止运动
 * @param movement 运动控制对象指针
 */
void movement_stop(Movement_t* movement) {
    if (movement == NULL) {
        return;
    }
    
    movement->is_moving = false;
    movement->current_step = 0;
    movement->current_entry = 0;
    movement->step_start_time = 0;
    
    LOG_INFO("Movement stopped");
}

/**
 * @brief 获取当前运动模式
 * @param movement 运动控制对象指针
 * @return 当前运动模式
 */
MovementMode_t movement_get_mode(const Movement_t* movement) {
    if (movement == NULL) {
        return MOVEMENT_MODE_STANDBY;
    }
    return movement->current_mode;
}

/**
 * @brief 检查是否正在运动
 * @param movement 运动控制对象指针
 * @return true 正在运动，false 未运动
 */
bool movement_is_moving(const Movement_t* movement) {
    if (movement == NULL) {
        return false;
    }
    return movement->is_moving;
}

/**
 * @brief 获取指定腿的控制对象
 * @param movement 运动控制对象指针
 * @param leg_index 腿索引
 * @return 腿控制对象指针
 */
BusLeg_t* movement_get_leg(Movement_t* movement, uint8_t leg_index) {
    if (movement == NULL || leg_index >= NUM_LEGS) {
        return NULL;
    }
    return &movement->legs[leg_index];
}

/**
 * @brief 设置腿部根部位置
 * @param movement 运动控制对象指针
 * @param leg_index 腿索引
 * @param position 根部位置
 */
void movement_set_leg_root_position(Movement_t* movement, uint8_t leg_index, const Point3D_t* position) {
    if (movement == NULL || leg_index >= NUM_LEGS || position == NULL) {
        return;
    }
    
    movement->legs[leg_index].root_position = *position;
    LOG_DEBUG("Leg %d root position set to (%.2f, %.2f, %.2f)", 
              leg_index, position->x, position->y, position->z);
}

/**
 * @brief 获取当前步骤索引
 * @param movement 运动控制对象指针
 * @return 当前步骤索引
 */
int movement_get_current_step(const Movement_t* movement) {
    if (movement == NULL) {
        return 0;
    }
    return movement->current_step;
}

/**
 * @brief 获取当前入口点索引
 * @param movement 运动控制对象指针
 * @return 当前入口点索引
 */
int movement_get_current_entry(const Movement_t* movement) {
    if (movement == NULL) {
        return 0;
    }
    return movement->current_entry;
}
