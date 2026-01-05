/**
 * @file tdd_disp_co5300.h
 * @brief Header file for CO5300 QSPI display driver interface
 *
 * This file provides the interface definitions for the CO5300 QSPI display controller,
 * including register definitions, configuration structures, and function declarations
 * for display initialization, registration, and backlight control.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
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
/**
 * @brief Sets the initialization sequence for the CO5300 display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_qspi_co5300_set_init_seq(const uint8_t *init_seq);

/**
 * @brief Send command to set backlight brightness for CO5300 display via QSPI
 * 
 * @param brightness Backlight brightness value (0-100)
 * @param arg Pointer to additional arguments or context (can be NULL)
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or error code on failure
 */
OPERATE_RET tdd_qspi_co5300_send_cmd_set_bl(uint8_t brightness, void *arg);

/**
 * @brief Registers the CO5300 QSPI display device with the display driver
 * 
 * @param name Device name to register
 * @param dev_cfg Pointer to the QSPI device configuration structure
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if parameters are NULL
 */
OPERATE_RET tdd_disp_qspi_co5300_register(char *name, DISP_QSPI_DEVICE_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_CO5300_H__ */
