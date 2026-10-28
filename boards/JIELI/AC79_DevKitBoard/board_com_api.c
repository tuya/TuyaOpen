/**
 * @file board_com_api.c
 * @brief Hardware registration for the AC79 DevKitBoard.
 *
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#include "board_com_api.h"
#include "tal_api.h"

#if defined(ENABLE_JIELI_ADKEY_BUTTON) && (ENABLE_JIELI_ADKEY_BUTTON == 1)
#include "tdd_button_adc.h"
#endif

#if defined(ENABLE_MEDIA) && (ENABLE_MEDIA == 1)
#include "tdd_audio.h"
#include "tkl_audio.h"

#ifndef AUDIO_CODEC_NAME
#define AUDIO_CODEC_NAME "audio_codec"
#endif
#endif

#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
#include "tdd_disp_ili9488.h"

#ifndef DISPLAY_NAME
#define DISPLAY_NAME "display"
#endif
#endif

#if defined(ENABLE_TP) && (ENABLE_TP == 1)
#include "tdd_tp_ft6336.h"
#include "tdd_tp_gt911.h"
#include "tkl_i2c.h"
#endif

/* JieLi vendor device and bus headers */
#undef BIT
#undef LIST_HEAD
#undef LIST_HEAD_INIT
#undef INIT_LIST_HEAD
#include "system/init.h"
#include "device/device.h"
#include "asm/emi.h"
#include "device/iic.h"
#include "asm/gpio.h"

/***********************************************************
************************macro define************************
***********************************************************/
/* AC79_DevKitBoard (AC791) K1 ADC ladder calibration. */
#define JIELI_K1_ADC_NUM     TUYA_ADC_NUM_0
#define JIELI_K1_ADC_CHANNEL 3U /* PB1 / AD_CH_PB01 */
/* SDK ADKEY V6..V7 are 823..1007; the press range is (V6, V7]. */
#define JIELI_K1_PRESSED_MIN 824U
#define JIELI_K1_PRESSED_MAX 1007U

/* 8-bit parallel EMI platform configuration for DevKitLCD */
#define EMI_BAUD EMI_BAUD_DIV8
static const struct emi_platform_data emi_data = {
    .bits_mode   = EMI_8BITS_MODE,
    .baudrate    = EMI_BAUD,
    .colection   = EMI_FALLING_COLT,
    .time_out    = 1 * 1000,
    .th          = EMI_TWIDTH_NO_HALF,
    .ts          = 0,
    .tw          = EMI_BAUD,
    .data_bit_en = 0,
};

/* Software I2C platform data on PORTH_00 (SCL) and PORTH_01 (SDA) */
SW_IIC_PLATFORM_DATA_BEGIN(sw_iic0_data).clk_pin = IO_PORTH_00, .dat_pin = IO_PORTH_01, .sw_iic_delay = 50,
                                                                SW_IIC_PLATFORM_DATA_END()

                                                                /* Register devices into JieLi SDK .device section */
                                                                REGISTER_DEVICES(ac79_devices) = {
                                                                    {"emi", &emi_dev_ops, (void *)&emi_data},
                                                                    {"iic0", &iic_dev_ops, (void *)&sw_iic0_data},
};

#if defined(ENABLE_TP) && (ENABLE_TP == 1)
static OPERATE_RET __board_register_touch(void)
{
    TUYA_IIC_BASE_CFG_T i2c_cfg = {
        .role       = TUYA_IIC_MODE_MASTER,
        .speed      = TUYA_IIC_BUS_SPEED_100K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT,
    };
    tkl_i2c_init(TUYA_I2C_NUM_0, &i2c_cfg);

    /* 1. Try probing FT6236 / FT6336 at 7-bit address 0x38 */
    uint8_t chip_id = 0;
    if (tkl_i2c_master_receive(TUYA_I2C_NUM_0, 0x38, &chip_id, 1, FALSE) == OPRT_OK) {
        PR_NOTICE("AC79_DevKitBoard TP probe: detected FT6236/FT6336 (0x38)");
        TDD_TP_FT6336_INFO_T ft_cfg = {
            /* PB0 is shared with the ILI9488 reset line; do not pulse it again
             * when LVGL opens the touch device after the display. */
            .rst_pin  = TUYA_GPIO_NUM_MAX,
            .intr_pin = IO_PORTB_04,
            .i2c_cfg =
                {
                    .port    = TUYA_I2C_NUM_0,
                    .scl_pin = IO_PORTH_00,
                    .sda_pin = IO_PORTH_01,
                },
            .tp_cfg =
                {
                    .x_max = 320,
                    .y_max = 480,
                    .flags =
                        {
                            .mirror_x = 0,
                            .mirror_y = 0,
                            .swap_xy  = 0,
                        },
                },
        };
        return tdd_tp_i2c_ft6336_register((char *)DISPLAY_NAME, &ft_cfg);
    }

    /* 2. Try probing GT911 at 7-bit addresses 0x14 or 0x5D */
    uint8_t        reg_addr[2] = {0x81, 0x40};
    uint8_t        id_buf[4]   = {0};
    const uint16_t gt_addrs[]  = {0x14, 0x5D};

    for (int i = 0; i < 2; i++) {
        if (tkl_i2c_master_send(TUYA_I2C_NUM_0, gt_addrs[i], reg_addr, 2, FALSE) == OPRT_OK) {
            if (tkl_i2c_master_receive(TUYA_I2C_NUM_0, gt_addrs[i], id_buf, 4, FALSE) == OPRT_OK) {
                PR_NOTICE("AC79_DevKitBoard TP probe: detected GT911 (0x%02X)", gt_addrs[i]);
                TDD_TP_GT911_INFO_T gt_cfg = {
                    .i2c_cfg =
                        {
                            .port    = TUYA_I2C_NUM_0,
                            .scl_pin = IO_PORTH_00,
                            .sda_pin = IO_PORTH_01,
                        },
                    .tp_cfg =
                        {
                            .x_max = 240,
                            .y_max = 320,
                            .flags =
                                {
                                    .mirror_x = 0,
                                    .mirror_y = 0,
                                    .swap_xy  = 0,
                                },
                        },
                };
                return tdd_tp_i2c_gt911_register((char *)DISPLAY_NAME, &gt_cfg);
            }
        }
    }

    PR_NOTICE("AC79_DevKitBoard TP probe: no touch chip detected, using default FT6236");
    TDD_TP_FT6336_INFO_T ft_default = {
        /* PB0 is shared with the ILI9488 reset line; do not pulse it again
         * when LVGL opens the touch device after the display. */
        .rst_pin  = TUYA_GPIO_NUM_MAX,
        .intr_pin = IO_PORTB_04,
        .i2c_cfg =
            {
                .port    = TUYA_I2C_NUM_0,
                .scl_pin = IO_PORTH_00,
                .sda_pin = IO_PORTH_01,
            },
        .tp_cfg =
            {
                .x_max = 320,
                .y_max = 480,
                .flags =
                    {
                        .mirror_x = 0,
                        .mirror_y = 0,
                        .swap_xy  = 0,
                    },
            },
    };
    return tdd_tp_i2c_ft6336_register((char *)DISPLAY_NAME, &ft_default);
}
#endif

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET ret = OPRT_OK;

#if defined(ENABLE_MEDIA) && (ENABLE_MEDIA == 1)
    TDD_AUDIO_T5AI_T audio_cfg = {
        .aec_enable       = FALSE,
        .ai_chn           = TKL_AI_0,
        .sample_rate      = TKL_AUDIO_SAMPLE_16K,
        .data_bits        = TKL_AUDIO_DATABITS_16,
        .channel          = TKL_AUDIO_CHANNEL_MONO,
        .spk_sample_rate  = TKL_AUDIO_SAMPLE_16K,
        .spk_pin          = 0,
        .spk_pin_polarity = 0,
    };
    ret = tdd_audio_register((char *)AUDIO_CODEC_NAME, audio_cfg);
    if (ret != OPRT_OK) {
        return ret;
    }
#endif

#if defined(ENABLE_JIELI_ADKEY_BUTTON) && (ENABLE_JIELI_ADKEY_BUTTON == 1)
    BUTTON_ADC_CFG_T button_cfg = {
        .adc_num     = JIELI_K1_ADC_NUM,
        .adc_ch      = JIELI_K1_ADC_CHANNEL,
        .pressed_min = JIELI_K1_PRESSED_MIN,
        .pressed_max = JIELI_K1_PRESSED_MAX,
    };
    ret = tdd_adc_button_register((char *)BUTTON_NAME, &button_cfg);
    if (ret != OPRT_OK) {
        return ret;
    }
#endif

#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
    ret = tdd_disp_mcu8080_ili9488_register((char *)DISPLAY_NAME);
    if (ret != OPRT_OK) {
        PR_ERR("MCU8080 ILI9488 display register failed: %d", ret);
        return ret;
    }
#endif

#if defined(ENABLE_TP) && (ENABLE_TP == 1)
    ret = __board_register_touch();
    if (ret != OPRT_OK) {
        PR_WARN("touch registration returned: %d", ret);
    }
#endif

    return OPRT_OK;
}
