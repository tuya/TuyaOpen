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


/**
 * data
**/
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

/**
 * GPIOI config
**/
#ifndef EPD_SCLK_PIN
#define EPD_SCLK_PIN    TUYA_GPIO_NUM_2
#endif

#ifndef EPD_MOSI_PIN
#define EPD_MOSI_PIN    TUYA_GPIO_NUM_4
#endif

#ifndef EPD_CS_PIN      
#define EPD_CS_PIN      TUYA_GPIO_NUM_3
#endif

#ifndef EPD_DC_PIN
#define EPD_DC_PIN      TUYA_GPIO_NUM_7
#endif

#ifndef EPD_RST_PIN
#define EPD_RST_PIN     TUYA_GPIO_NUM_8
#endif

#ifndef EPD_BUSY_PIN
#define EPD_BUSY_PIN    TUYA_GPIO_NUM_6
#endif

#ifndef EPD_PWR_PIN
#define EPD_PWR_PIN     TUYA_GPIO_NUM_28
#endif

/**
 * SPI config
**/
#define SPI_ID          TUYA_SPI_NUM_1
#define SPI_FREQ        4 * 1000 * 1000  // 4M

/*------------------------------------------------------------------------------------------------------*/
void DEV_Digital_Write(UWORD Pin, UBYTE Value);
UBYTE DEV_Digital_Read(UWORD Pin);

void DEV_SPI_WriteByte(UBYTE Value);
void DEV_SPI_Write_nByte(uint8_t *pData, uint32_t Len);
void DEV_Delay_ms(UDOUBLE xms);

void DEV_SPI_SendData(UBYTE Reg);
void DEV_SPI_SendnData(UBYTE *Reg);
UBYTE DEV_SPI_ReadData();

UBYTE DEV_Module_Init(void);
void DEV_Module_Exit(void);


#endif
