#ifndef AC79_DEVKITBOARD_AUDIO_CONFIG_H
#define AC79_DEVKITBOARD_AUDIO_CONFIG_H

/* AC79_DevKitBoard V1.2: MIC1 is differential on the board schematic. */
#define JIELI_AUDIO_MIC_CHANNEL LADC_CH_MIC1_P_N
#define JIELI_AUDIO_MIC_CHANNEL_COUNT 1
/* No reliable MIC bias setting is specified by the schematic or ADC sample. */
#define JIELI_AUDIO_MIC_BIAS_NOTE "vendor default; no explicit override"

/* DAC drives the external 8002 PA; keep the vendor-confirmed routing. */
#define JIELI_AUDIO_DAC_HW_CHANNEL 0x05
#define JIELI_AUDIO_DAC_DIFFER_OUTPUT 0
#define JIELI_AUDIO_DAC_CHANNEL_COUNT 4
#define JIELI_AUDIO_DAC_VCM_INIT_DELAY_MS 1000

/* PB2 is active-low mute. Hold muted while the DAC VCM settles. */
#define JIELI_AUDIO_PA_MUTE_PORT IO_PORTB_02
#define JIELI_AUDIO_PA_MUTE_LEVEL 0
#define JIELI_AUDIO_PA_RELEASE_DELAY_MS 200

#endif /* AC79_DEVKITBOARD_AUDIO_CONFIG_H */
