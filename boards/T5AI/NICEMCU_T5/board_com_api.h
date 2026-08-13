/**
 * @file board_com_api.h
 * @brief NiceMCU-T5 board hardware registration API
 * @version 0.1
 * @date 2026-08-12
 * @copyright Copyright (c) Tuya Inc. All Rights Reserved.
 */
#ifndef __BOARD_COM_API_H__
#define __BOARD_COM_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register board peripherals (INMP441 + MAX98357 I2S audio)
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_register_hardware(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_COM_API_H__ */
