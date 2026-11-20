/**
 * @file tdd_disp_jd9165t.h
 * @brief JD9165T MIPI-DSI panel driver header.
 *
 * The panel driver owns everything that describes the JD9165T itself: timing,
 * lane count, the DCS init sequence and the default reset delays. A board only
 * supplies its wiring through TDD_DISP_MIPI_BOARD_IO_T.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISP_JD9165T_H__
#define __TDD_DISP_JD9165T_H__

#include "tuya_cloud_types.h"
#include "tdd_display_mipi.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Overrides the built-in JD9165T init sequence.
 *
 * The table must stay valid after this call returns (static storage).
 *
 * @param cmds Pointer to the replacement sequence.
 * @param num  Number of entries in the sequence.
 *
 * @return OPRT_OK on success, OPRT_INVALID_PARM on bad parameters.
 */
OPERATE_RET tdd_disp_mipi_jd9165t_set_init_cmds(const TDD_DISP_MIPI_INIT_CMD_T *cmds, uint16_t num);

/**
 * @brief Registers a JD9165T panel with the MIPI bus driver.
 *
 * @param name Display device name, passed through to the display manager.
 * @param io   Board wiring; NULL is rejected.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdd_disp_mipi_jd9165t_register(char *name, const TDD_DISP_MIPI_BOARD_IO_T *io);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_JD9165T_H__ */
