/**
 * @file board_com_api.h
 * @author Tuya Inc.
 * @brief Header file for common board-level hardware registration APIs.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_COM_API_H__
#define __BOARD_COM_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET board_register_hardware(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_COM_API_H__ */
