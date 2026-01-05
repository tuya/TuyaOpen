/*****************************************************************************
* | File      	:   DEV_Config.h
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*                Used to shield the underlying layers of each master
*                and enhance portability
*----------------
* |	This version:   V1.0
* | Date        :   2025-11-19
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
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "Debug.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tal_cli.h"
#include "tkl_spi.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#include "tkl_timer.h"  
#include "tkl_system.h" 
#include "tal_system.h"


/**
 * data
**/
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

/**
 * GPIOI config
**/


#define EPD_MOSI_PIN    TUYA_GPIO_NUM_16
#define EPD_SCLK_PIN    TUYA_GPIO_NUM_14
#define EPD_CS_PIN      TUYA_GPIO_NUM_18
#define EPD_DC_PIN      TUYA_GPIO_NUM_19
#define EPD_RST_PIN     TUYA_GPIO_NUM_47
#define EPD_BUSY_PIN    TUYA_GPIO_NUM_46


#define EPD_TSCL_PIN    TUYA_GPIO_NUM_42
#define EPD_TSDA_PIN    TUYA_GPIO_NUM_43

#define EPD_TINT_PIN    TUYA_GPIO_NUM_44
#define EPD_TRST_PIN    TUYA_GPIO_NUM_40


// #define TRST_0		DEV_Digital_Write(EPD_TRST_PIN, 0)
// #define TRST_1		DEV_Digital_Write(EPD_TRST_PIN, 1)

// #define INT_0		DEV_Digital_Write(EPD_TINT_PIN, 0)
// #define INT_1		DEV_Digital_Write(EPD_TINT_PIN, 1)

/**
 * SPI config
**/
#define SPI_ID          TUYA_SPI_NUM_0
#define SPI_FREQ        4 * 1000 * 1000  // 4M

/**
 * I2C config
**/
#define  I2C_NUM        TUYA_I2C_NUM_1
#define  IIC_SCL        TUYA_IIC1_SCL
#define  IIC_SDA        TUYA_IIC1_SDA
#define  IIC_Address    0x14


/*------------------------------------------------------------------------------------------------------*/
void DEV_Digital_Write(UWORD Pin, UBYTE Value);
UBYTE DEV_Digital_Read(UWORD Pin);

void DEV_SPI_WriteByte(uint8_t Value);
void DEV_SPI_Write_nByte(uint8_t *pData, uint32_t Len);
UBYTE I2C_Write_Byte(UWORD Reg, char *Data, UBYTE len);
UBYTE I2C_Read_Byte(UWORD Reg, char *Data, UBYTE len);
void DEV_Delay_ms(UDOUBLE xms);

void DEV_SPI_SendData(UBYTE Reg);
void DEV_SPI_SendnData(UBYTE *Reg);
UBYTE DEV_SPI_ReadData();

UBYTE DEV_Module_Init(void);
void DEV_Module_Exit(void);


#endif
