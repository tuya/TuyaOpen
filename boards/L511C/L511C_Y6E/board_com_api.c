/**
 * @file board_com_api.c
 * @brief Common board-level hardware registration for L511C_Y6E.
 *
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#include "board_com_api.h"

OPERATE_RET board_register_hardware(void)
{
    /* No board peripherals are registered until the wiring is defined. */
    return OPRT_OK;
}
