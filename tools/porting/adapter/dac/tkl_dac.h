/**
 * @file tkl_adc.h
 * @brief Common process - adapter the adc api
 * @version 0.1
 * @date 2022-03-24
 *
 * @copyright Copyright 2021-2022 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_DAC_H__
#define __TKL_DAC_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief tuya hal dac init
 *
 * @param[in] port_num: dac port number
 * @param[in] cfg: dac config
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_dac_init(TUYA_DAC_NUM_E port_num);

/**
 * @brief dac deinit
 *
 * @param[in] port_num: dac port number
 * @param[in] ch_num: dac channel number
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_dac_deinit(TUYA_DAC_NUM_E port_num);

/**
 * @brief dac controller config
 *
 * @param[in] port_num: dac port number
 * @param[in] cmd: config cmd
 * @param[in] argu: config parmaer
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_dac_controller_config(TUYA_DAC_NUM_E port_num, TUYA_DAC_CMD_E cmd, void *argu);

/**
 * @brief get dac base cfg
 *
 * @param[in] port_num: dac port number
 * @param[in] cfg: dac base config struct
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_dac_base_cfg_get(TUYA_DAC_NUM_E port_num, TUYA_DAC_BASE_CFG_T *cfg);

/**
 * @brief dac start to convert
 *
 * @param[in] port_num: dac port number
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_dac_start(TUYA_DAC_NUM_E port_num);

/**
 * @brief dac stop to convert
 *
 * @param[in] port_num: dac port number
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_dac_stop(TUYA_DAC_NUM_E port_num);

/**
 * @brief dac's fifi reset
 *
 * @param[in] port_num: dac port number
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_dac_fifo_reset(TUYA_DAC_NUM_E port_num);

#ifdef __cplusplus
}
#endif

#endif
