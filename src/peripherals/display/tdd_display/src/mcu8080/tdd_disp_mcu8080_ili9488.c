/**
 * @file tdd_disp_mcu8080_ili9488.c
 * @brief ILI9488 panel configuration for the generic MCU8080 TDD.
 *
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#include "tdd_disp_ili9488.h"
#include "tdd_display_mcu8080.h"

/* Each row is: number of command/parameter words, delay in ms, command, params. */
static const uint32_t c_ili9488_init_seq[] = {
    1,    0,    0x21,                         /* Display inversion on */
    2,    0,    0x35, 0x00,                   /* Tearing effect line */
    3,    0,    0xB1, 0x90, 0x11,             /* Frame rate */
    2,    0,    0x36, 0xD8,                   /* Memory access control */
    2,    0,    0xC1, 0x41,                   /* Power control */
    5,    0,    0xF7, 0xA9, 0x51, 0x2C, 0x82, /* Adjust control 3 */
    2,    0,    0xC1, 0x41, 3,    0,    0xC0, 0x0F, 0x0F, 2,    0,    0xC2, 0x22, 4,    0,
    0xC5, 0x00, 0x53, 0x80, 2,    0,    0xB4, 0x02, 2,    0,    0xB7, 0xC6, 3,    0,    0xB6,
    0x02, 0x42, 3,    0,    0xBE, 0x00, 0x04, 2,    0,    0xE9, 0x00, 2,    0,    0x3A, 0x55, /* RGB565 */
    1,    200,  0x11,                                                                         /* Sleep out */
    1,    0,    0x29,                                                                         /* Display on */
    0,
};

OPERATE_RET tdd_disp_mcu8080_ili9488_register(char *name)
{
    if (name == NULL) {
        return OPRT_INVALID_PARM;
    }

    static TDD_DISP_MCU8080_CFG_T cfg = {
        .cfg =
            {
                .clk       = 0,
                .data_bits = 8,
                .width     = 320,
                .height    = 480,
                .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
                .dc_pin    = TUYA_GPIO_NUM_41, /* PC9: command/data select */
                .rst_pin   = TUYA_GPIO_NUM_16, /* PB0: LCD reset */
            },
        .bl =
            {
                .type = TUYA_DISP_BL_TP_GPIO,
                .gpio =
                    {
                        .pin          = TUYA_GPIO_NUM_24, /* PB8 */
                        .active_level = TUYA_GPIO_LEVEL_LOW,
                    },
            },
        .power =
            {
                .pin          = TUYA_GPIO_NUM_MAX,
                .active_level = TUYA_GPIO_LEVEL_LOW,
            },
        .rotation   = TUYA_DISPLAY_ROTATION_0,
        .in_fmt     = TUYA_PIXEL_FMT_RGB565,
        .is_swap    = true,
        .te_pin     = TUYA_GPIO_NUM_MAX,
        .te_mode    = TUYA_GPIO_IRQ_RISE,
        .cmd_caset  = ILI9488_CASET,
        .cmd_raset  = ILI9488_PASET,
        .cmd_ramwr  = ILI9488_RAMWR,
        .cmd_ramwrc = 0x3C,
        .init_seq   = c_ili9488_init_seq,
    };

    return tdd_disp_mcu8080_device_register(name, &cfg);
}

OPERATE_RET tdd_disp_emi_wl82_register(char *name)
{
    return tdd_disp_mcu8080_ili9488_register(name);
}
