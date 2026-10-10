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

    return ret;
}
