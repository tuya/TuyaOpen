/**
 * @file adkey_button_config.h
 * @brief AC79_DevKitBoard (AC791) K1 ADC ladder calibration.
 */

#ifndef _JIELI_AC79_ADKEY_BUTTON_CONFIG_H_
#define _JIELI_AC79_ADKEY_BUTTON_CONFIG_H_

#define JIELI_K1_ADC_NUM       TUYA_ADC_NUM_0
#define JIELI_K1_ADC_CHANNEL   3U /* PB1 / AD_CH_PB01 */
/* SDK ADKEY V6..V7 are 823..1007; the press range is (V6, V7]. */
#define JIELI_K1_PRESSED_MIN   824U
#define JIELI_K1_PRESSED_MAX   1007U

#endif /* _JIELI_AC79_ADKEY_BUTTON_CONFIG_H_ */
