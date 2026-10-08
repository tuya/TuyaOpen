/**
 * @file tdd_button_adc.h
 * @brief ADC ladder button device driver interface.
 */

#ifndef _TDD_BUTTON_ADC_H_
#define _TDD_BUTTON_ADC_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    TUYA_ADC_NUM_E adc_num;
    uint8_t adc_ch;
    uint16_t pressed_min;
    uint16_t pressed_max;
} BUTTON_ADC_CFG_T;

/**
 * @brief Register a button whose pressed state is represented by an ADC range.
 *
 * The ADC range endpoints are inclusive and use the raw sample domain selected
 * by the platform TKL ADC implementation.
 */
OPERATE_RET tdd_adc_button_register(char *name, BUTTON_ADC_CFG_T *adc_cfg);

#ifdef __cplusplus
}
#endif

#endif /* _TDD_BUTTON_ADC_H_ */
