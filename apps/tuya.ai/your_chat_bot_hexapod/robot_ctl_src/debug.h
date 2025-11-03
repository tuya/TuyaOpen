//
// 调试日志系统
// 用于C语言版本的总线舵机控制系统
// 集成 Tuya 系统日志
//

#pragma once

#include "tal_api.h"

// 使用 Tuya 系统的日志宏，映射到自定义的日志宏名称
#define LOG_ERROR(format, ...) PR_ERR("[ROBOT_CTL] " format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)  PR_WARN("[ROBOT_CTL] " format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  PR_INFO("[ROBOT_CTL] " format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) PR_DEBUG("[ROBOT_CTL] " format, ##__VA_ARGS__)
#define LOG_TRACE(format, ...) PR_TRACE("[ROBOT_CTL] " format, ##__VA_ARGS__)

// 简化的日志宏（不带级别前缀，使用 INFO 级别）
#define LOG(format, ...) PR_INFO("[ROBOT_CTL] " format, ##__VA_ARGS__)

// 为了保持向后兼容，保留这些宏定义（但不再使用）
#define LOG_LEVEL_NONE    0
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_WARN    2
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_DEBUG   4

// 初始化函数（现在为空实现，因为使用系统日志）
static inline void debug_init_log_output(void *writer, void *time_func) {
    // Tuya 系统日志已经初始化，无需额外初始化
    (void)writer;
    (void)time_func;
}
