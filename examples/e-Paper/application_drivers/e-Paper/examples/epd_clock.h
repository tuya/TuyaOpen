/*****************************************************************************
* | File        :   epd_clock.h
* | Author      :   Custom
* | Function    :   电子墨水屏时钟显示模块（使用统一缓冲区版本）
* | Info        :   
*                   负责整个墨水屏的刷新管理：
*                   - 初始化墨水屏和全屏缓冲区
*                   - 管理信息栏区域的绘制
*                   - 统一刷新信息栏和宠物区域
*                   - 避免多区域刷新冲突
*----------------
* |  This version:  V1.1
* | Date        :   2024-12-11
******************************************************************************/
#ifndef __EPD_CLOCK_H__
#define __EPD_CLOCK_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
                            配置参数（使用epd_display.h的定义）
============================================================================*/
/* 信息栏区域定义 - 与 epd_display.h 保持一致 */
/* 视觉上方，Y=0~120 */
#define EPD_CLOCK_X         0
#define EPD_CLOCK_Y         0       /* 信息栏顶边 */
#define EPD_CLOCK_WIDTH     800
#define EPD_CLOCK_HEIGHT    120

/**
 * @brief 刷新间隔配置（秒）
 */
#define EPD_CLOCK_REFRESH_SECONDS   60

/*============================================================================
                            函数声明
============================================================================*/

/**
 * @brief 启动信息栏显示线程
 * @details 初始化墨水屏、全屏缓冲区，创建刷新线程
 *          同时负责刷新信息栏和宠物区域
 * @return OPRT_OK 成功，其他值 失败
 */
OPERATE_RET epd_clock_start(VOID_T);

/**
 * @brief 停止信息栏显示线程
 * @details 停止并销毁线程，释放资源
 * @return OPRT_OK 成功，其他值 失败
 */
OPERATE_RET epd_clock_stop(VOID_T);

/**
 * @brief 检查时钟是否在运行
 * @return TRUE 正在运行，FALSE 已停止
 */
BOOL_T epd_clock_is_running(VOID_T);

/**
 * @brief 立即刷新显示（无需等待下一分钟）
 * @details 强制立即刷新整个屏幕
 * @return OPRT_OK 成功，其他值 失败
 */
OPERATE_RET epd_clock_refresh_now(VOID_T);

#ifdef __cplusplus
}
#endif

#endif /* __EPD_CLOCK_H__ */
