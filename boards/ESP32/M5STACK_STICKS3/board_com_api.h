/**
 * @file board_com_api.h
 * @brief Common board-level hardware registration APIs for M5Stack StickS3.
 * @version 0.1
 * @date 2026-04-27
 * @copyright Copyright (c) Tuya Inc. All Rights Reserved.
 */
#ifndef __BOARD_COM_API_H__
#define __BOARD_COM_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Register all board hardware peripherals.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_register_hardware(void);

#ifdef __cplusplus
}
#endif
#endif /* __BOARD_COM_API_H__ */
