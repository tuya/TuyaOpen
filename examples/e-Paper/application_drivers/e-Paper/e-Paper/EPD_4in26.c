/*****************************************************************************
* | File      	:  	EPD_4in26.c
* | Author      :   Waveshare team
* | Function    :   4.26英寸电子墨水屏驱动实现
* | Info        :   
*                   本文件实现了Waveshare 4.26英寸电子墨水屏的完整驱动
*                   
*                   技术规格：
*                   - 分辨率: 800 x 480 像素
*                   - 显示颜色: 黑/白（支持4级灰度）
*                   - 接口: SPI
*                   - 工作电压: 3.3V
*                   - 刷新时间: 标准约4秒，快速约1.5秒
*                   
*                   支持功能：
*                   - 全屏刷新（标准/快速）
*                   - 局部刷新
*                   - 4级灰度显示
*                   - 深度睡眠模式
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
#include "EPD_4in26.h"
#include "Debug.h"

/*============================================================================
                            查找表数据（LUT）
============================================================================*/
/**
 * @brief 4级灰度模式查找表数据
 * @details LUT（Look-Up Table）定义了电子墨水屏的刷新波形
 *          波形控制电子墨水微胶囊中的带电粒子运动
 *          不同的LUT配置产生不同的显示效果和刷新速度
 *          
 *          数据结构（共112字节）：
 *          - 字节0-99:   VCOM和驱动电压波形数据
 *          - 字节100-104: 相位时间配置
 *          - 字节105:    VGH（栅极高电压）设置
 *          - 字节106-108: VSH1/VSH2/VSL（源极电压）设置
 *          - 字节109:    VCOM电压设置
 *          - 字节110-111: 保留
 */
const unsigned char LUT_DATA_4Gray[112] = {
    // VCOM波形数据（50字节）- 控制公共电极的电压波形
    0x80, 0x48, 0x4A, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x48, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x88, 0x48, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xA8, 0x48, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    
    // 相位时间配置（25字节）- 定义每个刷新阶段的持续时间
    0x07, 0x1E, 0x1C, 0x02, 0x00,  // 阶段1时间配置
    0x05, 0x01, 0x05, 0x01, 0x02,  // 阶段2时间配置
    0x08, 0x01, 0x01, 0x04, 0x04,  // 阶段3时间配置
    0x00, 0x02, 0x00, 0x02, 0x01,  // 阶段4时间配置
    0x00, 0x00, 0x00, 0x00, 0x00,  // 阶段5时间配置
    
    // 保留区域（20字节）
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01,
    
    // 灰度映射和电压配置（12字节）
    0x22, 0x22, 0x22, 0x22, 0x22,  // 灰度映射表
    0x17, 0x41, 0xA8, 0x32, 0x30,  // 电压参数
    0x00, 0x00,                     // 保留
};

/*============================================================================
                            内部函数实现
============================================================================*/

/**
 * @function: EPD_4in26_Reset
 * @brief: 硬件复位电子墨水屏
 * @details: 通过RST引脚对墨水屏进行硬件复位
 *           复位时序：高电平100ms -> 低电平2ms -> 高电平100ms
 *           复位后墨水屏恢复到初始状态，需要重新初始化
 * 
 * @return: 无
 */
static void EPD_4in26_Reset(void)
{
    DEV_Digital_Write(EPD_RST_PIN, 1);  // RST拉高
    DEV_Delay_ms(100);                   // 保持100ms
    DEV_Digital_Write(EPD_RST_PIN, 0);  // RST拉低，触发复位
    DEV_Delay_ms(2);                     // 保持2ms（复位有效时间）
    DEV_Digital_Write(EPD_RST_PIN, 1);  // RST拉高，结束复位
    DEV_Delay_ms(100);                   // 等待墨水屏稳定
}

/**
 * @function: EPD_4in26_SendCommand
 * @brief: 向墨水屏发送命令
 * @details: 发送单字节命令到墨水屏控制器
 *           发送命令时DC引脚需要为低电平
 *           
 *           时序说明：
 *           1. DC拉低（选择命令模式）
 *           2. CS拉低（选中芯片）
 *           3. 通过SPI发送命令字节
 *           4. CS拉高（释放芯片）
 * 
 * @param[in] Reg 命令寄存器地址/命令码
 * @return: 无
 */
static void EPD_4in26_SendCommand(UBYTE Reg)
{
    DEV_Digital_Write(EPD_DC_PIN, 0);   // DC=0 表示发送命令
    DEV_Digital_Write(EPD_CS_PIN, 0);   // CS=0 选中芯片
    DEV_SPI_WriteByte(Reg);              // SPI发送命令字节
    DEV_Digital_Write(EPD_CS_PIN, 1);   // CS=1 释放芯片
}

/**
 * @function: EPD_4in26_SendData
 * @brief: 向墨水屏发送单字节数据
 * @details: 发送单字节数据到墨水屏控制器
 *           发送数据时DC引脚需要为高电平
 * 
 * @param[in] Data 要发送的数据字节
 * @return: 无
 */
static void EPD_4in26_SendData(UBYTE Data)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);   // DC=1 表示发送数据
    DEV_Digital_Write(EPD_CS_PIN, 0);   // CS=0 选中芯片
    DEV_SPI_WriteByte(Data);             // SPI发送数据字节
    DEV_Digital_Write(EPD_CS_PIN, 1);   // CS=1 释放芯片
}

/**
 * @function: EPD_4in26_SendData2
 * @brief: 向墨水屏批量发送数据
 * @details: 批量发送多字节数据，用于传输图像数据时提高效率
 * 
 * @param[in] pData 数据缓冲区指针
 * @param[in] len   数据长度（字节数）
 * @return: 无
 */
static void EPD_4in26_SendData2(UBYTE *pData, UDOUBLE len)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);   // DC=1 表示发送数据
    DEV_Digital_Write(EPD_CS_PIN, 0);   // CS=0 选中芯片
    DEV_SPI_Write_nByte(pData, len);     // SPI批量发送数据
    DEV_Digital_Write(EPD_CS_PIN, 1);   // CS=1 释放芯片
}

/**
 * @function: EPD_4in26_ReadBusy
 * @brief: 等待墨水屏空闲
 * @details: 轮询BUSY引脚状态，等待墨水屏完成当前操作
 *           BUSY=0（低电平）表示墨水屏正忙
 *           BUSY=1（高电平）表示墨水屏空闲
 *           
 *           电子墨水屏刷新需要较长时间（数秒），
 *           在此期间不能发送新的命令或数据
 * 
 * @return: 无
 * 
 * @note: 此函数会阻塞，直到墨水屏空闲
 */
void EPD_4in26_ReadBusy(void)
{
    Debug("e-Paper busy\r\n");
    
    // 轮询等待BUSY引脚变为低电平（墨水屏空闲）
    while(1)
    {
        // BUSY=0 表示墨水屏空闲，可以退出等待
        if(DEV_Digital_Read(EPD_BUSY_PIN) == 0) 
            break;
        DEV_Delay_ms(20);  // 每20ms检查一次，避免CPU占用过高
    }
    
    DEV_Delay_ms(20);  // 额外等待20ms确保稳定
    Debug("e-Paper busy release\r\n");
}

/*============================================================================
                            显示刷新控制函数
============================================================================*/

/**
 * @function: EPD_4in26_TurnOnDisplay
 * @brief: 触发标准显示刷新
 * @details: 启动墨水屏的显示更新序列
 *           使用标准刷新波形，显示质量最好，刷新时间约4秒
 *           
 *           命令说明：
 *           - 0x22: 显示更新控制命令
 *           - 0xF7: 启用全部刷新序列
 *           - 0x20: 激活显示更新序列
 * 
 * @return: 无
 */
static void EPD_4in26_TurnOnDisplay(void)
{
    EPD_4in26_SendCommand(0x22);  // 显示更新控制命令
    EPD_4in26_SendData(0xF7);     // 0xF7: 使用完整刷新序列
    EPD_4in26_SendCommand(0x20);  // 激活显示更新序列
    EPD_4in26_ReadBusy();          // 等待刷新完成
}

/**
 * @function: EPD_4in26_TurnOnDisplay_Fast
 * @brief: 触发快速显示刷新
 * @details: 启动墨水屏的快速刷新序列
 *           刷新时间约1.5秒，但可能有轻微残影
 *           
 *           命令说明：
 *           - 0xC7: 使用简化的刷新序列
 * 
 * @return: 无
 */
static void EPD_4in26_TurnOnDisplay_Fast(void)
{
    EPD_4in26_SendCommand(0x22);  // 显示更新控制命令
    EPD_4in26_SendData(0xC7);     // 0xC7: 使用快速刷新序列
    EPD_4in26_SendCommand(0x20);  // 激活显示更新序列
    EPD_4in26_ReadBusy();          // 等待刷新完成
}

/**
 * @function: EPD_4in26_TurnOnDisplay_Part
 * @brief: 触发局部显示刷新
 * @details: 启动墨水屏的局部刷新序列
 *           只刷新指定区域，速度更快
 *           
 *           命令说明：
 *           - 0xFF: 使用局部刷新序列
 * 
 * @return: 无
 */
static void EPD_4in26_TurnOnDisplay_Part(void)
{
    EPD_4in26_SendCommand(0x22);  // 显示更新控制命令
    EPD_4in26_SendData(0xFF);     // 0xFF: 使用局部刷新序列
    EPD_4in26_SendCommand(0x20);  // 激活显示更新序列
    EPD_4in26_ReadBusy();          // 等待刷新完成
}

/**
 * @function: EPD_4in26_TurnOnDisplay_4GRAY
 * @brief: 触发4级灰度显示刷新
 * @details: 启动墨水屏的4级灰度刷新序列
 *           使用自定义LUT实现灰度显示
 * 
 * @return: 无
 */
static void EPD_4in26_TurnOnDisplay_4GRAY(void)
{
    EPD_4in26_SendCommand(0x22);  // 显示更新控制命令
    EPD_4in26_SendData(0xC7);     // 使用灰度刷新序列
    EPD_4in26_SendCommand(0x20);  // 激活显示更新序列
    EPD_4in26_ReadBusy();          // 等待刷新完成
}

/*============================================================================
                            查找表配置函数
============================================================================*/

/**
 * @function: EPD_4in26_Lut
 * @brief: 加载4级灰度查找表
 * @details: 将LUT数据加载到墨水屏控制器
 *           LUT定义了灰度显示的刷新波形
 *           
 *           寄存器说明：
 *           - 0x32: VCOM/数据电压LUT寄存器
 *           - 0x03: VGH电压设置
 *           - 0x04: VSH1/VSH2/VSL电压设置
 *           - 0x2C: VCOM电压设置
 * 
 * @return: 无
 */
static void EPD_4in26_Lut(void)
{
    unsigned int count;
    
    // 加载波形数据到LUT寄存器（105字节）
    EPD_4in26_SendCommand(0x32);  // VCOM和数据电压LUT
    for(count = 0; count < 105; count++) {
        EPD_4in26_SendData(LUT_DATA_4Gray[count]);
    }

    // 设置VGH（栅极高电压）
    EPD_4in26_SendCommand(0x03);
    EPD_4in26_SendData(LUT_DATA_4Gray[105]);

    // 设置VSH1、VSH2、VSL（源极电压）
    EPD_4in26_SendCommand(0x04);
    EPD_4in26_SendData(LUT_DATA_4Gray[106]);  // VSH1
    EPD_4in26_SendData(LUT_DATA_4Gray[107]);  // VSH2
    EPD_4in26_SendData(LUT_DATA_4Gray[108]);  // VSL

    // 设置VCOM电压
    EPD_4in26_SendCommand(0x2C);
    EPD_4in26_SendData(LUT_DATA_4Gray[109]);
}

/*============================================================================
                            显示窗口和光标设置函数
============================================================================*/

/**
 * @function: EPD_4in26_SetWindows
 * @brief: 设置显示窗口范围
 * @details: 定义RAM数据写入的X和Y地址范围
 *           后续写入的图像数据将填充到此窗口区域
 *           
 *           寄存器说明：
 *           - 0x44: 设置X方向地址起止范围
 *           - 0x45: 设置Y方向地址起止范围
 * 
 * @param[in] Xstart 窗口起始X坐标
 * @param[in] Ystart 窗口起始Y坐标
 * @param[in] Xend   窗口结束X坐标
 * @param[in] Yend   窗口结束Y坐标
 * @return: 无
 */
static void EPD_4in26_SetWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend)
{
    // 设置X方向地址范围（10位地址，需要2字节）
    EPD_4in26_SendCommand(0x44);
    EPD_4in26_SendData(Xstart & 0xFF);         // X起始地址低8位
    EPD_4in26_SendData((Xstart >> 8) & 0x03);  // X起始地址高2位
    EPD_4in26_SendData(Xend & 0xFF);           // X结束地址低8位
    EPD_4in26_SendData((Xend >> 8) & 0x03);    // X结束地址高2位
    
    // 设置Y方向地址范围（10位地址，需要2字节）
    EPD_4in26_SendCommand(0x45);
    EPD_4in26_SendData(Ystart & 0xFF);         // Y起始地址低8位
    EPD_4in26_SendData((Ystart >> 8) & 0x03);  // Y起始地址高2位
    EPD_4in26_SendData(Yend & 0xFF);           // Y结束地址低8位
    EPD_4in26_SendData((Yend >> 8) & 0x03);    // Y结束地址高2位
}

/**
 * @function: EPD_4in26_SetCursor
 * @brief: 设置RAM写入光标位置
 * @details: 设置下一次数据写入的起始地址
 *           
 *           寄存器说明：
 *           - 0x4E: 设置X方向地址计数器
 *           - 0x4F: 设置Y方向地址计数器
 * 
 * @param[in] Xstart 光标X坐标
 * @param[in] Ystart 光标Y坐标
 * @return: 无
 */
static void EPD_4in26_SetCursor(UWORD Xstart, UWORD Ystart)
{
    // 设置X方向地址计数器
    EPD_4in26_SendCommand(0x4E);
    EPD_4in26_SendData(Xstart & 0xFF);
    EPD_4in26_SendData((Xstart >> 8) & 0x03);

    // 设置Y方向地址计数器
    EPD_4in26_SendCommand(0x4F);
    EPD_4in26_SendData(Ystart & 0xFF);
    EPD_4in26_SendData((Ystart >> 8) & 0x03);
}

/*============================================================================
                            公开API函数实现
============================================================================*/

/**
 * @function: EPD_4in26_Init
 * @brief: 标准模式初始化
 * @details: 初始化墨水屏为标准全刷新模式
 *           
 *           初始化流程：
 *           1. 硬件复位
 *           2. 软件复位（命令0x12）
 *           3. 配置内部温度传感器
 *           4. 配置软启动参数
 *           5. 配置驱动输出控制
 *           6. 配置边框设置
 *           7. 配置数据入口模式
 *           8. 设置显示窗口和光标
 * 
 * @return: 无
 */
void EPD_4in26_Init(void)
{
    // 硬件复位
    EPD_4in26_Reset();
    DEV_Delay_ms(100);

    // 等待墨水屏就绪
    EPD_4in26_ReadBusy();
    
    // 软件复位
    EPD_4in26_SendCommand(0x12);  // SWRESET命令
    EPD_4in26_ReadBusy();
    
    // 使用内部温度传感器（自动温度补偿）
    EPD_4in26_SendCommand(0x18);
    EPD_4in26_SendData(0x80);     // 0x80: 启用内部温度传感器

    // 设置软启动参数（优化启动波形）
    EPD_4in26_SendCommand(0x0C);
    EPD_4in26_SendData(0xAE);
    EPD_4in26_SendData(0xC7);
    EPD_4in26_SendData(0xC3);
    EPD_4in26_SendData(0xC0);
    EPD_4in26_SendData(0x80);

    // 设置驱动输出控制（配置显示分辨率）
    EPD_4in26_SendCommand(0x01);
    EPD_4in26_SendData((EPD_4in26_HEIGHT - 1) % 256);  // Y分辨率低8位
    EPD_4in26_SendData((EPD_4in26_HEIGHT - 1) / 256);  // Y分辨率高8位
    EPD_4in26_SendData(0x02);                          // 扫描方向

    // 设置边框显示
    EPD_4in26_SendCommand(0x3C);
    EPD_4in26_SendData(0x01);     // 边框设置

    // 设置数据入口模式
    EPD_4in26_SendCommand(0x11);
    EPD_4in26_SendData(0x01);     // X模式: X递增, Y递减

    // 设置显示窗口（全屏）
    EPD_4in26_SetWindows(0, EPD_4in26_HEIGHT - 1, EPD_4in26_WIDTH - 1, 0);

    // 设置光标位置
    EPD_4in26_SetCursor(0, 0);

    // 等待配置完成
    EPD_4in26_ReadBusy();
}

/**
 * @function: EPD_4in26_Init_Fast
 * @brief: 快速模式初始化
 * @details: 初始化墨水屏为快速刷新模式
 *           在标准初始化基础上增加温度预设配置
 *           可将刷新时间缩短至约1.5秒
 * 
 * @return: 无
 */
void EPD_4in26_Init_Fast(void)
{
    // 硬件复位
    EPD_4in26_Reset();
    DEV_Delay_ms(100);

    EPD_4in26_ReadBusy();
    EPD_4in26_SendCommand(0x12);  // 软件复位
    EPD_4in26_ReadBusy();
    
    // 使用内部温度传感器
    EPD_4in26_SendCommand(0x18);
    EPD_4in26_SendData(0x80);

    // 设置软启动参数
    EPD_4in26_SendCommand(0x0C);
    EPD_4in26_SendData(0xAE);
    EPD_4in26_SendData(0xC7);
    EPD_4in26_SendData(0xC3);
    EPD_4in26_SendData(0xC0);
    EPD_4in26_SendData(0x80);

    // 设置驱动输出控制
    EPD_4in26_SendCommand(0x01);
    EPD_4in26_SendData((EPD_4in26_HEIGHT - 1) % 256);
    EPD_4in26_SendData((EPD_4in26_HEIGHT - 1) / 256);
    EPD_4in26_SendData(0x02);

    // 设置边框
    EPD_4in26_SendCommand(0x3C);
    EPD_4in26_SendData(0x01);

    // 设置数据入口模式
    EPD_4in26_SendCommand(0x11);
    EPD_4in26_SendData(0x01);

    // 设置显示窗口
    EPD_4in26_SetWindows(0, EPD_4in26_HEIGHT - 1, EPD_4in26_WIDTH - 1, 0);

    // 设置光标位置
    EPD_4in26_SetCursor(0, 0);

    EPD_4in26_ReadBusy();

    // 快速模式特殊配置：设置温度寄存器以加速刷新
    EPD_4in26_SendCommand(0x1A);   // 写入温度寄存器
    EPD_4in26_SendData(0x5A);      // 预设温度值

    // 触发温度加载序列
    EPD_4in26_SendCommand(0x22);
    EPD_4in26_SendData(0x91);      // 加载温度值
    EPD_4in26_SendCommand(0x20);   // 激活序列
    
    EPD_4in26_ReadBusy();
}

/**
 * @function: EPD_4in26_Init_4GRAY
 * @brief: 4级灰度模式初始化
 * @details: 初始化墨水屏为4级灰度显示模式
 *           加载自定义LUT实现灰度效果
 * 
 * @return: 无
 */
void EPD_4in26_Init_4GRAY(void)
{
    // 硬件复位
    EPD_4in26_Reset();
    DEV_Delay_ms(100);

    EPD_4in26_ReadBusy();
    EPD_4in26_SendCommand(0x12);  // 软件复位
    EPD_4in26_ReadBusy();
    
    // 使用内部温度传感器
    EPD_4in26_SendCommand(0x18);
    EPD_4in26_SendData(0x80);

    // 设置软启动参数
    EPD_4in26_SendCommand(0x0C);
    EPD_4in26_SendData(0xAE);
    EPD_4in26_SendData(0xC7);
    EPD_4in26_SendData(0xC3);
    EPD_4in26_SendData(0xC0);
    EPD_4in26_SendData(0x80);

    // 设置驱动输出控制（灰度模式使用WIDTH作为Y分辨率）
    EPD_4in26_SendCommand(0x01);
    EPD_4in26_SendData((EPD_4in26_WIDTH - 1) % 256);
    EPD_4in26_SendData((EPD_4in26_WIDTH - 1) / 256);
    EPD_4in26_SendData(0x02);

    // 设置边框
    EPD_4in26_SendCommand(0x3C);
    EPD_4in26_SendData(0x01);

    // 设置数据入口模式
    EPD_4in26_SendCommand(0x11);
    EPD_4in26_SendData(0x01);

    // 设置显示窗口
    EPD_4in26_SetWindows(0, EPD_4in26_HEIGHT - 1, EPD_4in26_WIDTH - 1, 0);

    // 设置光标位置
    EPD_4in26_SetCursor(0, 0);

    EPD_4in26_ReadBusy();

    // 加载4级灰度LUT
    EPD_4in26_Lut();
}

/**
 * @function: EPD_4in26_Clear
 * @brief: 清屏（显示全白）
 * @details: 将整个屏幕刷新为白色
 *           同时更新新旧两个显示RAM
 *           
 *           图像数据说明：
 *           - 0xFF: 8个白色像素
 *           - 0x00: 8个黑色像素
 *           
 *           RAM说明：
 *           - 0x24: 新数据RAM（Black/White RAM）
 *           - 0x26: 旧数据RAM（用于局部刷新对比）
 * 
 * @return: 无
 */
void EPD_4in26_Clear(void)
{
    UWORD i;
    UWORD height = EPD_4in26_HEIGHT;
    UWORD width = EPD_4in26_WIDTH / 8;  // 每字节8像素
    
    // 创建一行全白数据
    UBYTE image[EPD_4in26_WIDTH / 8] = {0x00};
    for(i = 0; i < width; i++) {
        image[i] = 0xff;  // 全白
    }
    
    // 写入新数据RAM
    EPD_4in26_SendCommand(0x24);
    for(i = 0; i < height; i++)
    {
        EPD_4in26_SendData2(image, width);
    }

    // 写入旧数据RAM（用于后续局部刷新）
    EPD_4in26_SendCommand(0x26);
    for(i = 0; i < height; i++)
    {
        EPD_4in26_SendData2(image, width);
    }
    
    // 触发显示刷新
    EPD_4in26_TurnOnDisplay();
}

/**
 * @function: EPD_4in26_Display
 * @brief: 标准显示图像
 * @details: 将图像缓冲区数据写入墨水屏RAM并刷新显示
 * 
 * @param[in] Image 图像缓冲区指针
 *                  大小: 800*480/8 = 48000字节
 *                  格式: 每位1像素，1=白色，0=黑色
 * @return: 无
 */
void EPD_4in26_Display(UBYTE *Image)
{
    UWORD i;
    UWORD height = EPD_4in26_HEIGHT;
    UWORD width = EPD_4in26_WIDTH / 8;
    
    // 写入图像数据到新数据RAM
    EPD_4in26_SendCommand(0x24);
    for(i = 0; i < height; i++)
    {
        EPD_4in26_SendData2((UBYTE *)(Image + i * width), width);
    }
    
    // 触发显示刷新
    EPD_4in26_TurnOnDisplay();
}

/**
 * @function: EPD_4in26_Display_Base
 * @brief: 基础显示（更新新旧RAM）
 * @details: 同时将图像写入新旧两个RAM
 *           用于局部刷新前的初始画面设置
 * 
 * @param[in] Image 图像缓冲区指针
 * @return: 无
 */
void EPD_4in26_Display_Base(UBYTE *Image)
{
    UWORD i;
    UWORD height = EPD_4in26_HEIGHT;
    UWORD width = EPD_4in26_WIDTH / 8;
    
    // 写入新数据RAM
    EPD_4in26_SendCommand(0x24);
    for(i = 0; i < height; i++)
    {
        EPD_4in26_SendData2((UBYTE *)(Image + i * width), width);
    }

    // 写入旧数据RAM
    EPD_4in26_SendCommand(0x26);
    for(i = 0; i < height; i++)
    {
        EPD_4in26_SendData2((UBYTE *)(Image + i * width), width);
    }
    
    // 触发显示刷新
    EPD_4in26_TurnOnDisplay();
}

/**
 * @function: EPD_4in26_Display_Fast
 * @brief: 快速显示图像
 * @details: 使用快速刷新模式显示图像
 *           需要先调用EPD_4in26_Init_Fast()初始化
 * 
 * @param[in] Image 图像缓冲区指针
 * @return: 无
 */
void EPD_4in26_Display_Fast(UBYTE *Image)
{
    UWORD i;
    UWORD height = EPD_4in26_HEIGHT;
    UWORD width = EPD_4in26_WIDTH / 8;
    
    // 写入图像数据
    EPD_4in26_SendCommand(0x24);
    for(i = 0; i < height; i++)
    {
        EPD_4in26_SendData2((UBYTE *)(Image + i * width), width);
    }
    
    // 使用快速刷新
    EPD_4in26_TurnOnDisplay_Fast();
}

/**
 * @function: EPD_4in26_Display_Part
 * @brief: 局部显示
 * @details: 只刷新屏幕的指定矩形区域
 *           适用于小范围内容更新，如时钟显示等
 * 
 * @param[in] Image 局部图像缓冲区指针
 * @param[in] x     左上角X坐标
 * @param[in] y     左上角Y坐标  
 * @param[in] w     区域宽度
 * @param[in] l     区域高度
 * @return: 无
 * 
 * @note: 连续多次局部刷新后建议进行全刷以消除残影
 */
void EPD_4in26_Display_Part(UBYTE *Image, UWORD x, UWORD y, UWORD w, UWORD l)
{
    UWORD i;
    UWORD height = l;
    UWORD width = (w % 8 == 0) ? (w / 8) : (w / 8 + 1);  // 向上取整到字节边界

    // 重新配置墨水屏
    EPD_4in26_Reset();

    // 使用内部温度传感器
    EPD_4in26_SendCommand(0x18);
    EPD_4in26_SendData(0x80);

    // 设置边框（局部刷新使用不同设置）
    EPD_4in26_SendCommand(0x3C);
    EPD_4in26_SendData(0x80);

    // 设置局部显示窗口
    EPD_4in26_SetWindows(x, y, x + w - 1, y + l - 1);

    // 设置光标到窗口起始位置
    EPD_4in26_SetCursor(x, y);

    // 写入局部图像数据
    EPD_4in26_SendCommand(0x24);
    for(i = 0; i < height; i++)
    {
        EPD_4in26_SendData2((UBYTE *)(Image + i * width), width);
    }
    
    // 触发局部刷新
    EPD_4in26_TurnOnDisplay_Part();
}

/**
 * @function: EPD_4in26_Display_Part_NoReset
 * @brief: 局部刷新（无Reset版本）
 * @details: 用于连续刷新多个区域，避免每次都reset EPD
 *           第一个区域使用 EPD_4in26_Display_Part（带reset）
 *           后续区域使用此函数（不reset）
 */
void EPD_4in26_Display_Part_NoReset(UBYTE *Image, UWORD x, UWORD y, UWORD w, UWORD l)
{
    UWORD i;
    UWORD height = l;
    UWORD width = (w % 8 == 0) ? (w / 8) : (w / 8 + 1);

    // 不调用 EPD_4in26_Reset()，直接设置窗口

    // 设置局部显示窗口
    EPD_4in26_SetWindows(x, y, x + w - 1, y + l - 1);

    // 设置光标到窗口起始位置
    EPD_4in26_SetCursor(x, y);

    // 写入局部图像数据
    EPD_4in26_SendCommand(0x24);
    for(i = 0; i < height; i++)
    {
        EPD_4in26_SendData2((UBYTE *)(Image + i * width), width);
    }
    
    // 触发局部刷新
    EPD_4in26_TurnOnDisplay_Part();
}

/**
 * @function: EPD_4in26_4GrayDisplay
 * @brief: 4级灰度显示
 * @details: 显示4级灰度图像
 *           将2位灰度数据转换为墨水屏所需的格式
 *           
 *           灰度映射：
 *           - 0xC0: 白色 (11)
 *           - 0x80: 浅灰 (10)
 *           - 0x40: 深灰 (01)
 *           - 0x00: 黑色 (00)
 *           
 *           需要同时写入两个RAM来实现灰度效果
 * 
 * @param[in] Image 灰度图像缓冲区指针
 *                  大小: 800*480/4 = 96000字节
 *                  格式: 每2位1像素
 * @return: 无
 */
void EPD_4in26_4GrayDisplay(UBYTE *Image)
{
    UDOUBLE i, j, k;
    UBYTE temp1, temp2, temp3;

    // 写入旧数据RAM（第一个RAM平面）
    EPD_4in26_SendCommand(0x24);
    for(i = 0; i < 48000; i++) {
        temp3 = 0;
        for(j = 0; j < 2; j++) {
            temp1 = Image[i * 2 + j];
            for(k = 0; k < 2; k++) {
                temp2 = temp1 & 0xC0;
                // 根据灰度值设置对应位
                if(temp2 == 0xC0)       // 白色
                    temp3 |= 0x00;
                else if(temp2 == 0x00)  // 黑色
                    temp3 |= 0x01;
                else if(temp2 == 0x80)  // 浅灰
                    temp3 |= 0x01;
                else                     // 深灰 (0x40)
                    temp3 |= 0x00;
                temp3 <<= 1;

                temp1 <<= 2;
                temp2 = temp1 & 0xC0;
                if(temp2 == 0xC0)
                    temp3 |= 0x00;
                else if(temp2 == 0x00)
                    temp3 |= 0x01;
                else if(temp2 == 0x80)
                    temp3 |= 0x01;
                else
                    temp3 |= 0x00;
                if(j != 1 || k != 1)
                    temp3 <<= 1;

                temp1 <<= 2;
            }
        }
        EPD_4in26_SendData(temp3);
    }

    // 写入新数据RAM（第二个RAM平面）
    EPD_4in26_SendCommand(0x26);
    for(i = 0; i < 48000; i++) {
        temp3 = 0;
        for(j = 0; j < 2; j++) {
            temp1 = Image[i * 2 + j];
            for(k = 0; k < 2; k++) {
                temp2 = temp1 & 0xC0;
                // 第二个RAM平面的灰度映射
                if(temp2 == 0xC0)       // 白色
                    temp3 |= 0x00;
                else if(temp2 == 0x00)  // 黑色
                    temp3 |= 0x01;
                else if(temp2 == 0x80)  // 浅灰
                    temp3 |= 0x00;
                else                     // 深灰 (0x40)
                    temp3 |= 0x01;
                temp3 <<= 1;

                temp1 <<= 2;
                temp2 = temp1 & 0xC0;
                if(temp2 == 0xC0)
                    temp3 |= 0x00;
                else if(temp2 == 0x00)
                    temp3 |= 0x01;
                else if(temp2 == 0x80)
                    temp3 |= 0x00;
                else
                    temp3 |= 0x01;
                if(j != 1 || k != 1)
                    temp3 <<= 1;

                temp1 <<= 2;
            }
        }
        EPD_4in26_SendData(temp3);
    }

    // 触发灰度显示刷新
    EPD_4in26_TurnOnDisplay_4GRAY();
}

/**
 * @function: EPD_4in26_Sleep
 * @brief: 进入深度睡眠模式
 * @details: 让墨水屏进入低功耗睡眠状态
 *           睡眠模式下功耗降至微安级别
 *           屏幕保持最后显示的内容
 *           
 *           唤醒方法：
 *           - 调用EPD_4in26_Init()或其他初始化函数
 *           - 或者通过硬件复位（RST引脚）
 *           
 *           命令说明：
 *           - 0x10: 深度睡眠命令
 *           - 0x03: 睡眠检查码
 * 
 * @return: 无
 */
void EPD_4in26_Sleep(void)
{
    EPD_4in26_SendCommand(0x10);  // 进入深度睡眠命令
    EPD_4in26_SendData(0x03);     // 睡眠检查码
    DEV_Delay_ms(100);             // 等待进入睡眠状态
}
