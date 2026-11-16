/**
 * @file tkl_dvp.h
 * @brief Common process - camera dvp process
 * @version 0.1
 * @date 2025-06-09
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_DVP_H__
#define __TKL_DVP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/**
 * @brief tkl_dvp_init
 *
 * @param[in] dvp_cfg: camera device cfg
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_dvp_init(TUYA_DVP_CFG_T *dvp_cfg);

/**
 * @brief tkl_dvp_deinit
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_dvp_deinit(void);

/**
 * @brief tkl_dvp_start
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_dvp_start(void);

/**
 * @brief tkl_dvp_stop
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_dvp_stop(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TKL_DVP_H__ */
