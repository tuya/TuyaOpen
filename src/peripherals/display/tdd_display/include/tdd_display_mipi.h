/**
 * @file tdd_display_mipi.h
 * @brief MIPI-DSI display TDD interface
 *
 * A MIPI-DSI panel has no GRAM of its own: the host refreshes it continuously
 * from a frame buffer, exactly like a parallel RGB panel. That is why the TDD
 * surface is the same shape as the RGB one - a device info block plus an
 * open/flush/close triple - while the bus underneath is DSI.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISPLAY_MIPI_H__
#define __TDD_DISPLAY_MIPI_H__

#include "tdl_display_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t                 width;
    uint16_t                 height;
    TUYA_DISPLAY_PIXEL_FMT_E fmt;
    TUYA_DISPLAY_ROTATION_E  rotation;
    bool                     is_swap;
    TUYA_DISPLAY_BL_CTRL_T   bl;
    TUYA_DISPLAY_IO_CTRL_T   power;
} TDD_DISP_MIPI_CFG_T;

/**
 * @brief Register a MIPI-DSI display device with the display manager.
 *
 * @param name Device name the application passes to tdl_disp_find_dev().
 * @param cfg  Panel geometry and control configuration.
 *
 * @return OPRT_OK on success.
 */
OPERATE_RET tdd_disp_mipi_device_register(char *name, TDD_DISP_MIPI_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISPLAY_MIPI_H__ */
