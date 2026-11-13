/**
 * @file tdd_disp_st77916.h
 * @brief ST77916 QSPI display driver interface.
 *
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_ST77916_H__
#define __TDD_DISP_ST77916_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define ST77916_CASET  0x2A
#define ST77916_RASET  0x2B
#define ST77916_RAMWR  0x2C
#define ST77916_COLMOD 0x3A
#define ST77916_MADCTL 0x36

#define ST77916_MADCTL_ROTATE_90_CCW 0xA0

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Set a panel-specific initialization sequence for the ST77916 QSPI driver.
 *
 * @param[in] init_seq Sequence in the common TDD display format:
 *                     length, delay_ms, command, data...
 *
 * @return OPRT_OK on success, OPRT_INVALID_PARM if init_seq is NULL.
 */
OPERATE_RET tdd_disp_qspi_st77916_set_init_seq(const uint8_t *init_seq);

/**
 * @brief Register an ST77916 QSPI display device.
 *
 * @param name Device name used by TDL display management.
 * @param cfg Board resource and panel configuration.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdd_disp_qspi_st77916_register(char *name,
                                            DISP_QSPI_DEVICE_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ST77916_H__ */
