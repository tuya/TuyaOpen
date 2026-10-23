/**
 * @file tdd_display_emi_wl82.h
 * @brief JieLi WL82 (AC791N) 8-bit EMI parallel LCD TDD driver header file
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISPLAY_EMI_WL82_H__
#define __TDD_DISPLAY_EMI_WL82_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register JieLi WL82 EMI LCD display device
 *
 * Automatically detects panel model (ILI9341, ILI9488, or ILI9481),
 * initializes EMI hardware bus and registers display device to TDL.
 *
 * @param[in] name Display device name (e.g. "display")
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET tdd_disp_emi_wl82_register(char *name);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISPLAY_EMI_WL82_H__ */
