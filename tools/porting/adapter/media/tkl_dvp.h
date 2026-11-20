/**
 * @file tkl_dvp.h
 * @brief Tuya DVP driver adaptation layer (TKL) interface
 *
 * One interface for every platform: the TDD registers a frame source through
 * the callbacks in TUYA_DVP_CFG_T.inter_cfg, and the TKL hands finished frames
 * back through post_cb. There is no pull call: how the TKL obtains its own
 * buffers is its business.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TKL_DVP_H__
#define __TKL_DVP_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the DVP input
 * @param[in] dvp_cfg input geometry, output mode and the frame callbacks
 * @return OPRT_OK; OPRT_INVALID_PARM on a bad config (zero geometry, odd
 *         width, unsupported output_mode / sync_polarity);
 *         OPRT_NOT_SUPPORTED for an output mode this platform lacks;
 *         OPRT_INIT_MORE_THAN_ONCE without a deinit; OPRT_COM_ERROR otherwise
 * @note Unsupported combinations are rejected, never silently ignored.
 */
OPERATE_RET tkl_dvp_init(TUYA_DVP_CFG_T *dvp_cfg);

/**
 * @brief Stop streaming and release every camera resource
 * @return OPRT_OK / OPRT_COM_ERROR (not initialized)
 * @note Memory must stay level across repeated init/deinit cycles.
 */
OPERATE_RET tkl_dvp_deinit(void);

/**
 * @brief Start the input; frames arrive through post_cb from here on
 * @return OPRT_OK / OPRT_COM_ERROR (not initialized)
 */
OPERATE_RET tkl_dvp_start(void);

/**
 * @brief Stop streaming, keep the initialized resources; idempotent
 * @return OPRT_OK / OPRT_COM_ERROR (not initialized)
 */
OPERATE_RET tkl_dvp_stop(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __TKL_DVP_H__ */
