/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for ESP32-S31 module.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "board_com_api.h"

/**
 * @brief Registers the base hardware for ESP32-S31 module.
 *        The base module does not register any specific peripherals.
 *        Concrete boards (e.g. Korvo-1) should override this with their own implementation.
 *
 * @return OPRT_OK
 */
OPERATE_RET board_register_hardware(void)
{
    return OPRT_OK;
}