/**
 * @file ui_pomodoro_clock.h
 * @brief 番茄钟和实时时钟UI模块头文件
 * @version 0.1
 * @date 2025-01-01
 */

#ifndef __UI_POMODORO_CLOCK_H__
#define __UI_POMODORO_CLOCK_H__

#include "ui_display.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    POMODORO_STATE_IDLE = 0,
    POMODORO_STATE_WORK,
    POMODORO_STATE_SHORT_BREAK,
    POMODORO_STATE_LONG_BREAK,
} POMODORO_STATE_E;

// 番茄钟工作时间选项
typedef enum {
    POMODORO_TIME_5MIN = 5,
    POMODORO_TIME_10MIN = 10,
    POMODORO_TIME_20MIN = 20,
    POMODORO_TIME_25MIN = 25,
} POMODORO_TIME_OPTION_E;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief 初始化番茄钟和时钟UI
 * @param ui_font 字体配置
 * @param ui_theme 主题配置
 * @return 0-成功, -1-失败
 */
int ui_pomodoro_clock_init(UI_FONT_T *ui_font, UI_THEME_T *ui_theme);

/**
 * @brief 反初始化番茄钟和时钟UI
 */
void ui_pomodoro_clock_deinit(void);

/**
 * @brief 重置番茄钟
 */
void ui_pomodoro_reset(void);

/**
 * @brief 暂停番茄钟
 */
void ui_pomodoro_pause(void);

/**
 * @brief 开始/恢复番茄钟
 */
void ui_pomodoro_start(void);

/**
 * @brief 获取番茄钟当前状态
 * @return 番茄钟状态
 */
POMODORO_STATE_E ui_pomodoro_get_state(void);

/**
 * @brief 获取番茄钟剩余秒数
 * @return 剩余秒数
 */
uint32_t ui_pomodoro_get_remaining_seconds(void);

/**
 * @brief 获取番茄钟完成周期数
 * @return 完成周期数
 */
uint8_t ui_pomodoro_get_completed_cycles(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_POMODORO_CLOCK_H__ */