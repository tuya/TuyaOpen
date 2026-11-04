/**
 * @file board_com_api.h
 * @brief Common board-level hardware registration APIs (T9).
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_COM_API_H__
#define __BOARD_COM_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register all the hardware peripherals on the board.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_COM_API_H__ */
