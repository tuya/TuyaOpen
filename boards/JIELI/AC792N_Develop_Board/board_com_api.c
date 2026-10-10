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

#if defined(ENABLE_MIPI_DSI) && (ENABLE_MIPI_DSI == 1)
#include "tdd_display_mipi.h"

#ifndef DISPLAY_NAME
#define DISPLAY_NAME "display"
#endif

/* Panel geometry and the backlight/power lines. The reset and backlight pins
 * live in the platform board profile, not here: the vendor LCD driver drives
 * them from the panel's own board config, and duplicating them in the TuyaOpen
 * board would let the two drift apart.
 *
 * The panel is fixed on this board, so the geometry is stated once rather than
 * probed. It matches the MIPI_480x800_ST7701S panel the platform profile
 * selects; a different panel would need both sides changed together. */
#define JIELI_DISPLAY_WIDTH  480
#define JIELI_DISPLAY_HEIGHT 800
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* AC792N_Develop_Board (AC7926A reference) K1 ADC ladder calibration. */
#define JIELI_K1_ADC_NUM     TUYA_ADC_NUM_0
#define JIELI_K1_ADC_CHANNEL 0U /* PD00 / ADC_IO_CH_PD00 */
/* SDK ADKEY V0 is midpoint(0, V1=192), so slot 0 covers raw values 0..96. */
#define JIELI_K1_PRESSED_MIN 0U
#define JIELI_K1_PRESSED_MAX 96U

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET ret = OPRT_OK;

#if defined(ENABLE_MEDIA) && (ENABLE_MEDIA == 1)
    TDD_AUDIO_T5AI_T audio_cfg = {
        .aec_enable = FALSE,
        .ai_chn = TKL_AI_0,
        .sample_rate = TKL_AUDIO_SAMPLE_16K,
        .data_bits = TKL_AUDIO_DATABITS_16,
        .channel = TKL_AUDIO_CHANNEL_MONO,
        .spk_sample_rate = TKL_AUDIO_SAMPLE_16K,
        .spk_pin = 0,
        .spk_pin_polarity = 0,
    };
    ret = tdd_audio_register((char *)AUDIO_CODEC_NAME, audio_cfg);
    if (ret != OPRT_OK) {
        return ret;
    }
#endif

#if defined(ENABLE_JIELI_ADKEY_BUTTON) && (ENABLE_JIELI_ADKEY_BUTTON == 1)
    BUTTON_ADC_CFG_T button_cfg = {
        .adc_num = JIELI_K1_ADC_NUM,
        .adc_ch = JIELI_K1_ADC_CHANNEL,
        .pressed_min = JIELI_K1_PRESSED_MIN,
        .pressed_max = JIELI_K1_PRESSED_MAX,
    };
    ret = tdd_adc_button_register((char *)BUTTON_NAME, &button_cfg);
    if (ret != OPRT_OK) {
        return ret;
    }
#endif

#if defined(ENABLE_MIPI_DSI) && (ENABLE_MIPI_DSI == 1)
    {
        /* The backlight is a plain GPIO on this board, driven by the vendor
         * panel config; the TDL brightness control is left to that path rather
         * than claimed here. */
        TDD_DISP_MIPI_CFG_T disp_cfg = {
            .width = JIELI_DISPLAY_WIDTH,
            .height = JIELI_DISPLAY_HEIGHT,
            .fmt = TUYA_PIXEL_FMT_RGB565,
            .rotation = TUYA_DISPLAY_ROTATION_0,
            .is_swap = false,
            .bl = { .type = TUYA_DISP_BL_TP_NONE },
            .power = { .pin = TUYA_GPIO_NUM_MAX, .active_level = TUYA_GPIO_LEVEL_LOW },
        };
        ret = tdd_disp_mipi_device_register((char *)DISPLAY_NAME, &disp_cfg);
        if (ret != OPRT_OK) {
            return ret;
        }
    }
#endif

    return ret;
}
