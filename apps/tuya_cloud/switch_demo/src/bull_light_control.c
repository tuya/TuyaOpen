/**
 * @file bull_light_control.c
 * @brief 公牛酷毙灯触摸按键模拟控制实现 - 独立模块
 * @version 1.0
 * @date 2025-01-27
 * 
 * 通过继电器模拟触摸按键来控制公牛酷毙灯的各种功能
 * 独立模块，方便移植到其他项目
 */

#include "bull_light_control.h"
#include "tkl_output.h"
#include "tkl_gpio.h"
#include "tal_sw_timer.h"
#include "tuya_cloud_types.h"
#include "tuya_iot_dp.h"
#include "tal_system.h"
#include "tal_log.h"
#include "relay_drv.h"

// 全局灯光状态
static bull_light_state_t g_bull_light_state = {
    .switch_state = false,
    .brightness = 50,
    .color_temp = 50,
    .brightness_step = 5,    // 亮度调节步长
    .color_temp_step = 5     // 色温调节步长
};



/**
 * @brief 初始化GPIO引脚
 */
static void bull_light_gpio_init(void)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_HIGH
    };
    
    // 配置继电器控制引脚为输出模式
    tkl_gpio_init(RELAY_POWER_PIN, &gpio_cfg);
    tkl_gpio_init(RELAY_BRIGHTNESS_PIN, &gpio_cfg);
    tkl_gpio_init(RELAY_COLOR_PIN, &gpio_cfg);
    
    // 初始状态：所有继电器关闭（高电平触发）
    tkl_gpio_write(RELAY_POWER_PIN, TUYA_GPIO_LEVEL_HIGH);
    tkl_gpio_write(RELAY_BRIGHTNESS_PIN, TUYA_GPIO_LEVEL_HIGH);
    tkl_gpio_write(RELAY_COLOR_PIN, TUYA_GPIO_LEVEL_HIGH);
    
    PR_INFO("公牛灯GPIO初始化完成");
}

/**
 * @brief 模拟触摸按键短按
 * @param pin GPIO引脚
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_simulate_touch_short_press(uint32_t pin)
{
    PR_DEBUG("模拟触摸按键短按: GPIO%d", pin);
    
    // 按下按键（继电器吸合，低电平）
    tkl_gpio_write(pin, TUYA_GPIO_LEVEL_LOW);
    
    // 短按延时
    tal_system_sleep(TOUCH_SHORT_PRESS_MS);
    
    // 释放按键（继电器断开，高电平）
    tkl_gpio_write(pin, TUYA_GPIO_LEVEL_HIGH);
    
    // 按键释放后等待
    tal_system_sleep(TOUCH_RELEASE_TIME_MS);
    
    return OPRT_OK;
}

/**
 * @brief 模拟触摸按键长按
 * @param pin GPIO引脚
 * @param duration_ms 长按持续时间(毫秒)
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_simulate_touch_long_press(uint32_t pin, uint32_t duration_ms)
{
    PR_DEBUG("模拟触摸按键长按: GPIO%d, 持续时间: %dms", pin, duration_ms);
    
    // 按下按键（继电器吸合，低电平）
    tkl_gpio_write(pin, TUYA_GPIO_LEVEL_LOW);
    
    // 长按延时
    tal_system_sleep(duration_ms);
    
    // 释放按键（继电器断开，高电平）
    tkl_gpio_write(pin, TUYA_GPIO_LEVEL_HIGH);
    
    // 按键释放后等待
    tal_system_sleep(TOUCH_RELEASE_TIME_MS);
    
    return OPRT_OK;
}

/**
 * @brief 控制灯光开关
 * @param state 开关状态
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_switch(bool state)
{
    PR_INFO("控制公牛灯开关: %s", state ? "开启" : "关闭");
    
    // 电源键：短按即可切换开关状态
    bull_light_simulate_touch_short_press(RELAY_POWER_PIN);
    
    // 更新状态
    g_bull_light_state.switch_state = state;
    
    return OPRT_OK;
}

/**
 * @brief 控制亮度
 * @param brightness 亮度值 (0-100)
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_brightness(uint8_t brightness)
{
    if (brightness > 100) {
        brightness = 100;
    }
    
    PR_INFO("设置公牛灯亮度: %d%%", brightness);
    
    if (!g_bull_light_state.switch_state) {
        PR_WARN("公牛灯未开启，无法调节亮度");
        return OPRT_INVALID_PARM;
    }
    
    // 根据当前亮度和目标亮度的差值，决定调节方向和次数
    int8_t brightness_diff = brightness - g_bull_light_state.brightness;
    
    if (brightness_diff > 0) {
        // 需要增加亮度
        for (uint8_t i = 0; i < brightness_diff && i < 20; i += g_bull_light_state.brightness_step) {
            bull_light_control_brightness_step(BRIGHTNESS_UP);
            tal_system_sleep(TOUCH_INTERVAL_MS);
        }
    } else if (brightness_diff < 0) {
        // 需要减少亮度
        for (uint8_t i = 0; i < -brightness_diff && i < 20; i += g_bull_light_state.brightness_step) {
            bull_light_control_brightness_step(BRIGHTNESS_DOWN);
            tal_system_sleep(TOUCH_INTERVAL_MS);
        }
    }
    
    g_bull_light_state.brightness = brightness;
    
    return OPRT_OK;
}

/**
 * @brief 亮度调节（短按）
 * @param direction 调节方向
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_brightness_step(brightness_direction_t direction)
{
    if (!g_bull_light_state.switch_state) {
        PR_WARN("公牛灯未开启，无法调节亮度");
        return OPRT_INVALID_PARM;
    }
    
    PR_DEBUG("公牛灯亮度调节: %s", direction == BRIGHTNESS_UP ? "增加" : "减少");
    
    // 短按亮度键进行阶段调节
    bull_light_simulate_touch_short_press(RELAY_BRIGHTNESS_PIN);
    
    // 更新亮度状态（这里只是模拟，实际亮度由灯具内部控制）
    if (direction == BRIGHTNESS_UP) {
        g_bull_light_state.brightness = (g_bull_light_state.brightness + g_bull_light_state.brightness_step > 100) ? 
                                       100 : g_bull_light_state.brightness + g_bull_light_state.brightness_step;
    } else {
        g_bull_light_state.brightness = (g_bull_light_state.brightness < g_bull_light_state.brightness_step) ? 
                                       0 : g_bull_light_state.brightness - g_bull_light_state.brightness_step;
    }
    
    return OPRT_OK;
}

/**
 * @brief 亮度调节（长按连续调节）
 * @param direction 调节方向
 * @param duration_ms 长按持续时间(毫秒)
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_brightness_continuous(brightness_direction_t direction, uint32_t duration_ms)
{
    if (!g_bull_light_state.switch_state) {
        PR_WARN("公牛灯未开启，无法调节亮度");
        return OPRT_INVALID_PARM;
    }
    
    PR_DEBUG("公牛灯亮度连续调节: %s, 持续时间: %dms", direction == BRIGHTNESS_UP ? "增加" : "减少", duration_ms);
    
    // 长按亮度键进行连续调节
    bull_light_simulate_touch_long_press(RELAY_BRIGHTNESS_PIN, duration_ms);
    
    // 更新亮度状态（这里只是模拟，实际亮度由灯具内部控制）
    uint8_t step_count = duration_ms / 100; // 假设每100ms调节一步
    if (direction == BRIGHTNESS_UP) {
        g_bull_light_state.brightness = (g_bull_light_state.brightness + step_count > 100) ? 
                                       100 : g_bull_light_state.brightness + step_count;
    } else {
        g_bull_light_state.brightness = (g_bull_light_state.brightness < step_count) ? 
                                       0 : g_bull_light_state.brightness - step_count;
    }
    
    return OPRT_OK;
}

/**
 * @brief 控制色温
 * @param color_temp 色温值 (0-100)
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_color_temp(uint8_t color_temp)
{
    if (color_temp > 100) {
        color_temp = 100;
    }
    
    PR_INFO("设置公牛灯色温: %d%%", color_temp);
    
    if (!g_bull_light_state.switch_state) {
        PR_WARN("公牛灯未开启，无法调节色温");
        return OPRT_INVALID_PARM;
    }
    
    // 根据当前色温和目标色温的差值，决定调节方向和次数
    int8_t color_diff = color_temp - g_bull_light_state.color_temp;
    
    if (color_diff > 0) {
        // 需要增加色温
        for (uint8_t i = 0; i < color_diff && i < 20; i += g_bull_light_state.color_temp_step) {
            bull_light_control_color_temp_step(COLOR_TEMP_UP);
            tal_system_sleep(TOUCH_INTERVAL_MS);
        }
    } else if (color_diff < 0) {
        // 需要减少色温
        for (uint8_t i = 0; i < -color_diff && i < 20; i += g_bull_light_state.color_temp_step) {
            bull_light_control_color_temp_step(COLOR_TEMP_DOWN);
            tal_system_sleep(TOUCH_INTERVAL_MS);
        }
    }
    
    g_bull_light_state.color_temp = color_temp;
    
    return OPRT_OK;
}

/**
 * @brief 色温调节（短按）
 * @param direction 调节方向
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_color_temp_step(color_temp_direction_t direction)
{
    if (!g_bull_light_state.switch_state) {
        PR_WARN("公牛灯未开启，无法调节色温");
        return OPRT_INVALID_PARM;
    }
    
    PR_DEBUG("公牛灯色温调节: %s", direction == COLOR_TEMP_UP ? "增加" : "减少");
    
    // 短按色温键进行阶段调节
    bull_light_simulate_touch_short_press(RELAY_COLOR_PIN);
    
    // 更新色温状态（这里只是模拟，实际色温由灯具内部控制）
    if (direction == COLOR_TEMP_UP) {
        g_bull_light_state.color_temp = (g_bull_light_state.color_temp + g_bull_light_state.color_temp_step > 100) ? 
                                       100 : g_bull_light_state.color_temp + g_bull_light_state.color_temp_step;
    } else {
        g_bull_light_state.color_temp = (g_bull_light_state.color_temp < g_bull_light_state.color_temp_step) ? 
                                       0 : g_bull_light_state.color_temp - g_bull_light_state.color_temp_step;
    }
    
    return OPRT_OK;
}

/**
 * @brief 色温调节（长按连续调节）
 * @param direction 调节方向
 * @param duration_ms 长按持续时间(毫秒)
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_color_temp_continuous(color_temp_direction_t direction, uint32_t duration_ms)
{
    if (!g_bull_light_state.switch_state) {
        PR_WARN("公牛灯未开启，无法调节色温");
        return OPRT_INVALID_PARM;
    }
    
    PR_DEBUG("公牛灯色温连续调节: %s, 持续时间: %dms", direction == COLOR_TEMP_UP ? "增加" : "减少", duration_ms);
    
    // 长按色温键进行连续调节
    bull_light_simulate_touch_long_press(RELAY_COLOR_PIN, duration_ms);
    
    // 更新色温状态（这里只是模拟，实际色温由灯具内部控制）
    uint8_t step_count = duration_ms / 100; // 假设每100ms调节一步
    if (direction == COLOR_TEMP_UP) {
        g_bull_light_state.color_temp = (g_bull_light_state.color_temp + step_count > 100) ? 
                                       100 : g_bull_light_state.color_temp + step_count;
    } else {
        g_bull_light_state.color_temp = (g_bull_light_state.color_temp < step_count) ? 
                                       0 : g_bull_light_state.color_temp - step_count;
    }
    
    return OPRT_OK;
}

/**
 * @brief 处理DP命令
 * @param dpobj DP对象
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_handle_dp_command(dp_obj_recv_t *dpobj)
{
    OPERATE_RET ret = OPRT_OK;
    
    for (uint32_t i = 0; i < dpobj->dpscnt; i++) {
        dp_obj_t *dp = dpobj->dps + i;
        
        switch (dp->id) {
        case DPID_SWITCH:
            ret = bull_light_control_switch(dp->value.dp_bool);
            break;
            
        case DPID_BRIGHTNESS:
            ret = bull_light_control_brightness(dp->value.dp_value);
            break;
            
        case DPID_COLOR_TEMP:
            ret = bull_light_control_color_temp(dp->value.dp_value);
            break;
            
        case DPID_BRIGHTNESS_UP:
            ret = bull_light_control_brightness_step(BRIGHTNESS_UP);
            break;
            
        case DPID_BRIGHTNESS_DOWN:
            ret = bull_light_control_brightness_step(BRIGHTNESS_DOWN);
            break;
            
        case DPID_COLOR_TEMP_UP:
            ret = bull_light_control_color_temp_step(COLOR_TEMP_UP);
            break;
            
        case DPID_COLOR_TEMP_DOWN:
            ret = bull_light_control_color_temp_step(COLOR_TEMP_DOWN);
            break;
            
        default:
            PR_WARN("未知DP ID: %d", dp->id);
            break;
        }
        
        if (ret != OPRT_OK) {
            PR_ERR("处理公牛灯DP命令失败: ID=%d, 错误码=%d", dp->id, ret);
        }
    }
    
    return ret;
}

/**
 * @brief 获取当前灯光状态
 * @return bull_light_state_t* 灯光状态指针
 */
bull_light_state_t* bull_light_get_state(void)
{
    return &g_bull_light_state;
}

/**
 * @brief 初始化公牛灯控制模块
 * @return OPERATE_RET
 */
OPERATE_RET bull_light_control_init(void)
{
    PR_INFO("初始化公牛灯控制模块");
    
    // 初始化GPIO
    bull_light_gpio_init();
    
    // 初始化状态
    g_bull_light_state.switch_state = false;
    g_bull_light_state.brightness = 50;
    g_bull_light_state.color_temp = 50;
    g_bull_light_state.brightness_step = 5;
    g_bull_light_state.color_temp_step = 5;
    
    PR_INFO("公牛灯控制模块初始化完成");
    
    return OPRT_OK;
}