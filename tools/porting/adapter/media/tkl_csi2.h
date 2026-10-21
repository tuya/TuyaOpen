/**
 * @file tkl_csi2.h
 * @brief Common process - camera MIPI CSI2 process
 * @version 0.1
 * @date 2026-10-20
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_CSI2_H__
#define __TKL_CSI2_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/**
 * @brief tkl_csi2_init
 *
 * @param[in] csi2_cfg: camera device cfg
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_csi2_init(TUYA_DVP_CFG_T *csi2_cfg);

/**
 * @brief tkl_csi2_deinit
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_csi2_deinit(void);

/**
 * @brief tkl_csi2_start
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_csi2_start(void);

/**
 * @brief tkl_csi2_stop
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_csi2_stop(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TKL_CSI2_H__ */
