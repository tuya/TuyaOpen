/******************************************************************************
* | File        :   GUI_Paint.h
* | Author      :   Waveshare electronics
* | Function    :   图形绘制库头文件
* | Info        :   实现基础图形绘制和字符显示功能
*----------------
* | This version:   V3.1
* | Date        :   2019-10-10
******************************************************************************/
#ifndef __GUI_PAINT_H
#define __GUI_PAINT_H

#include "DEV_Config.h"
#include "../Fonts/fonts.h"

/**
 * @brief 图像属性结构体 - 存储图像缓冲区的所有属性
 */
typedef struct {
    UBYTE *Image;           // 图像缓冲区指针
    UWORD Width;            // 显示宽度（像素）
    UWORD Height;           // 显示高度（像素）
    UWORD WidthMemory;      // 内存中的实际宽度
    UWORD HeightMemory;     // 内存中的实际高度
    UWORD Color;            // 背景颜色
    UWORD Rotate;           // 旋转角度
    UWORD Mirror;           // 镜像设置
    UWORD WidthByte;        // 每行字节数
    UWORD HeightByte;       // 总行数
    UWORD Scale;            // 颜色深度(2=黑白,4=4灰度)
} PAINT;
extern PAINT Paint;

/* 旋转角度定义 */
#define ROTATE_0            0
#define ROTATE_90           90
#define ROTATE_180          180
#define ROTATE_270          270

/* 镜像方式枚举 */
typedef enum {
    MIRROR_NONE       = 0x00,   // 不镜像
    MIRROR_HORIZONTAL = 0x01,   // 水平镜像
    MIRROR_VERTICAL   = 0x02,   // 垂直镜像
    MIRROR_ORIGIN     = 0x03,   // 原点镜像
} MIRROR_IMAGE;
#define MIRROR_IMAGE_DFT MIRROR_NONE

/* 颜色定义 */
#define WHITE          0xFF     // 白色
#define BLACK          0x00     // 黑色
#define RED            BLACK

#define IMAGE_BACKGROUND    WHITE
#define FONT_FOREGROUND     BLACK
#define FONT_BACKGROUND     WHITE

/* 4级灰度颜色 */
#define GRAY1 0x03  // 最黑
#define GRAY2 0x02  // 深灰
#define GRAY3 0x01  // 浅灰
#define GRAY4 0x00  // 最白

/* 点大小枚举 */
typedef enum {
    DOT_PIXEL_1X1 = 1,
    DOT_PIXEL_2X2,
    DOT_PIXEL_3X3,
    DOT_PIXEL_4X4,
    DOT_PIXEL_5X5,
    DOT_PIXEL_6X6,
    DOT_PIXEL_7X7,
    DOT_PIXEL_8X8,
} DOT_PIXEL;
#define DOT_PIXEL_DFT  DOT_PIXEL_1X1

/* 点填充样式 */
typedef enum {
    DOT_FILL_AROUND = 1,    // 以点为中心
    DOT_FILL_RIGHTUP,       // 向右上扩展
} DOT_STYLE;
#define DOT_STYLE_DFT  DOT_FILL_AROUND

/* 线条样式 */
typedef enum {
    LINE_STYLE_SOLID = 0,   // 实线
    LINE_STYLE_DOTTED,      // 虚线
} LINE_STYLE;

/* 图形填充方式 */
typedef enum {
    DRAW_FILL_EMPTY = 0,    // 空心
    DRAW_FILL_FULL,         // 实心
} DRAW_FILL;

/* 时间结构体 */
typedef struct {
    UWORD Year;
    UBYTE Month;
    UBYTE Day;
    UBYTE Hour;
    UBYTE Min;
    UBYTE Sec;
} PAINT_TIME;
extern PAINT_TIME sPaint_time;

/* 初始化和清除函数 */
void Paint_NewImage(UBYTE *image, UWORD Width, UWORD Height, UWORD Rotate, UWORD Color);
void Paint_SelectImage(UBYTE *image);
void Paint_SetRotate(UWORD Rotate);
void Paint_SetMirroring(UBYTE mirror);
void Paint_SetPixel(UWORD Xpoint, UWORD Ypoint, UWORD Color);
void Paint_SetScale(UBYTE scale);
void Paint_Clear(UWORD Color);
void Paint_ClearWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color);

/* 图形绘制函数 */
void Paint_DrawPoint(UWORD Xpoint, UWORD Ypoint, UWORD Color, DOT_PIXEL Dot_Pixel, DOT_STYLE Dot_FillWay);
void Paint_DrawLine(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color, DOT_PIXEL Line_width, LINE_STYLE Line_Style);
void Paint_DrawRectangle(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color, DOT_PIXEL Line_width, DRAW_FILL Draw_Fill);
void Paint_DrawCircle(UWORD X_Center, UWORD Y_Center, UWORD Radius, UWORD Color, DOT_PIXEL Line_width, DRAW_FILL Draw_Fill);

/* 字符串显示函数 */
void Paint_DrawChar(UWORD Xstart, UWORD Ystart, const char Acsii_Char, sFONT* Font, UWORD Color_Foreground, UWORD Color_Background);
void Paint_DrawString_EN(UWORD Xstart, UWORD Ystart, const char * pString, sFONT* Font, UWORD Color_Foreground, UWORD Color_Background);
void Paint_DrawString_CN(UWORD Xstart, UWORD Ystart, const char * pString, cFONT* font, UWORD Color_Foreground, UWORD Color_Background);
void Paint_DrawNum(UWORD Xpoint, UWORD Ypoint, int32_t Nummber, sFONT* Font, UWORD Color_Foreground, UWORD Color_Background);
void Paint_DrawNumDecimals(UWORD Xpoint, UWORD Ypoint, double Nummber, sFONT* Font, UWORD Digit, UWORD Color_Foreground, UWORD Color_Background);
void Paint_DrawTime(UWORD Xstart, UWORD Ystart, PAINT_TIME *pTime, sFONT* Font, UWORD Color_Foreground, UWORD Color_Background);

/* 图片显示函数 */
void Paint_DrawBitMap(const unsigned char* image_buffer);

#endif
