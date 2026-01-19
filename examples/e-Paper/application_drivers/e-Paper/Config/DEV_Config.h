/*****************************************************************************
* | File      	:   DEV_Config.h
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :   硬件底层接口头文件
*                   用于屏蔽不同硬件平台的底层差异，提高代码可移植性
*                   本文件定义了墨水屏驱动所需的：
*                   - 数据类型别名
*                   - GPIO引脚配置
*                   - SPI通信参数
*                   - 底层驱动函数声明
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
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

/*============================================================================
                            标准库头文件
============================================================================*/
#include <stdint.h>     // 标准整数类型定义（uint8_t, uint16_t等）
#include <stdio.h>      // 标准输入输出（printf等）
#include <string.h>     // 字符串操作函数（memset, memcpy等）
#include "Debug.h"      // 调试日志输出宏定义

/*============================================================================
                            TuyaOS平台头文件
============================================================================*/
#include "tkl_gpio.h"       // TuyaOS GPIO操作接口
#include "tkl_spi.h"        // TuyaOS SPI通信接口
#include "tal_system.h"     // TuyaOS 系统函数（如tal_system_sleep延时）
#include "tal_log.h"        // TuyaOS 日志输出接口

/*============================================================================
                            数据类型定义
============================================================================*/
/**
 * @brief 数据类型别名定义
 * @details 为了代码可读性和跨平台兼容性，定义以下类型别名：
 *          - UBYTE:   无符号8位整数，用于存储单字节数据
 *          - UWORD:   无符号16位整数，用于存储GPIO引脚号等
 *          - UDOUBLE: 无符号32位整数，用于存储延时时间、数据长度等
 */
#define UBYTE   uint8_t     // 无符号单字节（0-255）
#define UWORD   uint16_t    // 无符号双字节（0-65535）
#define UDOUBLE uint32_t    // 无符号四字节（0-4294967295）

/*============================================================================
                            GPIO引脚配置
============================================================================*/
/**
 * @brief 墨水屏GPIO引脚定义
 * @details 定义墨水屏驱动所需的GPIO引脚号
 *          使用 #ifndef 允许在编译时通过宏定义覆盖默认值
 * 
 * 墨水屏引脚功能说明：
 * ┌──────────────┬────────────┬─────────────────────────────────┐
 * │   引脚名称   │  默认GPIO  │             功能说明            │
 * ├──────────────┼────────────┼─────────────────────────────────┤
 * │ EPD_SCLK_PIN │   GPIO2    │ SPI时钟信号（Serial Clock）     │
 * │ EPD_MOSI_PIN │   GPIO4    │ SPI主出从入（Master Out）       │
 * │ EPD_CS_PIN   │   GPIO3    │ SPI片选信号（Chip Select）      │
 * │ EPD_DC_PIN   │   GPIO7    │ 数据/命令选择（Data/Command）   │
 * │ EPD_RST_PIN  │   GPIO8    │ 硬件复位信号（Reset）           │
 * │ EPD_BUSY_PIN │   GPIO6    │ 忙状态指示（Busy Status）       │
 * │ EPD_PWR_PIN  │   GPIO28   │ 电源控制（Power Control）       │
 * └──────────────┴────────────┴─────────────────────────────────┘
 */

/* SPI时钟引脚 - 用于同步SPI数据传输 */
#ifndef EPD_SCLK_PIN
#define EPD_SCLK_PIN    TUYA_GPIO_NUM_2
#endif

/* SPI数据输出引脚 - 主机向墨水屏发送数据 */
#ifndef EPD_MOSI_PIN
#define EPD_MOSI_PIN    TUYA_GPIO_NUM_4
#endif

/* SPI片选引脚 - 低电平有效，选中墨水屏进行通信 */
#ifndef EPD_CS_PIN      
#define EPD_CS_PIN      TUYA_GPIO_NUM_3
#endif

/* 数据/命令选择引脚 - 高电平发送数据，低电平发送命令 */
/* 注意：GPIO7 与 KEY_PIN 冲突，已禁用按键功能 */
#ifndef EPD_DC_PIN
#define EPD_DC_PIN      TUYA_GPIO_NUM_7
#endif

/* 硬件复位引脚 - 低电平复位墨水屏 */
#ifndef EPD_RST_PIN
#define EPD_RST_PIN     TUYA_GPIO_NUM_8
#endif

/* 忙状态引脚 - 墨水屏刷新时输出低电平，空闲时输出高电平 */
#ifndef EPD_BUSY_PIN
#define EPD_BUSY_PIN    TUYA_GPIO_NUM_6
#endif

/* 电源控制引脚 - 控制墨水屏电源，高电平开启 */
#ifndef EPD_PWR_PIN
#define EPD_PWR_PIN     TUYA_GPIO_NUM_28
#endif

/*============================================================================
                            SPI通信配置
============================================================================*/
/**
 * @brief SPI通信参数定义
 * @details 配置与墨水屏通信的SPI接口参数
 *          
 *          SPI_ID:   使用的SPI外设编号（TuyaOS平台的SPI1）
 *          SPI_FREQ: SPI时钟频率，设置为4MHz
 *                    墨水屏通常支持的最高频率约为10MHz
 *                    4MHz是较为稳定的配置值
 */
#define SPI_ID          TUYA_SPI_NUM_1          // 使用SPI1外设
#define SPI_FREQ        4 * 1000 * 1000         // SPI时钟频率: 4MHz

/*============================================================================
                            函数声明
============================================================================*/

/*------------------------------ GPIO操作函数 ------------------------------*/

/**
 * @brief GPIO数字输出
 * @param Pin   GPIO引脚号
 * @param Value 输出电平（0=低电平，1=高电平）
 */
void DEV_Digital_Write(UWORD Pin, UBYTE Value);

/**
 * @brief GPIO数字读取
 * @param Pin GPIO引脚号
 * @return 引脚电平（0=低电平，1=高电平）
 */
UBYTE DEV_Digital_Read(UWORD Pin);

/*------------------------------ SPI通信函数 ------------------------------*/

/**
 * @brief SPI发送单字节（硬件SPI）
 * @param Value 要发送的字节
 */
void DEV_SPI_WriteByte(UBYTE Value);

/**
 * @brief SPI批量发送（硬件SPI）
 * @param pData 数据缓冲区指针
 * @param Len   数据长度
 */
void DEV_SPI_Write_nByte(uint8_t *pData, uint32_t Len);

/**
 * @brief 软件SPI发送单字节
 * @param Reg 要发送的字节
 */
void DEV_SPI_SendData(UBYTE Reg);

/**
 * @brief 软件SPI批量发送
 * @param Reg 数据数组指针
 */
void DEV_SPI_SendnData(UBYTE *Reg);

/**
 * @brief 软件SPI读取单字节
 * @return 读取到的字节
 */
UBYTE DEV_SPI_ReadData(void);

/*------------------------------ 延时函数 ------------------------------*/

/**
 * @brief 毫秒级延时
 * @param xms 延时毫秒数
 */
void DEV_Delay_ms(UDOUBLE xms);

/*------------------------------ 模块控制函数 ------------------------------*/

/**
 * @brief 墨水屏模块初始化
 * @details 初始化SPI接口和所有GPIO引脚
 * @return 0=成功
 */
UBYTE DEV_Module_Init(void);

/**
 * @brief 墨水屏模块退出
 * @details 释放所有硬件资源
 */
void DEV_Module_Exit(void);


#endif /* _DEV_CONFIG_H_ */
