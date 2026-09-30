#ifndef AC792N_DEVELOP_BOARD_AUDIO_CONFIG_H
#define AC792N_DEVELOP_BOARD_AUDIO_CONFIG_H

/*
 * AC792N_Develop_Board V1.21 uses both onboard differential microphones:
 * MIC1 is ADC0 on PC7/PC6 with MICBIASA (PC9), and MIC2 is ADC1 on
 * PC11/PC12 with MICBIASB (PC10). The 16 kHz Tuya audio interface is mono,
 * so the WL83 adapter downmixes the two interleaved ADC channels.
 */
#define JIELI_AUDIO_MIC_PORTS {IO_PORTC_07, IO_PORTC_06, IO_PORTC_11, IO_PORTC_12}
#define JIELI_AUDIO_MIC_CHANNEL_COUNT 2
/* Single ADC channel: opening both onboard microphones trips the vendor ADC's
 * channel check ("audio_adc_mic_open ch err", audio_adc.c:631) and reboots the
 * board. ADC0 is PC7/PC6 with MICBIASA (PC9); ADC1 is PC11/PC12 with MICBIASB
 * (PC10). A capture probe over both showed ADC1 responding far more than ADC0
 * (peaks ~540 vs ~25 of 32767), so ADC1 is the input to keep. */
#define JIELI_AUDIO_MIC_CHANNEL_MAP AUDIO_ADC_MIC_1
/* No JIELI_AUDIO_MIC_BIAS_SELECTION here: struct adc_platform_data
 * (include_lib/driver/cpu/wl83/asm/ladc.h) has no bias field, so nothing reads
 * such a macro. Per-channel bias comes from TCFG_ADCx_BIAS_SEL below. */
/* WL83 recommends opening every configured ADC channel for multi-mic use. */
#define JIELI_AUDIO_ADC_ALL_CHANNEL_OPEN 1

/* Enable ADC0/ADC1 for the board's onboard MIC1/MIC2 differential inputs. */
#undef TCFG_AUDIO_ADC_ENABLE
#define TCFG_AUDIO_ADC_ENABLE 1
#undef TCFG_ADC0_ENABLE
#define TCFG_ADC0_ENABLE 1
#undef TCFG_ADC0_MODE
#define TCFG_ADC0_MODE 1
#undef TCFG_ADC0_AIN_SEL
#define TCFG_ADC0_AIN_SEL 1
#undef TCFG_ADC0_BIAS_SEL
#define TCFG_ADC0_BIAS_SEL 1
#undef TCFG_ADC0_INSIDE_BIAS_RESISTOR_ENABLE
#define TCFG_ADC0_INSIDE_BIAS_RESISTOR_ENABLE 0
#undef TCFG_ADC0_BIAS_RSEL
#define TCFG_ADC0_BIAS_RSEL 3
#undef TCFG_ADC0_DCC_LEVEL
#define TCFG_ADC0_DCC_LEVEL 14
#undef TCFG_ADC0_POWER_IO
#define TCFG_ADC0_POWER_IO 0
#undef TCFG_ADC1_ENABLE
#define TCFG_ADC1_ENABLE 1
#undef TCFG_ADC1_MODE
#define TCFG_ADC1_MODE 1
#undef TCFG_ADC1_AIN_SEL
#define TCFG_ADC1_AIN_SEL 1
#undef TCFG_ADC1_BIAS_SEL
#define TCFG_ADC1_BIAS_SEL 2
#undef TCFG_ADC1_INSIDE_BIAS_RESISTOR_ENABLE
#define TCFG_ADC1_INSIDE_BIAS_RESISTOR_ENABLE 0
#undef TCFG_ADC1_BIAS_RSEL
#define TCFG_ADC1_BIAS_RSEL 3
#undef TCFG_ADC1_DCC_LEVEL
#define TCFG_ADC1_DCC_LEVEL 14
#undef TCFG_ADC1_POWER_IO
#define TCFG_ADC1_POWER_IO 0

/* Mono differential DAC (DACL-DACR) feeds the onboard LTK5313 SPK amplifier. */
#define JIELI_AUDIO_DAC_HW_CHANNEL AUDIO_DAC_CH_L
#define JIELI_AUDIO_DAC_DIFFER_OUTPUT 0
#define JIELI_AUDIO_DAC_CHANNEL_COUNT 1
/* VCM capacitor enable is inferred from the WL83 wifi_soundbox SDK sample;
 * the board schematic does not confirm this setting. Verify on hardware. */
#define JIELI_AUDIO_DAC_VCM_CAP_ENABLE 1

#undef TCFG_AUDIO_DAC_CONNECT_MODE
#define TCFG_AUDIO_DAC_CONNECT_MODE DAC_OUTPUT_MONO_L
#undef TCFG_AUDIO_DAC_MODE
#define TCFG_AUDIO_DAC_MODE DAC_MODE_DIFF
#undef TCFG_AUDIO_VCM_CAP_EN
#define TCFG_AUDIO_VCM_CAP_EN 1
/* PE15 drives the LTK5313 EN pin through an inverting transistor stage
 * (schematic PA block: PE15 -> MUTE net -> pull-down -> EN). LEVEL is the
 * *mute* value, not the release value: the SDK drives LEVEL at init and on DAC
 * close, and !LEVEL to release the amp. So LEVEL=0 means PE15 low while muted
 * and PE15 high while playing.
 *
 * This is the wifi_soundbox reference convention (IO_PORTE_15 + 0, confirmed in
 * its board sdk_config.h), and it is the value this board was verified with on
 * hardware. Do not "correct" it to 1: that inverts the sequence and leaves the
 * amplifier muted after init. */
#undef TCFG_AUDIO_DAC_PA_MUTE_EN
#define TCFG_AUDIO_DAC_PA_MUTE_EN 0
#undef TCFG_AUDIO_DAC_PA_MUTE_PORT
#define TCFG_AUDIO_DAC_PA_MUTE_PORT IO_PORTE_15
#undef TCFG_AUDIO_DAC_PA_MUTE_LEVEL
#define TCFG_AUDIO_DAC_PA_MUTE_LEVEL 0
#undef TCFG_AUDIO_DAC_PA_MUTE_DELAY_MS
#define TCFG_AUDIO_DAC_PA_MUTE_DELAY_MS 300

/* Consumed by the staged board.c PA bring-up sequence (jieli_build.py):
 * board_early_init drives MUTE_LEVEL, board_init waits
 * JIELI_AUDIO_PA_RELEASE_DELAY_MS, then drives !MUTE_LEVEL to release the amp.
 * Same convention as TCFG_AUDIO_DAC_PA_MUTE_LEVEL above: 0 is the mute value,
 * so PE15 ends up high once the amp is released. */
#define JIELI_AUDIO_PA_MUTE_PORT IO_PORTE_15
#define JIELI_AUDIO_PA_MUTE_LEVEL 0
#define JIELI_AUDIO_PA_RELEASE_DELAY_MS 300

#endif /* AC792N_DEVELOP_BOARD_AUDIO_CONFIG_H */
