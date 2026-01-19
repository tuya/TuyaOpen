/******************************************************************************
* | File        :   GUI_Paint.c
* | Author      :   Waveshare electronics
* | Function    :   图形绘制库实现
* | Info        :   实现点、线、矩形、圆等图形绘制和字符显示功能
*----------------
* | This version:   V3.2
* | Date        :   2020-07-23
******************************************************************************/
#include "GUI_Paint.h"
#include "DEV_Config.h"
#include "Debug.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 全局图像属性变量 */
PAINT Paint;

/******************************************************************************
 * @function: Paint_NewImage
 * @brief: 创建新图像并初始化属性
 * @param image  : 图像缓冲区指针
 * @param Width  : 图像宽度（像素）
 * @param Height : 图像高度（像素）
 * @param Rotate : 旋转角度（0/90/180/270）
 * @param Color  : 背景颜色
******************************************************************************/
void Paint_NewImage(UBYTE *image, UWORD Width, UWORD Height, UWORD Rotate, UWORD Color)
{
    Paint.Image = NULL;
    Paint.Image = image;

    Paint.WidthMemory = Width;
    Paint.HeightMemory = Height;
    Paint.Color = Color;    
    Paint.Scale = 2;  // 默认2级色深（黑白）
    
    // 计算每行字节数（8像素=1字节）
    Paint.WidthByte = (Width % 8 == 0) ? (Width / 8) : (Width / 8 + 1);
    Paint.HeightByte = Height;    
   
    Paint.Rotate = Rotate;
    Paint.Mirror = MIRROR_NONE;
    
    // 根据旋转角度设置显示宽高
    if(Rotate == ROTATE_0 || Rotate == ROTATE_180) {
        Paint.Width = Width;
        Paint.Height = Height;
    } else {
        Paint.Width = Height;
        Paint.Height = Width;
    }
}

/******************************************************************************
 * @function: Paint_SelectImage
 * @brief: 选择要绑定的图像缓冲区
 * @param image : 图像缓冲区指针
******************************************************************************/
void Paint_SelectImage(UBYTE *image)
{
    Paint.Image = image;
}

/******************************************************************************
 * @function: Paint_SetRotate
 * @brief: 设置图像旋转角度
 * @param Rotate : 旋转角度（0/90/180/270）
******************************************************************************/
void Paint_SetRotate(UWORD Rotate)
{
    if(Rotate == ROTATE_0 || Rotate == ROTATE_90 || 
       Rotate == ROTATE_180 || Rotate == ROTATE_270) {
        Paint.Rotate = Rotate;
    } else {
        Debug("rotate = 0, 90, 180, 270\r\n");
    }
}

/******************************************************************************
 * @function: Paint_SetMirroring
 * @brief: 设置图像镜像方式
 * @param mirror : 镜像类型
******************************************************************************/
void Paint_SetMirroring(UBYTE mirror)
{
    if(mirror == MIRROR_NONE || mirror == MIRROR_HORIZONTAL || 
        mirror == MIRROR_VERTICAL || mirror == MIRROR_ORIGIN) {
        Paint.Mirror = mirror;
    } else {
        Debug("mirror should be MIRROR_NONE, MIRROR_HORIZONTAL, \
        MIRROR_VERTICAL or MIRROR_ORIGIN\r\n");
    }    
}

/******************************************************************************
 * @function: Paint_SetScale
 * @brief: 设置颜色深度/灰度级别
 * @param scale : 2=黑白, 4=4级灰度, 7=7色
 ******************************************************************************/
void Paint_SetScale(UBYTE scale)
{
    if(scale == 2) {
        Paint.Scale = scale;
        // 黑白模式：8像素=1字节
        Paint.WidthByte = (Paint.WidthMemory % 8 == 0) ? 
                          (Paint.WidthMemory / 8) : (Paint.WidthMemory / 8 + 1);
    } else if(scale == 4) {
        Paint.Scale = scale;
        // 4灰度模式：4像素=1字节（每像素2位）
        Paint.WidthByte = (Paint.WidthMemory % 4 == 0) ? 
                          (Paint.WidthMemory / 4) : (Paint.WidthMemory / 4 + 1);
    } else if(scale == 7) {
        // 7色模式（5.65寸彩色屏）：2像素=1字节
		Paint.Scale = 7;
        Paint.WidthByte = (Paint.WidthMemory % 2 == 0) ? 
                          (Paint.WidthMemory / 2) : (Paint.WidthMemory / 2 + 1);
    } else {
        Debug("Set Scale Input parameter error\r\n");
        Debug("Scale Only support: 2 4 7\r\n");
    }
}

/******************************************************************************
 * @function: Paint_SetPixel
 * @brief: 设置单个像素点颜色（核心绘图函数）
 * @param Xpoint : X坐标
 * @param Ypoint : Y坐标
 * @param Color  : 颜色值
 * @details: 处理旋转、镜像后计算实际内存地址并写入颜色
******************************************************************************/
void Paint_SetPixel(UWORD Xpoint, UWORD Ypoint, UWORD Color)
{
    // 边界检查
    if(Xpoint > Paint.Width || Ypoint > Paint.Height) {
        Debug("Exceeding display boundaries\r\n");
        return;
    }      
    
    UWORD X, Y;
    
    // 根据旋转角度转换坐标
    switch(Paint.Rotate) {
    case 0:
        X = Xpoint;
        Y = Ypoint;  
        break;
    case 90:
        X = Paint.WidthMemory - Ypoint - 1;
        Y = Xpoint;
        break;
    case 180:
        X = Paint.WidthMemory - Xpoint - 1;
        Y = Paint.HeightMemory - Ypoint - 1;
        break;
    case 270:
        X = Ypoint;
        Y = Paint.HeightMemory - Xpoint - 1;
        break;
    default:
        return;
    }
    
    // 根据镜像设置转换坐标
    switch(Paint.Mirror) {
    case MIRROR_NONE:
        break;
    case MIRROR_HORIZONTAL:
        X = Paint.WidthMemory - X - 1;
        break;
    case MIRROR_VERTICAL:
        Y = Paint.HeightMemory - Y - 1;
        break;
    case MIRROR_ORIGIN:
        X = Paint.WidthMemory - X - 1;
        Y = Paint.HeightMemory - Y - 1;
        break;
    default:
        return;
    }

    // 再次边界检查
    if(X > Paint.WidthMemory || Y > Paint.HeightMemory) {
        Debug("Exceeding display boundaries\r\n");
        return;
    }
    
    // 根据颜色深度写入像素
    if(Paint.Scale == 2) {
        // 黑白模式：每字节8像素
        UDOUBLE Addr = X / 8 + Y * Paint.WidthByte;
        UBYTE Rdata = Paint.Image[Addr];
        if(Color == BLACK)
            Paint.Image[Addr] = Rdata & ~(0x80 >> (X % 8));
        else
            Paint.Image[Addr] = Rdata | (0x80 >> (X % 8));
    } else if(Paint.Scale == 4) {
        // 4灰度模式：每字节4像素
        UDOUBLE Addr = X / 4 + Y * Paint.WidthByte;
        Color = Color % 4;  // 确保颜色在0-3范围
        UBYTE Rdata = Paint.Image[Addr];
        Rdata = Rdata & (~(0xC0 >> ((X % 4) * 2)));
        Paint.Image[Addr] = Rdata | ((Color << 6) >> ((X % 4) * 2));
    } else if(Paint.Scale == 7 || Paint.Scale == 16) {
        // 7色/16色模式：每字节2像素
        UDOUBLE Addr = X / 2 + Y * Paint.WidthByte;
		UBYTE Rdata = Paint.Image[Addr];
        Rdata = Rdata & (~(0xF0 >> ((X % 2) * 4)));
        Paint.Image[Addr] = Rdata | ((Color << 4) >> ((X % 2) * 4));
    }
}

/******************************************************************************
 * @function: Paint_Clear
 * @brief: 清空整个图像缓冲区
 * @param Color : 填充颜色
******************************************************************************/
void Paint_Clear(UWORD Color)
{
    if(Paint.Scale == 2) {
        // 黑白模式：直接填充
		for (UWORD Y = 0; Y < Paint.HeightByte; Y++) {
            for (UWORD X = 0; X < Paint.WidthByte; X++) {
                UDOUBLE Addr = X + Y * Paint.WidthByte;
				Paint.Image[Addr] = Color;
			}
		}
    } else if(Paint.Scale == 4) {
        // 4灰度模式：每字节填充4个相同灰度
        for (UWORD Y = 0; Y < Paint.HeightByte; Y++) {
            for (UWORD X = 0; X < Paint.WidthByte; X++) {
                UDOUBLE Addr = X + Y * Paint.WidthByte;
                Paint.Image[Addr] = (Color << 6) | (Color << 4) | (Color << 2) | Color;
            }
        }
    } else if(Paint.Scale == 7 || Paint.Scale == 16) {
        // 7色模式：每字节填充2个相同颜色
		for (UWORD Y = 0; Y < Paint.HeightByte; Y++) {
            for (UWORD X = 0; X < Paint.WidthByte; X++) {
                UDOUBLE Addr = X + Y * Paint.WidthByte;
                Paint.Image[Addr] = (Color << 4) | Color;
			}
		}		
	}
}

/******************************************************************************
 * @function: Paint_ClearWindows
 * @brief: 清空指定窗口区域
******************************************************************************/
void Paint_ClearWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color)
{
    UWORD X, Y;
    for (Y = Ystart; Y < Yend; Y++) {
        for (X = Xstart; X < Xend; X++) {
            Paint_SetPixel(X, Y, Color);
        }
    }
}

/******************************************************************************
 * @function: Paint_DrawPoint
 * @brief: 绘制指定大小的点
 * @param Xpoint     : X坐标
 * @param Ypoint     : Y坐标
 * @param Color      : 颜色
 * @param Dot_Pixel  : 点大小（1x1到8x8）
 * @param Dot_Style  : 填充方式
******************************************************************************/
void Paint_DrawPoint(UWORD Xpoint, UWORD Ypoint, UWORD Color,
                     DOT_PIXEL Dot_Pixel, DOT_STYLE Dot_Style)
{
    if (Xpoint > Paint.Width || Ypoint > Paint.Height) {
        Debug("Paint_DrawPoint Input exceeds the normal display range\r\n");
        return;
    }

    int16_t XDir_Num, YDir_Num;
    if (Dot_Style == DOT_FILL_AROUND) {
        // 以点为中心向四周扩展
        for (XDir_Num = 0; XDir_Num < 2 * Dot_Pixel - 1; XDir_Num++) {
            for (YDir_Num = 0; YDir_Num < 2 * Dot_Pixel - 1; YDir_Num++) {
                if(Xpoint + XDir_Num - Dot_Pixel < 0 || 
                   Ypoint + YDir_Num - Dot_Pixel < 0)
                    break;
                Paint_SetPixel(Xpoint + XDir_Num - Dot_Pixel, 
                              Ypoint + YDir_Num - Dot_Pixel, Color);
            }
        }
    } else {
        // 从点向右下扩展
        for (XDir_Num = 0; XDir_Num < Dot_Pixel; XDir_Num++) {
            for (YDir_Num = 0; YDir_Num < Dot_Pixel; YDir_Num++) {
                Paint_SetPixel(Xpoint + XDir_Num - 1, 
                              Ypoint + YDir_Num - 1, Color);
            }
        }
    }
}

/******************************************************************************
 * @function: Paint_DrawLine
 * @brief: 绘制任意斜率的直线（Bresenham算法）
 * @param Xstart, Ystart : 起点坐标
 * @param Xend, Yend     : 终点坐标
 * @param Color          : 颜色
 * @param Line_width     : 线宽
 * @param Line_Style     : 线型（实线/虚线）
******************************************************************************/
void Paint_DrawLine(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend,
                    UWORD Color, DOT_PIXEL Line_width, LINE_STYLE Line_Style)
{
    if (Xstart > Paint.Width || Ystart > Paint.Height ||
        Xend > Paint.Width || Yend > Paint.Height) {
        Debug("Paint_DrawLine Input exceeds the normal display range\r\n");
        return;
    }

    UWORD Xpoint = Xstart;
    UWORD Ypoint = Ystart;
    int dx = (int)Xend - (int)Xstart >= 0 ? Xend - Xstart : Xstart - Xend;
    int dy = (int)Yend - (int)Ystart <= 0 ? Yend - Ystart : Ystart - Yend;

    // 方向增量
    int XAddway = Xstart < Xend ? 1 : -1;
    int YAddway = Ystart < Yend ? 1 : -1;

    // 累积误差
    int Esp = dx + dy;
    char Dotted_Len = 0;

    for (;;) {
        Dotted_Len++;
        // 虚线：每3个点画2个
        if (Line_Style == LINE_STYLE_DOTTED && Dotted_Len % 3 == 0) {
            Paint_DrawPoint(Xpoint, Ypoint, IMAGE_BACKGROUND, Line_width, DOT_STYLE_DFT);
            Dotted_Len = 0;
        } else {
            Paint_DrawPoint(Xpoint, Ypoint, Color, Line_width, DOT_STYLE_DFT);
        }
        
        if (2 * Esp >= dy) {
            if (Xpoint == Xend)
                break;
            Esp += dy;
            Xpoint += XAddway;
        }
        if (2 * Esp <= dx) {
            if (Ypoint == Yend)
                break;
            Esp += dx;
            Ypoint += YAddway;
        }
    }
}

/******************************************************************************
 * @function: Paint_DrawRectangle
 * @brief: 绘制矩形
 * @param Xstart, Ystart : 左上角坐标
 * @param Xend, Yend     : 右下角坐标
 * @param Color          : 颜色
 * @param Line_width     : 线宽
 * @param Draw_Fill      : 空心/实心
******************************************************************************/
void Paint_DrawRectangle(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend,
                         UWORD Color, DOT_PIXEL Line_width, DRAW_FILL Draw_Fill)
{
    if (Xstart > Paint.Width || Ystart > Paint.Height ||
        Xend > Paint.Width || Yend > Paint.Height) {
        Debug("Input exceeds the normal display range\r\n");
        return;
    }

    if (Draw_Fill) {
        // 实心矩形：填充横线
        UWORD Ypoint;
        for(Ypoint = Ystart; Ypoint < Yend; Ypoint++) {
            Paint_DrawLine(Xstart, Ypoint, Xend, Ypoint, Color, Line_width, LINE_STYLE_SOLID);
        }
    } else {
        // 空心矩形：四条边
        Paint_DrawLine(Xstart, Ystart, Xend, Ystart, Color, Line_width, LINE_STYLE_SOLID);
        Paint_DrawLine(Xstart, Ystart, Xstart, Yend, Color, Line_width, LINE_STYLE_SOLID);
        Paint_DrawLine(Xend, Yend, Xend, Ystart, Color, Line_width, LINE_STYLE_SOLID);
        Paint_DrawLine(Xend, Yend, Xstart, Yend, Color, Line_width, LINE_STYLE_SOLID);
    }
}

/******************************************************************************
 * @function: Paint_DrawCircle
 * @brief: 绘制圆形（8点对称算法）
 * @param X_Center, Y_Center : 圆心坐标
 * @param Radius             : 半径
 * @param Color              : 颜色
 * @param Line_width         : 线宽
 * @param Draw_Fill          : 空心/实心
******************************************************************************/
void Paint_DrawCircle(UWORD X_Center, UWORD Y_Center, UWORD Radius,
                      UWORD Color, DOT_PIXEL Line_width, DRAW_FILL Draw_Fill)
{
    if (X_Center > Paint.Width || Y_Center >= Paint.Height) {
        Debug("Paint_DrawCircle Input exceeds the normal display range\r\n");
        return;
    }

    // 从(0, R)开始画圆
    int16_t XCurrent, YCurrent;
    XCurrent = 0;
    YCurrent = Radius;

    // 累积误差
    int16_t Esp = 3 - (Radius << 1);

    int16_t sCountY;
    if (Draw_Fill == DRAW_FILL_FULL) {
        // 实心圆
        while (XCurrent <= YCurrent) {
            for (sCountY = XCurrent; sCountY <= YCurrent; sCountY++) {
                // 8个对称点
                Paint_DrawPoint(X_Center + XCurrent, Y_Center + sCountY, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                Paint_DrawPoint(X_Center - XCurrent, Y_Center + sCountY, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                Paint_DrawPoint(X_Center - sCountY, Y_Center + XCurrent, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                Paint_DrawPoint(X_Center - sCountY, Y_Center - XCurrent, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                Paint_DrawPoint(X_Center - XCurrent, Y_Center - sCountY, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                Paint_DrawPoint(X_Center + XCurrent, Y_Center - sCountY, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                Paint_DrawPoint(X_Center + sCountY, Y_Center - XCurrent, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                Paint_DrawPoint(X_Center + sCountY, Y_Center + XCurrent, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);
            }
            if (Esp < 0)
                Esp += 4 * XCurrent + 6;
            else {
                Esp += 10 + 4 * (XCurrent - YCurrent);
                YCurrent--;
            }
            XCurrent++;
        }
    } else {
        // 空心圆
        while (XCurrent <= YCurrent) {
            Paint_DrawPoint(X_Center + XCurrent, Y_Center + YCurrent, Color, Line_width, DOT_STYLE_DFT);
            Paint_DrawPoint(X_Center - XCurrent, Y_Center + YCurrent, Color, Line_width, DOT_STYLE_DFT);
            Paint_DrawPoint(X_Center - YCurrent, Y_Center + XCurrent, Color, Line_width, DOT_STYLE_DFT);
            Paint_DrawPoint(X_Center - YCurrent, Y_Center - XCurrent, Color, Line_width, DOT_STYLE_DFT);
            Paint_DrawPoint(X_Center - XCurrent, Y_Center - YCurrent, Color, Line_width, DOT_STYLE_DFT);
            Paint_DrawPoint(X_Center + XCurrent, Y_Center - YCurrent, Color, Line_width, DOT_STYLE_DFT);
            Paint_DrawPoint(X_Center + YCurrent, Y_Center - XCurrent, Color, Line_width, DOT_STYLE_DFT);
            Paint_DrawPoint(X_Center + YCurrent, Y_Center + XCurrent, Color, Line_width, DOT_STYLE_DFT);

            if (Esp < 0)
                Esp += 4 * XCurrent + 6;
            else {
                Esp += 10 + 4 * (XCurrent - YCurrent);
                YCurrent--;
            }
            XCurrent++;
        }
    }
}

/******************************************************************************
 * @function: Paint_DrawChar
 * @brief: 绘制单个英文字符
 * @param Xpoint, Ypoint     : 坐标
 * @param Acsii_Char         : ASCII字符
 * @param Font               : 字体
 * @param Color_Foreground   : 前景色
 * @param Color_Background   : 背景色
******************************************************************************/
void Paint_DrawChar(UWORD Xpoint, UWORD Ypoint, const char Acsii_Char,
                    sFONT* Font, UWORD Color_Foreground, UWORD Color_Background)
{
    UWORD Page, Column;

    if (Xpoint > Paint.Width || Ypoint > Paint.Height) {
        Debug("Paint_DrawChar Input exceeds the normal display range\r\n");
        return;
    }

    // 计算字符在字体表中的偏移
    uint32_t Char_Offset = (Acsii_Char - ' ') * Font->Height * 
                           (Font->Width / 8 + (Font->Width % 8 ? 1 : 0));
    const unsigned char *ptr = &Font->table[Char_Offset];

    // 逐行逐列绘制
    for (Page = 0; Page < Font->Height; Page++) {
        for (Column = 0; Column < Font->Width; Column++) {
            if (FONT_BACKGROUND == Color_Background) {
                // 透明背景：只绘制前景
                if (*ptr & (0x80 >> (Column % 8)))
                    Paint_SetPixel(Xpoint + Column, Ypoint + Page, Color_Foreground);
            } else {
                // 不透明背景
                if (*ptr & (0x80 >> (Column % 8))) {
                    Paint_SetPixel(Xpoint + Column, Ypoint + Page, Color_Foreground);
                } else {
                    Paint_SetPixel(Xpoint + Column, Ypoint + Page, Color_Background);
                }
            }
            if (Column % 8 == 7)
                ptr++;
        }
        if (Font->Width % 8 != 0)
            ptr++;
    }
}

/******************************************************************************
 * @function: Paint_DrawString_EN
 * @brief: 绘制英文字符串
******************************************************************************/
void Paint_DrawString_EN(UWORD Xstart, UWORD Ystart, const char * pString,
                         sFONT* Font, UWORD Color_Foreground, UWORD Color_Background)
{
    UWORD Xpoint = Xstart;
    UWORD Ypoint = Ystart;

    if (Xstart > Paint.Width || Ystart > Paint.Height) {
        Debug("Paint_DrawString_EN Input exceeds the normal display range\r\n");
        return;
    }

    while (*pString != '\0') {
        // X方向填满则换行
        if ((Xpoint + Font->Width) > Paint.Width) {
            Xpoint = Xstart;
            Ypoint += Font->Height;
        }
        // Y方向填满则回到起点
        if ((Ypoint + Font->Height) > Paint.Height) {
            Xpoint = Xstart;
            Ypoint = Ystart;
        }
        Paint_DrawChar(Xpoint, Ypoint, *pString, Font, Color_Background, Color_Foreground);
        pString++;
        Xpoint += Font->Width;
    }
}

/******************************************************************************
 * @function: Paint_DrawString_CN
 * @brief: 绘制中英文混合字符串
 * @details: 支持UTF-8编码的中文和ASCII英文
******************************************************************************/
void Paint_DrawString_CN(UWORD Xstart, UWORD Ystart, const char * pString, cFONT* font,
                        UWORD Color_Foreground, UWORD Color_Background)
{
    const char* p_text = pString;
    int x = Xstart, y = Ystart;
    int i, j, Num;

    while (*p_text != 0) {
        if(((unsigned char)*p_text) <= 0x7F) {
            // ASCII字符（单字节）
            for(Num = 0; Num < font->size; Num++) {
                if(*p_text == font->table[Num].index[0]) {
                    const char* ptr = &font->table[Num].matrix[0];
                    for (j = 0; j < font->Height; j++) {
                        for (i = 0; i < font->Width; i++) {
                            if (FONT_BACKGROUND == Color_Background) {
                                if (*ptr & (0x80 >> (i % 8))) {
                                    Paint_SetPixel(x + i, y + j, Color_Foreground);
                                }
                            } else {
                                if (*ptr & (0x80 >> (i % 8))) {
                                    Paint_SetPixel(x + i, y + j, Color_Foreground);
                                } else {
                                    Paint_SetPixel(x + i, y + j, Color_Background);
                                }
                            }
                            if (i % 8 == 7) ptr++;
                        }
                        if (font->Width % 8 != 0) ptr++;
                    }
                    break;
                }
            }
            p_text += 1;
            x += font->ASCII_Width;
        } else {
            // 中文字符（UTF-8 3字节）
            for(Num = 0; Num < font->size; Num++) {
                if (((unsigned char)p_text[0] == (unsigned char)font->table[Num].index[0]) &&
                    ((unsigned char)*(p_text + 1) == (unsigned char)font->table[Num].index[1]) &&
                    ((unsigned char)*(p_text + 2) == (unsigned char)font->table[Num].index[2])) {
                    const char* ptr = &font->table[Num].matrix[0];
                    for (j = 0; j < font->Height; j++) {
                        for (i = 0; i < font->Width; i++) {
                            if (FONT_BACKGROUND == Color_Background) {
                                if (*ptr & (0x80 >> (i % 8))) {
                                    Paint_SetPixel(x + i, y + j, Color_Foreground);
                                }
                            } else {
                                if (*ptr & (0x80 >> (i % 8))) {
                                    Paint_SetPixel(x + i, y + j, Color_Foreground);
                                } else {
                                    Paint_SetPixel(x + i, y + j, Color_Background);
                                }
                            }
                            if (i % 8 == 7) ptr++;
                        }
                        if (font->Width % 8 != 0) ptr++;
                    }
                    break;
                }
            }
            p_text += 3;
            x += font->Width;
        }
    }
}

/******************************************************************************
 * @function: Paint_DrawNum
 * @brief: 绘制整数数字
******************************************************************************/
#define ARRAY_LEN 255
void Paint_DrawNum(UWORD Xpoint, UWORD Ypoint, int32_t Nummber,
                   sFONT* Font, UWORD Color_Foreground, UWORD Color_Background)
{
    int16_t Num_Bit = 0, Str_Bit = 0;
    uint8_t Str_Array[ARRAY_LEN] = {0}, Num_Array[ARRAY_LEN] = {0};
    uint8_t *pStr = Str_Array;

    if (Xpoint > Paint.Width || Ypoint > Paint.Height) {
        Debug("Paint_DisNum Input exceeds the normal display range\r\n");
        return;
    }

    // 数字转字符串（逆序）
    while (Nummber) {
        Num_Array[Num_Bit] = Nummber % 10 + '0';
        Num_Bit++;
        Nummber /= 10;
    }

    // 反转字符串
    while (Num_Bit > 0) {
        Str_Array[Str_Bit] = Num_Array[Num_Bit - 1];
        Str_Bit++;
        Num_Bit--;
    }

    Paint_DrawString_EN(Xpoint, Ypoint, (const char*)pStr, Font, Color_Background, Color_Foreground);
}

/******************************************************************************
 * @function: Paint_DrawTime
 * @brief: 绘制时间（HH:MM:SS格式）
******************************************************************************/
void Paint_DrawTime(UWORD Xstart, UWORD Ystart, PAINT_TIME *pTime, sFONT* Font,
                    UWORD Color_Foreground, UWORD Color_Background)
{
    uint8_t value[10] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
    UWORD Dx = Font->Width;

    // 绘制 HH:MM:SS
    Paint_DrawChar(Xstart, Ystart, value[pTime->Hour / 10], Font, Color_Background, Color_Foreground);
    Paint_DrawChar(Xstart + Dx, Ystart, value[pTime->Hour % 10], Font, Color_Background, Color_Foreground);
    Paint_DrawChar(Xstart + Dx + Dx / 4 + Dx / 2, Ystart, ':', Font, Color_Background, Color_Foreground);
    Paint_DrawChar(Xstart + Dx * 2 + Dx / 2, Ystart, value[pTime->Min / 10], Font, Color_Background, Color_Foreground);
    Paint_DrawChar(Xstart + Dx * 3 + Dx / 2, Ystart, value[pTime->Min % 10], Font, Color_Background, Color_Foreground);
    Paint_DrawChar(Xstart + Dx * 4 + Dx / 2 - Dx / 4, Ystart, ':', Font, Color_Background, Color_Foreground);
    Paint_DrawChar(Xstart + Dx * 5, Ystart, value[pTime->Sec / 10], Font, Color_Background, Color_Foreground);
    Paint_DrawChar(Xstart + Dx * 6, Ystart, value[pTime->Sec % 10], Font, Color_Background, Color_Foreground);
}

/******************************************************************************
 * @function: Paint_DrawBitMap
 * @brief: 绘制位图图片
 * @param image_buffer : 图片数据数组
******************************************************************************/
void Paint_DrawBitMap(const unsigned char* image_buffer)
{
    UWORD x, y;
    UDOUBLE Addr = 0;

    for (y = 0; y < Paint.HeightByte; y++) {
        for (x = 0; x < Paint.WidthByte; x++) {
            Addr = x + y * Paint.WidthByte;
            Paint.Image[Addr] = (unsigned char)image_buffer[Addr];
        }
    }
}

/******************************************************************************
 * @function: Paint_DrawImage
 * @brief: 在指定位置绘制图片
******************************************************************************/
void Paint_DrawImage(const unsigned char *image_buffer, UWORD xStart, UWORD yStart, 
                     UWORD W_Image, UWORD H_Image) 
{
    UWORD x, y;
    UWORD w_byte = (W_Image % 8) ? (W_Image / 8) + 1 : W_Image / 8;
    UDOUBLE Addr = 0;
	UDOUBLE pAddr = 0;
    
    for (y = 0; y < H_Image; y++) {
        for (x = 0; x < w_byte; x++) {
            Addr = x + y * w_byte;
            pAddr = x + (xStart / 8) + ((y + yStart) * Paint.WidthByte);
            Paint.Image[pAddr] = (unsigned char)image_buffer[Addr];
        }
    }
}
