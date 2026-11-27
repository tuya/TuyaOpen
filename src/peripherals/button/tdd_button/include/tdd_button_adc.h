/**
 * @file tdd_button_adc.h
 * @brief Generic ADC / ADKEY ladder button device driver interface.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef _TDD_BUTTON_ADC_H_
#define _TDD_BUTTON_ADC_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    TUYA_ADC_NUM_E adc_num;
    uint8_t        adc_ch;
    uint16_t       pressed_min;
    uint16_t       pressed_max;
    uint8_t        adc_width;  /* ADC resolution in bits (8..16); 0 defaults to 10. */
    uint16_t       ref_vol;    /* Reference voltage in mV; 0 defaults to 3300. */
    uint8_t        sample_cnt; /* Median-filter sample count per poll (1..7); 0 defaults to 3. */
} BUTTON_ADC_CFG_T;

typedef struct {
    const char *name;
    uint16_t    pressed_min;
    uint16_t    pressed_max;
} BUTTON_ADC_KEY_T;

/**
 * @brief Register a button whose pressed state is represented by an ADC range.
 *
 * The ADC range endpoints are inclusive and use the raw sample domain selected
 * by the platform TKL ADC implementation. Multiple buttons may share the same
 * ADC port/channel (e.g. an ADKEY resistor ladder).
 */
OPERATE_RET tdd_adc_button_register(char *name, BUTTON_ADC_CFG_T *adc_cfg);

/**
 * @brief Register an array of ADKEY ladder buttons on a shared ADC channel.
 */
OPERATE_RET tdd_adc_button_register_keys(TUYA_ADC_NUM_E adc_num, uint8_t adc_ch, uint8_t adc_width, uint16_t ref_vol,
                                         uint8_t sample_cnt, const BUTTON_ADC_KEY_T *keys, uint8_t key_count);

#ifdef __cplusplus
}
#endif

#endif /* _TDD_BUTTON_ADC_H_ */
