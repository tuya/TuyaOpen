/**
  ******************************************************************************
  * @file    fonts.h
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    18-February-2014
  * @brief   字体数据头文件
  * 
  *          本文件定义了用于电子墨水屏显示的字体数据结构和字体声明
  *          
  *          支持的字体：
  *          - 英文ASCII字体：Font8, Font12, Font16, Font20, Font24
  *          - 中文字体：Font12CN, Font24CN（UTF-8编码）
  *          
  *          字体数据格式：
  *          - 每个字符由点阵数据表示
  *          - 每个像素占1位，8像素=1字节
  *          - 数据按行存储，高位在左
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2014 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* 防止头文件重复包含 */
#ifndef __FONTS_H
#define __FONTS_H

/*============================================================================
                            字体尺寸限制定义
============================================================================*/
/**
 * @brief 字体最大尺寸定义
 * @details 定义字体点阵数据的最大尺寸限制
 *          用于静态分配中文字符点阵数组大小
 *          
 *          MAX_HEIGHT_FONT: 最大字体高度（像素）
 *          MAX_WIDTH_FONT:  最大字体宽度（像素）
 *          OFFSET_BITMAP:   BMP文件头偏移量
 */
#define MAX_HEIGHT_FONT         41      // 最大字体高度：41像素
#define MAX_WIDTH_FONT          32      // 最大字体宽度：32像素
#define OFFSET_BITMAP           54      // BMP文件数据偏移量

#ifdef __cplusplus
 extern "C" {
#endif

/*============================================================================
                            头文件包含
============================================================================*/
#include <stdint.h>     // 标准整数类型定义

/*============================================================================
                            ASCII字体结构体定义
============================================================================*/
/**
 * @brief ASCII字体结构体
 * @details 用于存储ASCII字符集（英文、数字、符号）的字体信息
 *          
 *          成员说明：
 *          - table:  点阵数据表指针，存储所有字符的点阵数据
 *          - Width:  单个字符宽度（像素）
 *          - Height: 单个字符高度（像素）
 *          
 *          点阵数据存储格式：
 *          - 按ASCII码顺序存储，从空格(' '=0x20)开始
 *          - 每个字符占用 Height * ((Width+7)/8) 字节
 *          - 每行按字节对齐，高位在左
 *          
 *          使用示例：
 *          要获取字符'A'的点阵数据：
 *          offset = ('A' - ' ') * Height * ((Width+7)/8)
 *          data = &table[offset]
 */
typedef struct _tFont
{    
    const uint8_t *table;   // 点阵数据表指针
    uint16_t Width;         // 字符宽度（像素）
    uint16_t Height;        // 字符高度（像素）
} sFONT;

/*============================================================================
                            中文字体结构体定义
============================================================================*/
/**
 * @brief 中文字符数据结构体
 * @details 存储单个中文字符的索引和点阵数据
 *          
 *          成员说明：
 *          - index:  字符索引（UTF-8编码，最多4字节）
 *          - matrix: 点阵数据数组
 *          
 *          UTF-8编码说明：
 *          - ASCII字符：1字节（0x00-0x7F）
 *          - 中文字符：3字节（0xE0-0xEF开头）
 *          - index[3]用于字符串结束符'\0'
 */
typedef struct
{
    unsigned char index[4];                                 // UTF-8字符索引（3字节+结束符）
    const char matrix[MAX_HEIGHT_FONT * MAX_WIDTH_FONT / 8]; // 点阵数据
} CH_CN;

/**
 * @brief 中文字体结构体
 * @details 存储整个中文字体的信息，包含字符表和尺寸信息
 *          
 *          成员说明：
 *          - table:       字符数据表指针（CH_CN数组）
 *          - size:        字符表中的字符数量
 *          - ASCII_Width: ASCII字符宽度（半角）
 *          - Width:       中文字符宽度（全角）
 *          - Height:      字符高度
 *          
 *          使用说明：
 *          - 中文字体通常包含常用汉字和ASCII字符
 *          - ASCII字符宽度通常是中文字符宽度的一半
 *          - 查找字符时需要遍历table数组匹配index
 */
typedef struct
{    
    const CH_CN *table;     // 字符数据表指针
    uint16_t size;          // 字符数量
    uint16_t ASCII_Width;   // ASCII字符宽度（像素）
    uint16_t Width;         // 中文字符宽度（像素）
    uint16_t Height;        // 字符高度（像素）
} cFONT;

/*============================================================================
                            字体变量声明
============================================================================*/
/**
 * @brief ASCII字体声明
 * @details 提供5种不同大小的ASCII字体
 *          
 *          字体规格：
 *          ┌─────────┬───────┬────────┬──────────────────────┐
 *          │  字体   │ 宽度  │ 高度   │       适用场景       │
 *          ├─────────┼───────┼────────┼──────────────────────┤
 *          │ Font8   │  5px  │  8px   │ 小型标签、密集文字   │
 *          │ Font12  │  7px  │  12px  │ 小型文字、注释       │
 *          │ Font16  │  11px │  16px  │ 正常文字显示         │
 *          │ Font20  │  14px │  20px  │ 较大文字、标题       │
 *          │ Font24  │  17px │  24px  │ 大标题、重要信息     │
 *          └─────────┴───────┴────────┴──────────────────────┘
 */
extern sFONT Font24;    // 24像素高度字体（17x24）
extern sFONT Font20;    // 20像素高度字体（14x20）
extern sFONT Font16;    // 16像素高度字体（11x16）
extern sFONT Font12;    // 12像素高度字体（7x12）
extern sFONT Font8;     // 8像素高度字体（5x8）

/**
 * @brief 中文字体声明
 * @details 提供2种中文字体，支持UTF-8编码的汉字显示
 *          
 *          字体规格：
 *          ┌───────────┬────────┬────────┬──────────────────┐
 *          │   字体    │ 中文宽 │  高度  │     适用场景     │
 *          ├───────────┼────────┼────────┼──────────────────┤
 *          │ Font12CN  │  12px  │  12px  │ 小型中文显示     │
 *          │ Font24CN  │  24px  │  24px  │ 正常中文显示     │
 *          └───────────┴────────┴────────┴──────────────────┘
 *          
 *          注意：中文字体只包含预定义的常用汉字
 *                需要显示的汉字必须在字体表中存在
 */
extern cFONT Font12CN;  // 12像素中文字体
extern cFONT Font24CN;  // 24像素中文字体

#ifdef __cplusplus
}
#endif
  
#endif /* __FONTS_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
