/**
 * @file bull_light_control.h
 * @brief 公牛酷毙灯触摸按键模拟控制 - 独立模块
 * @version 1.0
 * @date 2025-01-27
 * 
 * 通过继电器模拟触摸按键来控制公牛酷毙灯的各种功能
 * 独立模块，方便移植到其他项目
 */

#ifndef __BULL_LIGHT_CONTROL_H__
#define __BULL_LIGHT_CONTROL_H__

#include "tuya_cloud_types.h"
#include "tuya_iot_dp.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

// 数据点定义
#define DPID_SWITCH           1   // 开关
#define DPID_BRIGHTNESS       2   // 亮度 (0-100)
#define DPID_COLOR_TEMP       3   // 色温 (0-100)
#define DPID_BRIGHTNESS_UP    4   // 亮度增加
#define DPID_BRIGHTNESS_DOWN  5   // 亮度减少
#define DPID_COLOR_TEMP_UP    6   // 色温增加
#define DPID_COLOR_TEMP_DOWN  7   // 色温减少

// 继电器控制引脚定义（需要根据实际硬件连接修改）
#define RELAY_POWER_PIN       20   // 电源开关继电器（按键1）
#define RELAY_BRIGHTNESS_PIN  21   // 亮度调节继电器（按键2）
#define RELAY_COLOR_PIN       22   // 色温调节继电器（按键3）

// 触摸按键模拟时间定义
#define TOUCH_SHORT_PRESS_MS  100  // 短按持续时间(毫秒)
#define TOUCH_LONG_PRESS_MS   2000 // 长按持续时间(毫秒)
#define TOUCH_RELEASE_TIME_MS 50   // 按键释放后等待时间(毫秒)
#define TOUCH_INTERVAL_MS     200  // 连续按键间隔时间(毫秒)

// 亮度调节方向
typedef enum {
    BRIGHTNESS_UP = 0,        // 亮度增加
    BRIGHTNESS_DOWN = 1       // 亮度减少
} brightness_direction_t;

// 色温调节方向
typedef enum {
    COLOR_TEMP_UP = 0,        // 色温增加
    COLOR_TEMP_DOWN = 1       // 色温减少
} color_temp_direction_t;

/***********************************************************
***********************typedef define***********************
***********************************************************/

// 灯光状态结构体
typedef struct {
    bool switch_state;        // 开关状态
    uint8_t brightness;      // 亮度 (0-100)
    uint8_t color_temp;      // 色温 (0-100)
    uint8_t brightness_step; // 亮度调节步长
    uint8_t color_temp_step; // 色温调节步长
} bull_light_state_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief 初始化公牛灯控制模块
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_init(void);

/**
 * @brief 控制灯光开关
 * @param state 开关状态
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_switch(bool state);

/**
 * @brief 控制亮度
 * @param brightness 亮度值 (0-100)
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_brightness(uint8_t brightness);

/**
 * @brief 控制色温
 * @param color_temp 色温值 (0-100)
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_color_temp(uint8_t color_temp);

/**
 * @brief 亮度调节（短按）
 * @param direction 调节方向
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_brightness_step(brightness_direction_t direction);

/**
 * @brief 亮度调节（长按连续调节）
 * @param direction 调节方向
 * @param duration_ms 长按持续时间(毫秒)
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_brightness_continuous(brightness_direction_t direction, uint32_t duration_ms);

/**
 * @brief 色温调节（短按）
 * @param direction 调节方向
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_color_temp_step(color_temp_direction_t direction);

/**
 * @brief 色温调节（长按连续调节）
 * @param direction 调节方向
 * @param duration_ms 长按持续时间(毫秒)
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_color_temp_continuous(color_temp_direction_t direction, uint32_t duration_ms);

/**
 * @brief 处理DP命令
 * @param dpobj DP对象
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_handle_dp_command(dp_obj_recv_t *dpobj);

/**
 * @brief 获取当前灯光状态
 * @return bull_light_state_t* 灯光状态指针
 */
bull_light_state_t* bull_light_get_state(void);

/**
 * @brief 模拟触摸按键短按
 * @param pin GPIO引脚
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_simulate_touch_short_press(uint32_t pin);

/**
 * @brief 模拟触摸按键长按
 * @param pin GPIO引脚
 * @param duration_ms 长按持续时间(毫秒)
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_simulate_touch_long_press(uint32_t pin, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif /* __BULL_LIGHT_CONTROL_H__ */