/**
 * @file lcd_ili9488.h
 * @version 0.1
 * @date 2025-03-12
 */

#ifndef __LCD_ILI9488_H__
#define __LCD_ILI9488_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* ILI9488 commands */
#define ILI9488_NOP         0x00
#define ILI9488_SWRESET     0x01
#define ILI9488_RDDID       0x04
#define ILI9488_RDDST       0x09

#define ILI9488_SLPIN       0x10
#define ILI9488_SLPOUT      0x11
#define ILI9488_PTLON       0x12
#define ILI9488_NORON       0x13

#define ILI9488_RDMODE      0x0A
#define ILI9488_RDMADCTL    0x0B
#define ILI9488_RDPIXFMT    0x0C
#define ILI9488_RDIMGFMT    0x0D
#define ILI9488_RDSELFDIAG  0x0F

#define ILI9488_INVOFF      0x20
#define ILI9488_INVON       0x21
#define ILI9488_GAMMASET    0x26
#define ILI9488_DISPOFF     0x28
#define ILI9488_DISPON      0x29

#define ILI9488_CASET       0x2A
#define ILI9488_PASET       0x2B
#define ILI9488_RAMWR       0x2C
#define ILI9488_RAMRD       0x2E

#define ILI9488_PTLAR       0x30
#define ILI9488_MADCTL      0x36
#define ILI9488_PIXFMT      0x3A

#define ILI9488_IFMODE      0xB0
#define ILI9488_FRMCTR1     0xB1
#define ILI9488_FRMCTR2     0xB2
#define ILI9488_FRMCTR3     0xB3
#define ILI9488_INVCTR      0xB4
#define ILI9488_PRCTR       0xB5
#define ILI9488_DFUNCTR     0xB6

#define ILI9488_PWCTR1      0xC0
#define ILI9488_PWCTR2      0xC1
#define ILI9488_PWCTR3      0xC2
#define ILI9488_PWCTR4      0xC3
#define ILI9488_PWCTR5      0xC4
#define ILI9488_VMCTR1      0xC5
#define ILI9488_VMCTR2      0xC7

#define ILI9488_RDID1       0xDA
#define ILI9488_RDID2       0xDB
#define ILI9488_RDID3       0xDC
#define ILI9488_RDID4       0xDD

#define ILI9488_GMCTRP1     0xE0
#define ILI9488_GMCTRN1     0xE1
#define ILI9488_SETIMAGE    0xE9

#define ILI9488_ACTRL3      0xF7
#define ILI9488_ACTRL4      0xF8
/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/


#ifdef __cplusplus
}
#endif

#endif /* __LCD_ILI9488_H__ */
