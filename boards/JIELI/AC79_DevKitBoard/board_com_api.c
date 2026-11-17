/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Board-level hardware registration for the AC79 devkit.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "board_com_api.h"

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

/* The panel needs both the 8080 backend and the display component: the
 * former is a chip-level switch that defaults on for AC79, while the
 * latter follows ENABLE_LIBLVGL. Guard on both or the TDL header is
 * pulled in for a component the build did not compile. */
#if defined(ENABLE_MCU8080) && (ENABLE_MCU8080 == 1) && defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
#include "tdd_disp_ili9488.h"
#endif

/* The camera TDD owns the frame pool and the open/close state machine; the
 * sensor itself (probe, I2C, registers) is the vendor video pipeline's job,
 * so the sensor interface table below stays empty. The board Kconfig choice
 * selects which vendor device the TKL adapter drives (video0 for DVP, video1
 * plus SPI2 for the 12-pin path); both share PA2/PA3/PA4 and PH0-PH3 and
 * cannot be built together. */
#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1) &&                                                                  \
    (defined(JIELI_CAMERA_PATH_DVP) || defined(JIELI_CAMERA_PATH_SPI)) &&                                              \
    ((defined(JIELI_CAMERA_PATH_DVP) && (JIELI_CAMERA_PATH_DVP == 1)) ||                                               \
     (defined(JIELI_CAMERA_PATH_SPI) && (JIELI_CAMERA_PATH_SPI == 1)))
#include "tdd_camera_dvp.h"
#define JIELI_CAMERA_DEV_NAME "camera"
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

/* The AC79 DevKit carries a 320x480 ILI9488 panel on the MCU8080 bus. The TKL
 * 8080 backend owns the DC/RST pins and the vendor `emi` device owns the bus
 * timing, so only the geometry and pixel format are stated here. */
/* The panel needs both the 8080 backend and the display component: the
 * former is a chip-level switch that defaults on for AC79, while the
 * latter follows ENABLE_LIBLVGL. Guard on both or the TDL header is
 * pulled in for a component the build did not compile. */
#if defined(ENABLE_MCU8080) && (ENABLE_MCU8080 == 1) && defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
#define JIELI_8080_DISPLAY_NAME   "display"
#define JIELI_8080_DISPLAY_WIDTH  320
#define JIELI_8080_DISPLAY_HEIGHT 480
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

/* The panel needs both the 8080 backend and the display component: the
 * former is a chip-level switch that defaults on for AC79, while the
 * latter follows ENABLE_LIBLVGL. Guard on both or the TDL header is
 * pulled in for a component the build did not compile. */
#if defined(ENABLE_MCU8080) && (ENABLE_MCU8080 == 1) && defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
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
            .te_mode = TUYA_GPIO_IRQ_RISE,
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

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1) &&                                                                  \
    ((defined(JIELI_CAMERA_PATH_DVP) && (JIELI_CAMERA_PATH_DVP == 1)) ||                                               \
     (defined(JIELI_CAMERA_PATH_SPI) && (JIELI_CAMERA_PATH_SPI == 1)))
    {
        /* The vendor sensor drivers own I2C, reset and the register table,
         * and the TKL adapter owns the capture device. Nothing in this
         * structure opens an I2C port or toggles a GPIO: TUYA_I2C_NUM_MAX /
         * TUYA_GPIO_NUM_MAX disable those TDD-side steps. */
        TDD_DVP_SR_CFG_T camera_cfg = {
            .usr_cfg =
                {
                    .pwr = {.pin = TUYA_GPIO_NUM_MAX, .active_level = TUYA_GPIO_LEVEL_LOW},
                    .rst = {.pin = TUYA_GPIO_NUM_MAX, .active_level = TUYA_GPIO_LEVEL_LOW},
                    .i2c = {.port = TUYA_I2C_NUM_MAX},
                    .clk = 24000000,
                },
            .max_fps    = JIELI_BOARD_CAMERA_FPS,
            .max_width  = JIELI_BOARD_CAMERA_WIDTH,
            .max_height = JIELI_BOARD_CAMERA_HEIGHT,
            .fmt        = TUYA_FRAME_FMT_YUV422,
        };
        TDD_DVP_SR_INTFS_T camera_intfs = {0};

        ret = tdl_camera_dvp_device_register((char *)JIELI_CAMERA_DEV_NAME, &camera_cfg, &camera_intfs);
        if (ret != OPRT_OK) {
            return ret;
        }
        printf("camera path: %s\n",
#if defined(JIELI_CAMERA_PATH_DVP) && (JIELI_CAMERA_PATH_DVP == 1)
               "DVP (J1/video0)"
#else
               "SPI (J4/video1+spi2)"
#endif
        );
    }
#endif

    return ret;
}
