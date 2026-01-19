/*****************************************************************************
* | File      	:   EPD_4in26.h
* | Author      :   Waveshare team
* | Function    :   4.26英寸电子墨水屏驱动头文件
* | Info        :
*                   本文件定义了4.26英寸电子墨水屏的：
*                   - 显示分辨率参数
*                   - 驱动函数声明
*                   
*                   支持的显示模式：
*                   - 标准全刷新（约4秒）
*                   - 快速刷新（约1.5秒）
*                   - 局部刷新
*                   - 4级灰度显示
*----------------
* |	This version:   V1.0
* | Date        :   2023-12-19
* | Info        :   Waveshare 4.26寸电子墨水屏驱动
* -----------------------------------------------------------------------------
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
#ifndef __EPD_4in26_H_
#define __EPD_4in26_H_

/*============================================================================
                            头文件包含
============================================================================*/
#include "DEV_Config.h"     // 硬件配置和底层接口

/*============================================================================
                            显示屏参数定义
============================================================================*/
/**
 * @brief 显示屏分辨率定义
 * @details 4.26英寸电子墨水屏的物理分辨率
 *          - 宽度: 800像素
 *          - 高度: 480像素
 *          - 每像素1位（黑白）或2位（4灰度）
 *          - 显示区域: 800 x 480 = 384,000像素
 *          - 黑白模式图像缓冲区大小: 800 * 480 / 8 = 48,000字节
 */
#define EPD_4in26_WIDTH       800    // 显示屏宽度（像素）
#define EPD_4in26_HEIGHT      480    // 显示屏高度（像素）

/*============================================================================
                            函数声明
============================================================================*/

/*------------------------------ 初始化函数 ------------------------------*/

/**
 * @brief 标准初始化
 * @details 初始化电子墨水屏为标准刷新模式
 *          适用于对刷新速度要求不高、需要最佳显示质量的场景
 *          刷新时间约4秒
 */
void EPD_4in26_Init(void);

/**
 * @brief 快速初始化
 * @details 初始化电子墨水屏为快速刷新模式
 *          刷新速度较快（约1.5秒），但可能存在轻微残影
 *          适用于需要频繁更新显示内容的场景
 */
void EPD_4in26_Init_Fast(void);

/**
 * @brief 4级灰度初始化
 * @details 初始化电子墨水屏为4级灰度显示模式
 *          支持显示：黑、深灰、浅灰、白四种灰度
 *          可显示更丰富的图像层次
 */
void EPD_4in26_Init_4GRAY(void);

/*------------------------------ 显示控制函数 ------------------------------*/

/**
 * @brief 清屏
 * @details 将整个屏幕刷新为白色
 *          建议在首次显示前调用，消除可能的残影
 */
void EPD_4in26_Clear(void);

/**
 * @brief 标准显示
 * @details 将图像缓冲区内容显示到屏幕上
 *          使用标准刷新波形，显示质量最好
 * @param Image 图像缓冲区指针，大小需为48000字节（800*480/8）
 *              每个字节表示8个像素，1=白色，0=黑色
 */
void EPD_4in26_Display(UBYTE *Image);

/**
 * @brief 基础显示（带旧数据更新）
 * @details 同时更新新旧数据RAM，用于后续局部刷新的基础
 *          首次显示图像时建议使用此函数
 * @param Image 图像缓冲区指针
 */
void EPD_4in26_Display_Base(UBYTE *Image);

/**
 * @brief 快速显示
 * @details 使用快速刷新模式显示图像
 *          刷新时间约1.5秒，可能有轻微残影
 *          需要先调用EPD_4in26_Init_Fast()初始化
 * @param Image 图像缓冲区指针
 */
void EPD_4in26_Display_Fast(UBYTE *Image);

/**
 * @brief 局部显示
 * @details 只刷新屏幕的指定区域，刷新速度快
 *          适用于只有部分内容需要更新的场景
 *          注意：连续多次局部刷新后建议进行一次全刷清除残影
 * @param Image 局部图像缓冲区指针
 * @param x     显示区域左上角X坐标
 * @param y     显示区域左上角Y坐标
 * @param w     显示区域宽度
 * @param l     显示区域高度
 */
void EPD_4in26_Display_Part(UBYTE *Image, UWORD x, UWORD y, UWORD w, UWORD l);

/**
 * @brief 局部刷新（连续调用版本，不重置EPD）
 * @details 用于连续刷新多个区域时，避免每次都reset
 *          第一个区域应该使用 EPD_4in26_Display_Part
 *          后续区域使用此函数
 */
void EPD_4in26_Display_Part_NoReset(UBYTE *Image, UWORD x, UWORD y, UWORD w, UWORD l);

/**
 * @brief 4级灰度显示
 * @details 显示4级灰度图像
 *          需要先调用EPD_4in26_Init_4GRAY()初始化
 *          每2位表示一个像素的灰度：
 *          - 0x00: 黑色
 *          - 0x40: 深灰
 *          - 0x80: 浅灰
 *          - 0xC0: 白色
 * @param Image 灰度图像缓冲区指针，大小为96000字节（800*480/4）
 */
void EPD_4in26_4GrayDisplay(UBYTE *Image);

/*------------------------------ 电源管理函数 ------------------------------*/

/**
 * @brief 进入睡眠模式
 * @details 让电子墨水屏进入深度睡眠模式以降低功耗
 *          睡眠后屏幕保持最后显示的内容
 *          唤醒需要重新调用初始化函数
 *          
 *          功耗说明：
 *          - 工作模式: 约数十mA
 *          - 睡眠模式: 约几μA
 */
void EPD_4in26_Sleep(void);


#endif /* __EPD_4in26_H_ */
