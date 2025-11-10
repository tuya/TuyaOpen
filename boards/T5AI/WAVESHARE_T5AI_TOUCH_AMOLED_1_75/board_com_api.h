/**
 * @file board_com_api.h
 * @author Tuya Inc.
 * @brief Header file for common board-level hardware registration APIs.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_COM_API_H__
#define __BOARD_COM_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Registers all the hardware peripherals (audio, button, LED) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void);
OPERATE_RET board_set_brightness(uint8_t value);
#ifdef __cplusplus
}
#endif

#endif /* __BOARD_COM_API_H__ */
