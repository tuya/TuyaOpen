/*****************************************************************************
* | File      	:   EPD_4in26_test.c
* | Author      :   Waveshare team
* | Function    :   4.26英寸电子墨水屏测试Demo
* | Info        :   
*                   本文件实现了4.26英寸墨水屏的综合功能测试
*                   演示了墨水屏的各种显示功能和使用方法
*                   
*                   测试流程：
*                   1. 模块初始化和清屏
*                   2. 快速模式显示预置图片
*                   3. 绘制各种图形（点、线、矩形、圆）
*                   4. 显示中英文文字和数字
*                   5. 局部刷新演示（动态时钟）
*                   6. 4级灰度显示演示
*                   7. 进入睡眠模式
*----------------
* |	This version:   V1.0
* | Date        :   2023-12-19
* | Info        :
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
#include "EPD_Test.h"       // 测试头文件
#include "EPD_4in26.h"      // 4.26英寸墨水屏驱动

/**
 * @function: EPD_test
 * @brief: 4.26英寸墨水屏综合测试函数
 * @details: 
 *           演示墨水屏的完整功能，包括初始化、显示、刷新、睡眠等
 *           
 *           内存需求说明：
 *           - 黑白模式: 800*480/8 = 48,000字节
 *           - 灰度模式: 800*480/4 = 96,000字节
 *           
 *           测试耗时说明：
 *           - 标准刷新: 约4秒
 *           - 快速刷新: 约1.5秒
 *           - 局部刷新: 约0.3秒
 *           - 完整测试: 约2分钟
 * 
 * @return: int
 *          - 0:  测试成功
 *          - -1: 测试失败
 */
int EPD_test(void)
{
    printf("EPD_4in26_test Demo\r\n");
    
    /*========================================================================
                    第1步：硬件初始化
    ========================================================================*/
    // 初始化硬件模块（SPI、GPIO等）
    if(DEV_Module_Init() != 0) {
        return -1;  // 初始化失败
    }

    /*========================================================================
                    第2步：墨水屏初始化和清屏
    ========================================================================*/
    printf("e-Paper Init and Clear...\r\n");
    EPD_4in26_Init();   // 标准模式初始化
    EPD_4in26_Clear();  // 清屏（显示全白）
    DEV_Delay_ms(500);  // 等待500ms

    /*========================================================================
                    第3步：创建图像缓冲区
    ========================================================================*/
    UBYTE *BlackImage;  // 图像缓冲区指针
    
    // 计算图像缓冲区大小
    // 每8个像素占用1字节，需要向上取整
    UDOUBLE Imagesize = ((EPD_4in26_WIDTH % 8 == 0) ? 
                         (EPD_4in26_WIDTH / 8) : 
                         (EPD_4in26_WIDTH / 8 + 1)) * EPD_4in26_HEIGHT;
    
    // 动态分配图像缓冲区内存
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        return -1;  // 内存分配失败
    }
    
    // 创建新图像（设置缓冲区、尺寸、旋转角度、背景色）
    printf("Paint_NewImage\r\n");
    Paint_NewImage(BlackImage, EPD_4in26_WIDTH, EPD_4in26_HEIGHT, 0, WHITE);

    /*========================================================================
                    第4步：快速模式显示预置图片
    ========================================================================*/
#if 1
    // 使用快速模式初始化（刷新时间约1.5秒）
    EPD_4in26_Init_Fast();
    printf("show image for array\r\n");
    
    // 选择图像缓冲区并清空
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
    
    // 绘制预置的位图图片（gImage_4in26定义在ImageData.c中）
    Paint_DrawBitMap(gImage_4in26);
    
    // 快速显示图片
    EPD_4in26_Display_Fast(BlackImage);
    DEV_Delay_ms(2000);  // 显示2秒
#endif

    /*========================================================================
                    第5步：图形和文字绘制演示
    ========================================================================*/
#if 1
    // 重新初始化为标准模式（显示质量更好）
    EPD_4in26_Init();
    
    // 选择图像缓冲区
    printf("SelectImage:BlackImage\r\n");
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);  // 清空为白色背景

    // 绘制图形演示
    printf("Drawing:BlackImage\r\n");
    
    /* 绘制点（不同大小） */
    Paint_DrawPoint(10, 80, BLACK, DOT_PIXEL_1X1, DOT_STYLE_DFT);   // 1x1像素的点
    Paint_DrawPoint(10, 90, BLACK, DOT_PIXEL_2X2, DOT_STYLE_DFT);   // 2x2像素的点
    Paint_DrawPoint(10, 100, BLACK, DOT_PIXEL_3X3, DOT_STYLE_DFT);  // 3x3像素的点
    
    /* 绘制直线 */
    // 实线：从(20,70)到(70,120)
    Paint_DrawLine(20, 70, 70, 120, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    // 实线：从(70,70)到(20,120)，与上面的线交叉形成X
    Paint_DrawLine(70, 70, 20, 120, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    
    /* 绘制矩形 */
    // 空心矩形：左上角(20,70)，右下角(70,120)
    Paint_DrawRectangle(20, 70, 70, 120, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    // 实心矩形：左上角(80,70)，右下角(130,120)
    Paint_DrawRectangle(80, 70, 130, 120, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    
    /* 绘制圆形 */
    // 空心圆：圆心(45,95)，半径20
    Paint_DrawCircle(45, 95, 20, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    // 实心白圆：圆心(105,95)，半径20（在黑色矩形上显示白色圆）
    Paint_DrawCircle(105, 95, 20, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    
    /* 绘制虚线 */
    // 水平虚线
    Paint_DrawLine(85, 95, 125, 95, BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    // 垂直虚线
    Paint_DrawLine(105, 75, 105, 115, BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    
    /* 绘制英文字符串 */
    // 16号字体，黑字白底
    Paint_DrawString_EN(10, 1, "waveshare", &Font16, BLACK, WHITE);
    // 12号字体，白字黑底（反色显示）
    Paint_DrawString_EN(10, 20, "hello world", &Font12, WHITE, BLACK);
    
    /* 绘制数字 */
    Paint_DrawNum(10, 33, 123456789, &Font12, BLACK, WHITE);
    Paint_DrawNum(10, 50, 987654321, &Font16, WHITE, BLACK);
    
    /* 绘制中文字符串 */
    Paint_DrawString_CN(130, 1, "你好abc", &Font12CN, BLACK, WHITE);
    Paint_DrawString_CN(130, 20, "微雪电子", &Font24CN, WHITE, BLACK);

    // 显示绘制结果
    printf("EPD_Display\r\n");
    // 使用Base显示，同时更新新旧RAM（为后续局部刷新做准备）
    EPD_4in26_Display_Base(BlackImage);
    DEV_Delay_ms(2000);
#endif

    /*========================================================================
                    第6步：局部刷新演示（动态时钟）
    ========================================================================*/
#if 1
    printf("Partial refresh\r\n");
    
    // 为局部刷新创建较小的图像缓冲区
    Paint_NewImage(BlackImage, 200, 50, 0, WHITE);
    
    // 初始化时间结构体
    PAINT_TIME sPaint_time;
    sPaint_time.Hour = 12;
    sPaint_time.Min = 34;
    sPaint_time.Sec = 56;
    
    UBYTE num = 10;  // 刷新次数
    
    // 循环更新时钟显示
    for (;;) {
        // 秒数递增
        sPaint_time.Sec = sPaint_time.Sec + 1;
        
        // 处理进位（秒->分->时）
        if (sPaint_time.Sec == 60) {
            sPaint_time.Min = sPaint_time.Min + 1;
            sPaint_time.Sec = 0;
            if (sPaint_time.Min == 60) {
                sPaint_time.Hour = sPaint_time.Hour + 1;
                sPaint_time.Min = 0;
                if (sPaint_time.Hour == 24) {
                    sPaint_time.Hour = 0;
                    sPaint_time.Min = 0;
                    sPaint_time.Sec = 0;
                }
            }
        }
        
        // 清空并绘制时间
        Paint_Clear(WHITE);
        Paint_DrawTime(20, 10, &sPaint_time, &Font20, WHITE, BLACK);
        
        // 局部刷新：只更新时钟区域（位置80,200，大小200x50）
        EPD_4in26_Display_Part(BlackImage, 80, 200, 200, 50);
        
        DEV_Delay_ms(500);  // 模拟时钟间隔（实际应为1秒）
        
        // 检查是否达到刷新次数
        num = num - 1;
        if(num == 0) {
            break;
        }
    }
#endif

    /*========================================================================
                    第7步：4级灰度显示演示
    ========================================================================*/
#if 1
    // 释放之前的缓冲区
    free(BlackImage);
    
    printf("show Gray------------------------\r\n");
    
    // 重新计算灰度模式的缓冲区大小（每像素2位，即每4像素1字节）
    Imagesize = ((EPD_4in26_WIDTH % 4 == 0) ? 
                 (EPD_4in26_WIDTH / 4) : 
                 (EPD_4in26_WIDTH / 4 + 1)) * EPD_4in26_HEIGHT;
    
    // 分配灰度图像缓冲区
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        return -1;
    }
    
    // 初始化为4级灰度模式
    EPD_4in26_Init_4GRAY();
    printf("4 grayscale display\r\n");
    
    // 创建灰度图像（旋转90度）
    Paint_NewImage(BlackImage, EPD_4in26_WIDTH, EPD_4in26_HEIGHT, 90, WHITE);
    Paint_SetScale(4);   // 设置为4级灰度模式
    Paint_Clear(0xff);   // 清空为白色
    
    /* 使用灰度绘制各种图形 */
    // 绘制不同大小的灰色点
    Paint_DrawPoint(10, 80, GRAY4, DOT_PIXEL_1X1, DOT_STYLE_DFT);
    Paint_DrawPoint(10, 90, GRAY4, DOT_PIXEL_2X2, DOT_STYLE_DFT);
    Paint_DrawPoint(10, 100, GRAY4, DOT_PIXEL_3X3, DOT_STYLE_DFT);
    
    // 灰色线条
    Paint_DrawLine(20, 70, 70, 120, GRAY4, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(70, 70, 20, 120, GRAY4, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    
    // 灰色矩形
    Paint_DrawRectangle(20, 70, 70, 120, GRAY4, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(80, 70, 130, 120, GRAY4, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    
    // 灰色圆形（使用不同灰度级别）
    Paint_DrawCircle(45, 95, 20, GRAY4, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(105, 95, 20, GRAY2, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    
    // 灰色虚线
    Paint_DrawLine(85, 95, 125, 95, GRAY4, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    Paint_DrawLine(105, 75, 105, 115, GRAY4, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    
    /* 使用不同灰度组合显示文字 */
    Paint_DrawString_EN(10, 0, "waveshare", &Font16, GRAY4, GRAY1);
    Paint_DrawString_EN(10, 20, "hello world", &Font12, GRAY3, GRAY1);
    Paint_DrawNum(10, 33, 123456789, &Font12, GRAY4, GRAY2);
    Paint_DrawNum(10, 50, 987654321, &Font16, GRAY1, GRAY4);
    
    /* 中文灰度显示演示（展示4种灰度组合效果） */
    Paint_DrawString_CN(150, 0, "你好abc", &Font12CN, GRAY4, GRAY1);   // 黑字浅灰底
    Paint_DrawString_CN(150, 20, "你好abc", &Font12CN, GRAY3, GRAY2);  // 深灰字灰底
    Paint_DrawString_CN(150, 40, "你好abc", &Font12CN, GRAY2, GRAY3);  // 灰字深灰底
    Paint_DrawString_CN(150, 60, "你好abc", &Font12CN, GRAY1, GRAY4);  // 浅灰字黑底
    Paint_DrawString_CN(10, 130, "微雪电子", &Font24CN, GRAY1, GRAY4);
    
    // 显示灰度图像
    EPD_4in26_4GrayDisplay(BlackImage);
    DEV_Delay_ms(3000);
#endif

    /*========================================================================
                    第8步：清屏并进入睡眠模式
    ========================================================================*/
    // 重新初始化并清屏
    EPD_4in26_Init();
    EPD_4in26_Clear();
    
    // 进入深度睡眠模式（降低功耗）
    printf("Goto Sleep...\r\n");
    EPD_4in26_Sleep();
    
    // 释放图像缓冲区内存
    free(BlackImage);
    BlackImage = NULL;
    
    // 等待2秒确保睡眠模式生效
    DEV_Delay_ms(2000);
    
    // 关闭硬件模块（释放GPIO、SPI等资源）
    printf("close 5V, Module enters 0 power consumption ...\r\n");
    DEV_Module_Exit();
    
    return 0;  // 测试成功完成
}
