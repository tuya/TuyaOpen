/**
 * @file tdd_disp_nv3041.h
 * @brief Header file for NV3041 QSPI display driver interface
 *
 * This file provides the interface definitions for the NV3041 QSPI display controller,
 * including register definitions, command macros, and function declarations for display
 * initialization and device registration.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
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

#define NV3041_CASET                0x2A // Column Address Set
#define NV3041_RASET                0x2B // Row Address Set

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Sets the initialization sequence for the NV3041 display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_qspi_nv3041_set_init_seq(const uint8_t *init_seq);

/**
 * @brief Registers the NV3041 QSPI display device with the display driver
 * 
 * @param name Device name to register
 * @param dev_cfg Pointer to the QSPI device configuration structure
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if parameters are NULL
 */
OPERATE_RET tdd_disp_qspi_nv3041_register(char *name, DISP_QSPI_DEVICE_CFG_T *dev_cfg);


#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_NV3041_H__ */
