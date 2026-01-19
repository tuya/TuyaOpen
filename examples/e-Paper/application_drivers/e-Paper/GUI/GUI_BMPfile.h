/*****************************************************************************
* | File        :   GUI_BMPfile.h
* | Author      :   Waveshare team
* | Function    :   BMP图片文件读取和显示头文件
* | Info        :
*                   本文件定义了BMP文件格式的数据结构和读取函数
*                   支持从文件系统读取BMP图片并显示到墨水屏
*                   
*                   注意：需要文件系统支持（fopen/fread等）
*                         嵌入式系统可能不支持这些函数
*                   
*                   支持的BMP格式：
*                   - 单色位图（1位）
*                   - 4色位图（2位）
*                   - 16色位图（4位）
*                   - 24位RGB位图
*----------------
* | This version:   V2.3
* | Date        :   2022-07-27
******************************************************************************/
#ifndef __GUI_BMPFILE_H
#define __GUI_BMPFILE_H

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "DEV_Config.h"

/*============================================================================
                            BMP文件头结构体（14字节）
============================================================================*/
/**
 * @brief BMP文件头结构体
 * @details 位于BMP文件最开始的14字节，标识文件类型和大小
 * 
 *          成员说明：
 *          - bType:      文件类型标识（"BM"=0x4D42）
 *          - bSize:      文件总大小（字节）
 *          - bReserved1: 保留字段，必须为0
 *          - bReserved2: 保留字段，必须为0
 *          - bOffset:    图像数据起始偏移量
 */
typedef struct BMP_FILE_HEADER {
    UWORD bType;        // 文件类型标识（0x4D42 = "BM"）
    UDOUBLE bSize;      // 文件大小
    UWORD bReserved1;   // 保留字段1
    UWORD bReserved2;   // 保留字段2
    UDOUBLE bOffset;    // 图像数据偏移量
} __attribute__ ((packed)) BMPFILEHEADER;

/*============================================================================
                            BMP信息头结构体（40字节）
============================================================================*/
/**
 * @brief BMP信息头结构体
 * @details 位于文件头之后的40字节，描述图像的详细信息
 */
typedef struct BMP_INFO {
    UDOUBLE biInfoSize;      // 信息头大小（40字节）
    UDOUBLE biWidth;         // 图像宽度（像素）
    UDOUBLE biHeight;        // 图像高度（像素）
    UWORD biPlanes;          // 颜色平面数（通常为1）
    UWORD biBitCount;        // 每像素位数（1/4/8/24等）
    UDOUBLE biCompression;   // 压缩类型（0=不压缩）
    UDOUBLE bimpImageSize;   // 图像数据大小
    UDOUBLE biXPelsPerMeter; // 水平分辨率（像素/米）
    UDOUBLE biYPelsPerMeter; // 垂直分辨率（像素/米）
    UDOUBLE biClrUsed;       // 使用的颜色数
    UDOUBLE biClrImportant;  // 重要颜色数
} __attribute__ ((packed)) BMPINFOHEADER;

/*============================================================================
                            调色板结构体（4字节/条目）
============================================================================*/
/**
 * @brief RGB颜色表结构体（调色板）
 * @details 用于索引色位图，每个条目描述一种颜色
 */
typedef struct RGB_QUAD {
    UBYTE rgbBlue;           // 蓝色分量（0-255）
    UBYTE rgbGreen;          // 绿色分量（0-255）
    UBYTE rgbRed;            // 红色分量（0-255）
    UBYTE rgbReversed;       // 保留字段
} __attribute__ ((packed)) BMPRGBQUAD;

/*============================================================================
                            函数声明
============================================================================*/

/**
 * @brief 读取单色BMP图片
 * @param path   BMP文件路径
 * @param Xstart 显示起始X坐标
 * @param Ystart 显示起始Y坐标
 * @return 0=成功
 */
UBYTE GUI_ReadBmp(const char *path, UWORD Xstart, UWORD Ystart);

/**
 * @brief 读取4色灰度BMP图片
 */
UBYTE GUI_ReadBmp_4Gray(const char *path, UWORD Xstart, UWORD Ystart);

/**
 * @brief 读取16色灰度BMP图片
 */
UBYTE GUI_ReadBmp_16Gray(const char *path, UWORD Xstart, UWORD Ystart);

/**
 * @brief 读取24位RGB 4色BMP图片
 */
UBYTE GUI_ReadBmp_RGB_4Color(const char *path, UWORD Xstart, UWORD Ystart);

/**
 * @brief 读取24位RGB 6色BMP图片
 */
UBYTE GUI_ReadBmp_RGB_6Color(const char *path, UWORD Xstart, UWORD Ystart);

/**
 * @brief 读取24位RGB 7色BMP图片（5.65寸彩色屏用）
 */
UBYTE GUI_ReadBmp_RGB_7Color(const char *path, UWORD Xstart, UWORD Ystart);

#endif
