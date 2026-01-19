/*****************************************************************************
* | File      	:	EPD_Test.h
* | Author      :   Waveshare team
* | Function    :   电子墨水屏测试Demo头文件
* | Info        :
*                   本文件定义了墨水屏测试函数的接口
*                   包含所需的头文件依赖
*                   
*                   测试Demo功能：
*                   1. 图片显示测试
*                   2. 图形绘制测试（点、线、矩形、圆）
*                   3. 文字显示测试（中英文）
*                   4. 局部刷新测试（时钟显示）
*                   5. 4级灰度显示测试
*                   6. 睡眠模式测试
*----------------
* |	This version:   V1.1
* | Date        :   2022-07-28
* | Info        :   
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
#ifndef _EPD_TEST_H_
#define _EPD_TEST_H_

/*============================================================================
                            头文件包含
============================================================================*/
#include "DEV_Config.h"     // 硬件配置（GPIO、SPI等）
#include "GUI_Paint.h"      // 图形绘制库（点、线、矩形、圆、文字等）
#include "GUI_BMPfile.h"    // BMP图片处理
#include "ImageData.h"      // 测试图片数据
#include "Debug.h"          // 调试日志输出
#include <stdlib.h>         // 标准库（malloc, free内存管理）

/*============================================================================
                            函数声明
============================================================================*/

/**
 * @brief 电子墨水屏综合测试函数
 * @details 执行完整的墨水屏功能测试，包括：
 *          1. 初始化和清屏
 *          2. 快速模式图片显示
 *          3. 图形绘制（点、线、矩形、圆）
 *          4. 文字显示（中英文、数字）
 *          5. 局部刷新（时钟动态显示）
 *          6. 4级灰度显示
 *          7. 进入睡眠模式
 * 
 * @return int
 *         - 0:  测试成功完成
 *         - -1: 测试失败（初始化失败或内存分配失败）
 * 
 * @note 运行此测试需要约1-2分钟（包含多次刷新等待时间）
 */
int EPD_test(void);

#endif /* _EPD_TEST_H_ */
