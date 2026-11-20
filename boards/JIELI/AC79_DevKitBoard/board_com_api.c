/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Board-level hardware registration for the AC79 devkit.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
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

/* The AC79 DevKit carries a 320x480 ILI9488 panel on the MCU8080 bus. It needs
 * both the 8080 backend (a chip-level switch that defaults on for AC79) and the
 * display component (follows ENABLE_LIBLVGL); guard on both or the TDL header
 * is pulled in for a component the build did not compile. The TKL 8080 backend
 * owns the DC/RST pins and the vendor `emi` device owns the bus timing, so
 * only the geometry and pixel format are stated here. */
#if defined(ENABLE_MCU8080) && (ENABLE_MCU8080 == 1) && defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
#include "tdd_disp_ili9488.h"

#define JIELI_8080_DISPLAY_NAME   "display"
#define JIELI_8080_DISPLAY_WIDTH  320
#define JIELI_8080_DISPLAY_HEIGHT 480
#endif

/* Exactly one camera module is fitted (the connectors share pins), and the
 * board Kconfig names it. Both modules hand the sensor to the TDD: the board
 * only supplies the I2C port, reset pin and clock below, and the platform
 * replays the tables once the sensor clock is up. */
#if defined(JIELI_BOARD_CAMERA_GC2145) && (JIELI_BOARD_CAMERA_GC2145 == 1)
#include "tdd_camera_dvp.h"
#include "tdd_camera_gc2145.h"

#define JIELI_CAMERA_DEV_NAME "camera"
#define JIELI_CAMERA_ASC      "GC2145 on J1 (DVP)"
#elif defined(JIELI_BOARD_CAMERA_GC0310) && (JIELI_BOARD_CAMERA_GC0310 == 1)
#include "tdd_camera_dvp.h"
#include "tdd_camera_gc0310_spi.h"

#define JIELI_CAMERA_DEV_NAME "camera"
#define JIELI_CAMERA_ASC      "GC0310 on J4 (SPI)"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* AC79_DevKitBoard (AC791) K1 ADC ladder calibration. */
#define JIELI_K1_ADC_NUM     TUYA_ADC_NUM_0
#define JIELI_K1_ADC_CHANNEL 3U /* PB1 / AD_CH_PB01 */
/* SDK ADKEY V6..V7 are 823..1007; the press range is (V6, V7]. */
#define JIELI_K1_PRESSED_MIN 824U
#define JIELI_K1_PRESSED_MAX 1007U

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

#if defined(JIELI_8080_DISPLAY_NAME)
    {
        /* The backlight is a plain GPIO on this board, driven by the vendor
         * panel config; the TDL brightness control is left to that path rather
         * than claimed here. */
        DISP_MCU8080_DEVICE_CFG_T disp_cfg = {
            .width     = JIELI_8080_DISPLAY_WIDTH,
            .height    = JIELI_8080_DISPLAY_HEIGHT,
            .x_offset  = 0,
            .y_offset  = 0,
            .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
            .rotation  = TUYA_DISPLAY_ROTATION_0,
            /* TE comes from the panel on PC0 (vendor demo_ui board data sets
             * .te_pin = IO_PORTC_00 for this devkit, and the schematic routes
             * the LCD connector's TE to PC0). Without it the 8080 write
             * lands mid scan-out and the picture shows a split line that
             * moves with the refresh phase. Pin number = port C * 16 + 0. */
            .te_pin  = TUYA_GPIO_NUM_32,
            .te_mode   = TUYA_GPIO_IRQ_RISE,
            /* The TKL 8080 backend stores this but the vendor `emi` device owns
             * the bus timing, so it is informational only. */
            .clk       = 0,
            .data_bits = 8,
            .bl        = {.type = TUYA_DISP_BL_TP_NONE},
            .power     = {.pin = TUYA_GPIO_NUM_MAX, .active_level = TUYA_GPIO_LEVEL_LOW},
        };
        ret = tdd_disp_mcu8080_ili9488_register((char *)JIELI_8080_DISPLAY_NAME, &disp_cfg);
        if (ret != OPRT_OK) {
            return ret;
        }
    }
#endif

#if defined(JIELI_CAMERA_DEV_NAME)
    {
        TDD_DVP_SR_USR_CFG_T camera_cfg = {
            .pwr = {.pin = BOARD_CAMERA_POWER_PIN, .active_level = TUYA_GPIO_LEVEL_LOW},
            .rst = {.pin = BOARD_CAMERA_RST_PIN, .active_level = TUYA_GPIO_LEVEL_LOW},
            .i2c = {.port = BOARD_CAMERA_I2C_PORT, .clk = BOARD_CAMERA_I2C_SCL, .sda = BOARD_CAMERA_I2C_SDA},
            .clk = BOARD_CAMERA_CLK,
        };

#if defined(JIELI_BOARD_CAMERA_GC2145) && (JIELI_BOARD_CAMERA_GC2145 == 1)
        /* DVP module: the sensor runs at the board geometry and tkl_dvp
         * delivers a centre crop of it, so BOARD_CAMERA_WIDTH/HEIGHT are the
         * sensor size, not the preview size. */
        ret = tdd_camera_dvp_gc2145_register((char *)JIELI_CAMERA_DEV_NAME, &camera_cfg);
#else
        /* SPI module: the TDD owns the sensor and tkl_dvp only opens spi2. */
        ret = tdd_camera_gc0310_spi_register((char *)JIELI_CAMERA_DEV_NAME, &camera_cfg);
#endif
        if (ret != OPRT_OK) {
            return ret;
        }
        PR_NOTICE("camera module: %s", JIELI_CAMERA_ASC);
    }
#endif

    return ret;
}
