/* host stub: declarations only; mocks are defined in the test */
#ifndef __STUB_TKL_ADC_H__
#define __STUB_TKL_ADC_H__
#include "tuya_cloud_types.h"
OPERATE_RET tkl_adc_init(TUYA_ADC_NUM_E port_num, TUYA_ADC_BASE_CFG_T *cfg);
OPERATE_RET tkl_adc_deinit(TUYA_ADC_NUM_E port_num);
OPERATE_RET tkl_adc_read_single_channel(TUYA_ADC_NUM_E port_num, uint8_t ch_id, int32_t *data);
#endif
