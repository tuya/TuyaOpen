/**
 * @file tuya_t9_evb.c
 * @brief TUYA_T9_EVB board hardware registration
 *
 * Board-side entry point shared by the apps that call
 * board_register_hardware(). The T9 EVB currently carries Wi-Fi/BLE only;
 * the audio path, camera and display are not populated yet, so no adapter is
 * registered for them.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

/**
 * @brief Register board hardware
 *
 * T9 EVB has no board-level device to register yet. Audio needs the platform
 * tkl_audio backend first; camera and display are not populated.
 */
OPERATE_RET board_register_hardware(void)
{
    return OPRT_OK;
}
