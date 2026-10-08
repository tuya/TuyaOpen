/**
 * @file adkey_button_config.h
 * @brief AC792N_Develop_Board (AC7926A reference) K1 ADC ladder calibration.
 */

#ifndef _JIELI_AC792N_ADKEY_BUTTON_CONFIG_H_
#define _JIELI_AC792N_ADKEY_BUTTON_CONFIG_H_

#define JIELI_K1_ADC_NUM       TUYA_ADC_NUM_0
#define JIELI_K1_ADC_CHANNEL   0U /* PD00 / ADC_IO_CH_PD00 */
/* SDK ADKEY V0 is midpoint(0, V1=192), so slot 0 covers raw values 0..96. */
#define JIELI_K1_PRESSED_MIN   0U
#define JIELI_K1_PRESSED_MAX   96U

#endif /* _JIELI_AC792N_ADKEY_BUTTON_CONFIG_H_ */
