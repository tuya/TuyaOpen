/**
 * @file tdd_disp_co5300.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_CO5300_H__
#define __TDD_DISP_CO5300_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define CO5300_WRITE_REG            0x02

#define CO5300_WRITE_COLOR          0x32
#define CO5300_ADDR_LEN             3
#define CO5300_ADDR_0               0x00
#define CO5300_ADDR_1               0x2C
#define CO5300_ADDR_2               0x00

#define CO5300_CASET                0x2A // Column Address Set
#define CO5300_RASET                0x2B // Row Address Set

#define CO5300_BL                   0x51 // Set backlight
/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdd_disp_qspi_co5300_register(char *name, DISP_QSPI_DEVICE_CFG_T *dev_cfg);
OPERATE_RET tdd_disp_qspi_co5300_set_bl(uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_CO5300_H__ */
