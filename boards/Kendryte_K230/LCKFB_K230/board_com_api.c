/**
 * @file board_com_api.c
 * @brief LCKFB K230 board hardware registration (stub for first bring-up).
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "board_com_api.h"

/**
 * TODO(Stage 1): register the LCKFB K230 board LED/button via tdd_gpio_*
 * (tdd_led_gpio_register / tdd_gpio_button_register) once the pinout is
 * confirmed - see platform/Kendryte_K230/docs/reference/lckfb-k230-board.md.
 * Console is UART0 @115200 3.3V (pins IO38/IO39).
 */
OPERATE_RET board_register_hardware(void)
{
    return OPRT_OK;
}
