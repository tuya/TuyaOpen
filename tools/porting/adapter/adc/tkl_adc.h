/**
 * @file tkl_adc.h
 * @brief Common process - adapter the adc api
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2021-2022 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_ADC_H__
#define __TKL_ADC_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief tuya kernel adc init
 *
 * @param[in] port_num: adc port number
 * @param[in] cfg: adc config
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_adc_init(TUYA_ADC_NUM_E port_num, TUYA_ADC_BASE_CFG_T *cfg);

/**
 * @brief adc deinit
 *
 * @param[in] port_num: adc port number

 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_adc_deinit(TUYA_ADC_NUM_E port_num);

/**
 * @brief get adc width
 *
 * @param[in] port_num: adc port number

 *
 * @return adc width
 */
uint8_t tkl_adc_width_get(TUYA_ADC_NUM_E port_num);

/**
 * @brief get adc reference voltage
 *
 * @param[in] port_num

 *
 * @return adc reference voltage(bat: mv)
 */
uint32_t tkl_adc_ref_voltage_get(TUYA_ADC_NUM_E port_num);

/**
 * @brief adc get temperature
 *
 * @return temperature(bat: 'C)
 */
int32_t tkl_adc_temperature_get(void);

/**
 * @brief adc read
 *
 * @param[in] port_num: adc port number
 * @param[out] buff: points to the list of data read from the ADC register
 * @param[in] len:  buff len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_adc_read_data(TUYA_ADC_NUM_E port_num, int32_t *buff, uint16_t len);

/**
 * @brief read single channel
 *
 * @param[in] port_num: adc port number
 * @param[in] ch_id: channel id in one adc unit
 * @param[out] buff: convert result buffer
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 *
 */
OPERATE_RET tkl_adc_read_single_channel(TUYA_ADC_NUM_E port_num, uint8_t ch_id, int32_t *data);

/**
 * @brief read voltage
 *
 * @param[in] port_num: adc port number
 * @param[out] buff: points to the list of calculate voltage,bat : mv
 * @param[in] len:  buff len
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 *
 */
OPERATE_RET tkl_adc_read_voltage(TUYA_ADC_NUM_E port_num, int32_t *buff, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
