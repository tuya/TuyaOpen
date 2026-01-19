/*****************************************************************************
* | File      	:   DEV_Config.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :   电子墨水屏硬件底层接口实现
*                   本文件实现了与TuyaOS平台的硬件抽象层对接
*                   包括GPIO控制、SPI通信、延时等基础功能
*----------------
* |	This version:   V1.0
* | Date        :   2025-11-19
* | Info        :   适配TuyaOS平台的版本
* -----------------------------------------------------------------------------
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of theex Software, and to permit persons to  whom the Software is
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
#include "DEV_Config.h"

/*============================================================================
                            全局变量定义
============================================================================*/

/**
 * @brief GPIO输出模式配置结构体
 * @details 用于配置GPIO为推挽输出模式，默认输出低电平
 *          - mode: 推挽输出模式（TUYA_GPIO_PUSH_PULL）
 *          - direct: 输出方向
 *          - level: 初始电平为低
 */
TUYA_GPIO_BASE_CFG_T out_pin_cfg = {
    .mode = TUYA_GPIO_PUSH_PULL,    // 推挽输出模式，驱动能力强
    .direct = TUYA_GPIO_OUTPUT,      // 设置为输出方向
    .level = TUYA_GPIO_LEVEL_LOW     // 初始输出低电平
};

/**
 * @brief GPIO输入模式配置结构体
 * @details 用于配置GPIO为上拉输入模式，用于读取按键或状态信号
 *          - mode: 上拉输入模式（TUYA_GPIO_PULLUP）
 *          - direct: 输入方向
 */
TUYA_GPIO_BASE_CFG_T in_pin_cfg = {
    .mode = TUYA_GPIO_PULLUP,       // 上拉输入模式，空闲时为高电平
    .direct = TUYA_GPIO_INPUT,       // 设置为输入方向
};

/*============================================================================
                            GPIO读写函数
============================================================================*/

/**
 * @function: DEV_Digital_Write
 * @brief: GPIO数字输出函数
 * @details: 向指定GPIO引脚写入高/低电平
 *           这是墨水屏驱动最基础的GPIO控制函数
 * 
 * @param[in] Pin   要操作的GPIO引脚号（使用TUYA_GPIO_NUM_x宏）
 * @param[in] Value 输出电平值
 *                  - 0: 输出低电平
 *                  - 1: 输出高电平
 * @return: 无
 */
void DEV_Digital_Write(UWORD Pin, UBYTE Value)
{
    tkl_gpio_write(Pin, Value);
}

/**
 * @function: DEV_Digital_Read
 * @brief: GPIO数字输入读取函数
 * @details: 读取指定GPIO引脚的当前电平状态
 *           主要用于读取墨水屏的BUSY引脚状态
 * 
 * @param[in] Pin 要读取的GPIO引脚号
 * @return: UBYTE
 *          - 0: 引脚为低电平
 *          - 1: 引脚为高电平
 */
UBYTE DEV_Digital_Read(UWORD Pin)
{
    TUYA_GPIO_LEVEL_E read_level = 0;  // 用于存储读取到的电平值

    // 调用TuyaOS的GPIO读取函数
    tkl_gpio_read(Pin, &read_level);

    // 将TuyaOS的电平枚举值转换为0/1返回
    if(read_level == TUYA_GPIO_LEVEL_LOW)
        return 0;
    else
        return 1;
}

/*============================================================================
                            SPI通信函数
============================================================================*/

/**
 * @function: DEV_SPI_WriteByte
 * @brief: SPI发送单字节函数
 * @details: 通过硬件SPI接口发送单个字节数据到墨水屏
 *           使用TuyaOS的SPI驱动接口实现
 * 
 * @param[in] Value 要发送的字节数据
 * @return: 无
 * 
 * @note: 此函数使用硬件SPI，速度较快，适合批量数据传输
 */
void DEV_SPI_WriteByte(uint8_t Value)
{
    tkl_spi_send(SPI_ID, &Value, 1);  // 发送1个字节
}

/**
 * @function: DEV_SPI_Write_nByte
 * @brief: SPI批量发送函数
 * @details: 通过硬件SPI接口批量发送多个字节数据
 *           用于发送图像数据等大量数据时效率更高
 * 
 * @param[in] pData 要发送的数据缓冲区指针
 * @param[in] Len   要发送的数据长度（字节数）
 * @return: 无
 */
void DEV_SPI_Write_nByte(uint8_t *pData, uint32_t Len)
{
    tkl_spi_send(SPI_ID, pData, Len);
}

/*============================================================================
                            GPIO模式配置函数
============================================================================*/

/**
 * @function: DEV_GPIO_Mode
 * @brief: GPIO模式配置函数
 * @details: 设置指定GPIO引脚的输入/输出模式
 *           某些操作需要动态切换GPIO方向（如软件SPI的双向数据线）
 * 
 * @param[in] Pin  要配置的GPIO引脚号
 * @param[in] Mode 模式选择
 *                 - 0: 输入模式（带上拉）
 *                 - 1: 输出模式（推挽）
 * @return: 无
 */
void DEV_GPIO_Mode(UWORD Pin, UWORD Mode)
{
    if(Mode == 0) {
        // 配置为输入模式
		tkl_gpio_init(Pin, &in_pin_cfg);
	} else {
        // 配置为输出模式
		tkl_gpio_init(Pin, &out_pin_cfg);
	}
}

/*============================================================================
                            延时函数
============================================================================*/

/**
 * @function: DEV_Delay_ms
 * @brief: 毫秒级延时函数
 * @details: 让程序暂停执行指定的毫秒数
 *           墨水屏刷新过程中需要多次延时等待
 * 
 * @param[in] xms 延时时间（毫秒）
 * @return: 无
 * 
 * @note: 使用TuyaOS的系统睡眠函数实现，会让出CPU给其他任务
 */
void DEV_Delay_ms(UDOUBLE xms)
{
    tal_system_sleep(xms);
}

/*============================================================================
                            GPIO初始化函数
============================================================================*/

/**
 * @function: DEV_GPIO_Init
 * @brief: 墨水屏GPIO引脚初始化
 * @details: 初始化墨水屏所需的所有GPIO引脚
 *           包括：BUSY(输入)、RST、DC、CS、PWR(输出)
 * 
 * @return: 无
 * 
 * @note: 墨水屏引脚说明：
 *        - BUSY: 忙状态指示（输入），低电平表示忙
 *        - RST:  复位引脚（输出），低电平复位
 *        - DC:   数据/命令选择（输出），高=数据，低=命令
 *        - CS:   片选引脚（输出），低电平选中
 *        - PWR:  电源控制引脚（输出），高电平开启电源
 */
void DEV_GPIO_Init(void)
{
    // 初始化BUSY引脚为输入模式（用于读取墨水屏状态）
    DEV_GPIO_Mode(EPD_BUSY_PIN, 0);
    
    // 初始化RST复位引脚为输出模式
	DEV_GPIO_Mode(EPD_RST_PIN, 1);
    
    // 初始化DC数据/命令选择引脚为输出模式
	DEV_GPIO_Mode(EPD_DC_PIN, 1);
    
    // 初始化CS片选引脚为输出模式
	DEV_GPIO_Mode(EPD_CS_PIN, 1);
    
    // 初始化PWR电源控制引脚为输出模式
    DEV_GPIO_Mode(EPD_PWR_PIN, 1);
    
    // 以下两行被注释，因为使用硬件SPI时不需要手动控制MOSI和SCLK
    // DEV_GPIO_Mode(EPD_MOSI_PIN, 0);
	// DEV_GPIO_Mode(EPD_SCLK_PIN, 1);

    // 设置CS为高电平（不选中），准备通信时再拉低
	DEV_Digital_Write(EPD_CS_PIN, 1);
    
    // 开启墨水屏电源
    DEV_Digital_Write(EPD_PWR_PIN, 1);
}

/*============================================================================
                            软件SPI函数（备用）
============================================================================*/

/**
 * @function: DEV_SPI_SendnData
 * @brief: 软件SPI批量发送函数
 * @details: 通过软件模拟SPI时序发送多个字节
 *           当硬件SPI不可用时使用此函数
 * 
 * @param[in] Reg 要发送的数据数组指针
 * @return: 无
 * 
 * @warning: 此函数中sizeof(Reg)获取的是指针大小而非数组大小，
 *           实际使用时需要额外传入数据长度参数
 */
void DEV_SPI_SendnData(UBYTE *Reg)
{
    UDOUBLE size;
    size = sizeof(Reg);  // 注意：这里获取的是指针大小（4或8字节）
    for(UDOUBLE i=0 ; i<size ; i++)
    {
        DEV_SPI_SendData(Reg[i]);
    }
}

/**
 * @function: DEV_SPI_SendData
 * @brief: 软件SPI单字节发送函数
 * @details: 通过GPIO模拟SPI时序发送单个字节
 *           手动控制SCLK和MOSI引脚，实现SPI Mode 0时序
 *           
 *           SPI Mode 0时序特点：
 *           - CPOL=0: 空闲时时钟为低电平
 *           - CPHA=0: 在时钟上升沿采样数据
 *           - 数据高位在前（MSB First）
 * 
 * @param[in] Reg 要发送的字节数据
 * @return: 无
 */
void DEV_SPI_SendData(UBYTE Reg)
{
    UBYTE i, j = Reg;
    
    // 将MOSI引脚配置为输出模式
	DEV_GPIO_Mode(EPD_MOSI_PIN, 1);
    
    // 拉低CS片选，开始通信
	DEV_Digital_Write(EPD_CS_PIN, 0);
    
    // 循环发送8位数据（高位在前）
    for(i = 0; i < 8; i++)
    {
        // 拉低时钟，准备设置数据位
        DEV_Digital_Write(EPD_SCLK_PIN, 0);     
        
        // 根据当前最高位设置MOSI电平
        if (j & 0x80)  // 检查最高位是否为1
        {
            DEV_Digital_Write(EPD_MOSI_PIN, 1);  // 发送1
        }
        else
        {
            DEV_Digital_Write(EPD_MOSI_PIN, 0);  // 发送0
        }
        
        // 拉高时钟，在上升沿让从设备采样数据
        DEV_Digital_Write(EPD_SCLK_PIN, 1);
        
        // 数据左移一位，准备发送下一位
        j = j << 1;
    }
    
    // 通信结束，恢复时钟为低电平
	DEV_Digital_Write(EPD_SCLK_PIN, 0);
    
    // 拉高CS片选，结束通信
	DEV_Digital_Write(EPD_CS_PIN, 1);
}

/**
 * @function: DEV_SPI_ReadData
 * @brief: 软件SPI单字节读取函数
 * @details: 通过GPIO模拟SPI时序读取单个字节
 *           将MOSI引脚临时配置为输入模式用于接收数据
 *           
 *           读取时序：
 *           - 在时钟下降沿后读取MOSI引脚电平
 *           - 数据高位在前（MSB First）
 * 
 * @return: UBYTE 读取到的字节数据
 */
UBYTE DEV_SPI_ReadData()
{
    UBYTE i, j = 0xff;  // 初始化为全1
    
    // 将MOSI引脚配置为输入模式（用于接收数据）
	DEV_GPIO_Mode(EPD_MOSI_PIN, 0);
    
    // 拉低CS片选，开始通信
	DEV_Digital_Write(EPD_CS_PIN, 0);
    
    // 循环读取8位数据
    for(i = 0; i < 8; i++)
	{
        // 拉低时钟
		DEV_Digital_Write(EPD_SCLK_PIN, 0);
        
        // 数据左移一位，为新数据腾出最低位
		j = j << 1;
        
        // 读取MOSI引脚电平，设置数据最低位
		if (DEV_Digital_Read(EPD_MOSI_PIN))
		{
            j = j | 0x01;   // 读到高电平，最低位置1
		}
		else
		{
            j = j & 0xfe;   // 读到低电平，最低位置0
		}
        
        // 拉高时钟
		DEV_Digital_Write(EPD_SCLK_PIN, 1);
	}
    
    // 通信结束，恢复时钟为低电平
	DEV_Digital_Write(EPD_SCLK_PIN, 0);
    
    // 拉高CS片选，结束通信
	DEV_Digital_Write(EPD_CS_PIN, 1);
    
	return j;
}

/*============================================================================
                            模块初始化与退出函数
============================================================================*/

/**
 * @function: DEV_Module_Init
 * @brief: 墨水屏模块初始化函数
 * @details: 初始化墨水屏驱动所需的所有硬件资源
 *           包括SPI接口配置和GPIO引脚初始化
 *           
 *           SPI配置说明：
 *           - 模式: SPI Mode 0（CPOL=0, CPHA=0）
 *           - 频率: 4MHz
 *           - 数据位: 8位
 *           - 位序: MSB优先（高位在前）
 *           - 角色: 主机模式
 *           - 类型: 软件单线SPI
 * 
 * @return: UBYTE
 *          - 0: 初始化成功
 */
UBYTE DEV_Module_Init(void)
{
    printf("/***********************************/ \r\n");
    
    /* SPI接口初始化 */
    TUYA_SPI_BASE_CFG_T spi_cfg = {
        .mode = TUYA_SPI_MODE0,              // SPI模式0：CPOL=0, CPHA=0
        .freq_hz = SPI_FREQ,                 // SPI时钟频率：4MHz
        .databits = TUYA_SPI_DATA_BIT8,      // 数据位宽：8位
        .bitorder = TUYA_SPI_ORDER_MSB2LSB,  // 位序：高位在前
        .role = TUYA_SPI_ROLE_MASTER,        // 主机模式
        .type = TUYA_SPI_SOFT_ONE_WIRE_TYPE  // 软件单线SPI类型
    };
    
    // 调用TuyaOS SPI初始化接口
    tkl_spi_init(SPI_ID, &spi_cfg);

    // 初始化所有GPIO引脚
    DEV_GPIO_Init();
    
    printf("/***********************************/ \r\n");
    return 0;  // 返回0表示初始化成功
}

/**
 * @function: DEV_Module_Exit
 * @brief: 墨水屏模块退出函数
 * @details: 释放墨水屏驱动占用的所有硬件资源
 *           包括反初始化SPI接口和所有GPIO引脚
 *           在不再使用墨水屏时调用此函数释放资源
 * 
 * @return: 无
 */
void DEV_Module_Exit(void)
{
    // 反初始化SPI接口
    tkl_spi_deinit(SPI_ID);
    
    // 反初始化所有GPIO引脚，释放资源
    tkl_gpio_deinit(EPD_SCLK_PIN);   // SPI时钟引脚
    tkl_gpio_deinit(EPD_MOSI_PIN);   // SPI数据输出引脚
    tkl_gpio_deinit(EPD_CS_PIN);     // 片选引脚
    tkl_gpio_deinit(EPD_DC_PIN);     // 数据/命令选择引脚
    tkl_gpio_deinit(EPD_RST_PIN);    // 复位引脚
    tkl_gpio_deinit(EPD_BUSY_PIN);   // 忙状态引脚
    tkl_gpio_deinit(EPD_PWR_PIN);    // 电源控制引脚
}
