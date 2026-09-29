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
/* Temporary bring-up isolation: test the SDK's default MIC1 input (ADC1,
 * PC11/PC12) on its own before enabling both onboard microphones. */
#define JIELI_AUDIO_MIC_CHANNEL_MAP AUDIO_ADC_MIC_1
#define JIELI_AUDIO_MIC_BIAS_SELECTION (AUDIO_MIC_BIAS_CH0 | AUDIO_MIC_BIAS_CH1)
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
#undef TCFG_AUDIO_DAC_PA_MUTE_EN
#define TCFG_AUDIO_DAC_PA_MUTE_EN 0
#undef TCFG_AUDIO_DAC_PA_MUTE_PORT
#define TCFG_AUDIO_DAC_PA_MUTE_PORT IO_PORTE_15
#undef TCFG_AUDIO_DAC_PA_MUTE_LEVEL
#define TCFG_AUDIO_DAC_PA_MUTE_LEVEL 0
#undef TCFG_AUDIO_DAC_PA_MUTE_DELAY_MS
#define TCFG_AUDIO_DAC_PA_MUTE_DELAY_MS JIELI_AUDIO_PA_RELEASE_DELAY_MS

/* PE15 is active-low mute; the external PA is released after 300 ms. */
#define JIELI_AUDIO_PA_MUTE_PORT IO_PORTE_15
#define JIELI_AUDIO_PA_MUTE_LEVEL 0
#define JIELI_AUDIO_PA_RELEASE_DELAY_MS 300

#endif /* AC792N_DEVELOP_BOARD_AUDIO_CONFIG_H */
