/**
 * @file lcd_gc9a01.h
 * @version 0.1
 * @date 2025-03-13
 */

#ifndef __LCD_GC9A01_H__
#define __LCD_GC9A01_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* GC9A01 commands */
#define GC9A01_NOP     0x00
#define GC9A01_SWRESET 0x01
#define GC9A01_RDDID   0x04
#define GC9A01_RDDST   0x09

#define GC9A01_RDDPM      0x0A // Read display power mode
#define GC9A01_RDD_MADCTL 0x0B // Read display MADCTL
#define GC9A01_RDD_COLMOD 0x0C // Read display pixel format
#define GC9A01_RDDIM      0x0D // Read display image mode
#define GC9A01_RDDSM      0x0E // Read display signal mode
#define GC9A01_RDDSR      0x0F // Read display self-diagnostic result (GC9A01)

#define GC9A01_SLPIN  0x10 // Enter Sleep Mode
#define GC9A01_SLPOUT 0x11 // Sleep out
#define GC9A01_PTLON  0x12
#define GC9A01_NORON  0x13

#define GC9A01_INVOFF  0x20
#define GC9A01_INVON   0x21
#define GC9A01_GAMSET  0x26 // Gamma set
#define GC9A01_DISPOFF 0x28
#define GC9A01_DISPON  0x29
#define GC9A01_CASET   0x2A // Column Address Set
#define GC9A01_RASET   0x2B // Row Address Set
#define GC9A01_RAMWR   0x2C
#define GC9A01_RGBSET  0x2D // Color setting for 4096, 64K and 262K colors
#define GC9A01_RAMRD   0x2E

#define GC9A01_PTLAR   0x30
#define GC9A01_VSCRDEF 0x33 // Vertical scrolling definition (GC9A01)
#define GC9A01_TEOFF   0x34 // Tearing effect line off
#define GC9A01_TEON    0x35 // Tearing effect line on
#define GC9A01_MADCTL  0x36 // Memory lcd_data access control
#define GC9A01_IDMOFF  0x38 // Idle mode off
#define GC9A01_IDMON   0x39 // Idle mode on
#define GC9A01_COLMOD  0x3A // Color mode
#define GC9A01_RAMWRC  0x3C // Memory write continue (GC9A01)
#define GC9A01_RAMRDC  0x3E // Memory read continue (GC9A01)

#define GC9A01_RAMCTRL   0xB0 // RAM control
#define GC9A01_RGBCTRL   0xB1 // RGB control
#define GC9A01_PORCTRL   0xB2 // Porch control
#define GC9A01_FRCTRL1   0xB3 // Frame rate control
#define GC9A01_PARCTRL   0xB5 // Partial mode control
#define GC9A01_GCTRL     0xB7 // Gate control
#define GC9A01_GTADJ     0xB8 // Gate on timing adjustment
#define GC9A01_DGMEN     0xBA // Digital gamma enable
#define GC9A01_VCOMS     0xBB // VCOMS setting
#define GC9A01_LCMCTRL   0xC0 // LCM control
#define GC9A01_IDSET     0xC1 // ID setting
#define GC9A01_VDVVRHEN  0xC2 // VDV and VRH command enable
#define GC9A01_VRHS      0xC3 // VRH set
#define GC9A01_VDVSET    0xC4 // VDV setting
#define GC9A01_VCMOFSET  0xC5 // VCOMS offset set
#define GC9A01_FRCTR2    0xC6 // FR Control 2
#define GC9A01_CABCCTRL  0xC7 // CABC control
#define GC9A01_REGSEL1   0xC8 // Register value section 1
#define GC9A01_REGSEL2   0xCA // Register value section 2
#define GC9A01_PWMFRSEL  0xCC // PWM frequency selection
#define GC9A01_PWCTRL1   0xD0 // Power control 1
#define GC9A01_VAPVANEN  0xD2 // Enable VAP/VAN signal output
#define GC9A01_CMD2EN    0xDF // Command 2 enable
#define GC9A01_PVGAMCTRL 0xE0 // Positive voltage gamma control
#define GC9A01_NVGAMCTRL 0xE1 // Negative voltage gamma control
#define GC9A01_DGMLUTR   0xE2 // Digital gamma look-up table for red
#define GC9A01_DGMLUTB   0xE3 // Digital gamma look-up table for blue
#define GC9A01_GATECTRL  0xE4 // Gate control
#define GC9A01_SPI2EN    0xE7 // SPI2 enable
#define GC9A01_PWCTRL2   0xE8 // Power control 2
#define GC9A01_EQCTRL    0xE9 // Equalize time control
#define GC9A01_PROMCTRL  0xEC // Program control
#define GC9A01_IREN2     0xEF // Inter register enable 2
#define GC9A01_PROMEN    0xFA // Program mode enable
#define GC9A01_NVMSET    0xFC // NVM setting
#define GC9A01_PROMACT   0xFE // Program action

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/


#ifdef __cplusplus
}
#endif

#endif /* __LCD_GC9A01_H__ */
