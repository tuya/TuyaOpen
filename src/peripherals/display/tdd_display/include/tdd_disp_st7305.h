/**
 * @file tdd_disp_st7305.h
 * @version 0.1
 * @date 2025-03-12
 */

#ifndef __TDD_DISP_ST7305_H__
#define __TDD_DISP_ST7305_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

/***********************************************************
***********************macro define***********************
***********************************************************/


/***********************************************************
***********************type define***********************
***********************************************************/
#define ST7305_CASET     0x2A // Column Address Set
#define ST7305_RASET     0x2B // Row Address Set
#define ST7305_RAMWR     0x2C

/***********************************************************
***********************function declare**********************
***********************************************************/
OPERATE_RET tdd_disp_spi_mono_st7305_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg, uint8_t caset_xs);

#endif // __TDD_DISP_SPI_ST7305_H__
