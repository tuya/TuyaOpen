/**
 * @file board_com_api.h
 * @author Tuya Inc.
 * @brief Header file for common board-level hardware registration APIs for Raspberry Pi platform.
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
 * @brief Registers all the hardware peripherals on the Raspberry Pi platform.
 * 
 * This function initializes and registers hardware components including:
 * - ALSA audio device (if ENABLE_AUDIO_ALSA is enabled)
 * - Other platform-specific peripherals
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_COM_API_H__ */

