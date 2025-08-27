/**
 * @file tdd_disp_nv3041.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_NV3041_H__
#define __TDD_DISP_NV3041_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define NV3041_WRITE_REG            0x02

#define NV3041_WRITE_COLOR          0x32
#define NV3041_ADDR_LEN             3
#define NV3041_ADDR_0               0x00
#define NV3041_ADDR_1               0x2C
#define NV3041_ADDR_2               0x00

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdd_disp_qspi_nv3041_register(char *name, DISP_QSPI_DEVICE_CFG_T *dev_cfg);


#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_NV3041_H__ */
