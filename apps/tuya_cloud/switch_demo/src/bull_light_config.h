/**
 * @file bull_light_config.h
 * @brief 公牛灯控制模块配置文件
 * @version 1.0
 * @date 2025-01-27
 * 
 * 用于配置公牛灯控制模块的硬件参数和功能选项
 * 移植时需要根据实际硬件修改此文件
 */

#ifndef __BULL_LIGHT_CONFIG_H__
#define __BULL_LIGHT_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************硬件配置*****************************
***********************************************************/

// 继电器控制引脚定义（需要根据实际硬件连接修改）
#define BULL_RELAY_POWER_PIN       GPIO_NUM_2   // 电源开关继电器（按键1）
#define BULL_RELAY_BRIGHTNESS_PIN  GPIO_NUM_3   // 亮度调节继电器（按键2）
#define BULL_RELAY_COLOR_PIN       GPIO_NUM_4   // 色温调节继电器（按键3）

// 继电器触发方式定义
#define BULL_RELAY_TRIGGER_LEVEL   TKL_GPIO_LEVEL_LOW   // 继电器触发电平（低电平触发）
#define BULL_RELAY_RELEASE_LEVEL   TKL_GPIO_LEVEL_HIGH  // 继电器释放电平（高电平释放）

/***********************************************************
***********************时间配置*****************************
***********************************************************/

// 触摸按键模拟时间定义
#define BULL_TOUCH_SHORT_PRESS_MS  100  // 短按持续时间(毫秒)
#define BULL_TOUCH_LONG_PRESS_MS   2000 // 长按持续时间(毫秒)
#define BULL_TOUCH_RELEASE_TIME_MS 50   // 按键释放后等待时间(毫秒)
#define BULL_TOUCH_INTERVAL_MS     200  // 连续按键间隔时间(毫秒)

/***********************************************************
***********************功能配置*****************************
***********************************************************/

// 亮度调节配置
#define BULL_BRIGHTNESS_STEP       5     // 亮度调节步长
#define BULL_BRIGHTNESS_MAX        100   // 最大亮度值
#define BULL_BRIGHTNESS_MIN        0     // 最小亮度值

// 色温调节配置
#define BULL_COLOR_TEMP_STEP       5     // 色温调节步长
#define BULL_COLOR_TEMP_MAX        100   // 最大色温值
#define BULL_COLOR_TEMP_MIN        0     // 最小色温值

// 调节限制配置
#define BULL_MAX_ADJUST_STEPS      20    // 单次最大调节步数
#define BULL_STEP_INTERVAL_MS      100   // 连续调节时每步间隔时间(毫秒)

/***********************************************************
***********************调试配置*****************************
***********************************************************/

// 调试开关
#define BULL_DEBUG_ENABLE          1     // 启用调试输出 (1:启用, 0:禁用)

// 调试级别
#define BULL_DEBUG_LEVEL_INFO      1     // 信息级别调试
#define BULL_DEBUG_LEVEL_WARN      1     // 警告级别调试
#define BULL_DEBUG_LEVEL_ERR       1     // 错误级别调试
#define BULL_DEBUG_LEVEL_DEBUG     0     // 调试级别调试

#ifdef __cplusplus
}
#endif

#endif /* __BULL_LIGHT_CONFIG_H__ */
