/*****************************************************************************
* | File      	:	Debug.h
* | Author      :   Waveshare team
* | Function    :	调试日志输出接口
* | Info        :   调试信息输出宏定义
*                   提供统一的日志输出接口，便于调试和问题定位
*                   
*                   使用说明：
*                   - PR_DEBUG: 调试级别日志，用于开发调试
*                   - PR_INFO:  信息级别日志，用于正常运行信息
*                   - PR_WARN:  警告级别日志，用于潜在问题提示
*                   - PR_ERR:   错误级别日志，用于错误信息输出
*                   - Debug:    条件编译调试输出
*
*   Image scanning
*      Please use progressive scanning to generate images or fonts
*      （图像扫描：请使用逐行扫描方式生成图像或字体）
*----------------
* |	This version:   V2.0
* | Date        :   2018-10-30
* | Info        :   
*   1.USE_DEBUG -> DEBUG, If you need to see the debug information, 
*    clear the execution: make DEBUG=-DDEBUG
*   （如果需要查看调试信息，请在编译时添加 -DDEBUG 宏定义）
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

******************************************************************************/
#ifndef __DEBUG_H
#define __DEBUG_H

/*============================================================================
                            头文件包含
============================================================================*/
#include <stdio.h>      // 标准输入输出（printf函数）
#include "tal_log.h"    // TuyaOS日志输出接口

/*============================================================================
                            日志输出宏定义
============================================================================*/
/**
 * @brief 墨水屏模块日志输出宏
 * @details 封装TuyaOS的日志输出接口，为墨水屏模块添加统一的日志前缀[EPD]
 *          便于在系统日志中快速定位墨水屏相关信息
 * 
 * 日志级别说明：
 * ┌───────────┬─────────────────────────────────────────────────────────┐
 * │  宏名称   │                       使用场景                          │
 * ├───────────┼─────────────────────────────────────────────────────────┤
 * │ PR_DEBUG  │ 调试信息，开发阶段使用，如变量值、执行流程跟踪         │
 * │ PR_INFO   │ 运行信息，记录正常的运行状态，如初始化完成、刷新开始   │
 * │ PR_WARN   │ 警告信息，提示潜在问题，如参数越界但已处理             │
 * │ PR_ERR    │ 错误信息，记录错误状态，如初始化失败、通信超时         │
 * └───────────┴─────────────────────────────────────────────────────────┘
 * 
 * 使用示例：
 *   PR_DEBUG("pixel value: %d", pixel);
 *   PR_INFO("EPD init success");
 *   PR_WARN("image size exceeds display area");
 *   PR_ERR("SPI communication timeout");
 */

/* 调试级别日志 - 最详细的日志信息，用于开发调试 */
#ifndef PR_DEBUG
#define PR_DEBUG(fmt, ...) TAL_PR_DEBUG("[EPD] " fmt, ##__VA_ARGS__)
#endif

/* 信息级别日志 - 记录正常的运行信息 */
#ifndef PR_INFO
#define PR_INFO(fmt, ...) TAL_PR_INFO("[EPD] " fmt, ##__VA_ARGS__)
#endif

/* 警告级别日志 - 记录潜在问题或异常情况 */
#ifndef PR_WARN
#define PR_WARN(fmt, ...) TAL_PR_WARN("[EPD] " fmt, ##__VA_ARGS__)
#endif

/* 错误级别日志 - 记录错误和异常 */
#ifndef PR_ERR
#define PR_ERR(fmt, ...) TAL_PR_ERR("[EPD] " fmt, ##__VA_ARGS__)
#endif

/*============================================================================
                            条件编译调试宏
============================================================================*/
/**
 * @brief 条件编译调试输出宏
 * @details 根据DEBUG宏的定义情况选择不同的调试输出方式：
 *          - DEBUG已定义：使用标准printf输出，带"Debug:"前缀
 *          - DEBUG未定义：使用PR_DEBUG输出到TuyaOS日志系统
 * 
 * 编译选项：
 *   - 启用详细调试：编译时添加 -DDEBUG 参数
 *   - 关闭详细调试：不添加 -DDEBUG 参数（默认）
 * 
 * 使用示例：
 *   Debug("init complete, width=%d, height=%d\n", width, height);
 */
#if DEBUG
    /* DEBUG模式：使用printf直接输出到控制台 */
    #define Debug(__info,...) printf("Debug: " __info, ##__VA_ARGS__)
#else
    /* 非DEBUG模式：使用TuyaOS日志系统输出 */
	#define Debug       PR_DEBUG
#endif

#endif /* __DEBUG_H */
