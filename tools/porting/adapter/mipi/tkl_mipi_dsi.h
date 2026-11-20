/**
 * @file tkl_mipi_dsi.h
 * @brief Common process - adapter the MIPI DSI/DPU api
 * @version 0.1
 * @date 2026-04-09
 *
 * @copyright Copyright 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_MIPI_H__
#define __TKL_MIPI_H__

#include "tuya_cloud_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    UINT8_T *buffer;
    /** Release function for @ref buffer; travels with the frame, so frames from
     *  different sources may be interleaved on the same panel. */
    void (*free_cb)(void *frame_buff);
} tkl_mipi_dsi_flush_frame_t;

OPERATE_RET tkl_mipi_dsi_set_input_mode(BOOL_T decompress);
OPERATE_RET tkl_mipi_dsi_init(CONST TUYA_MIPI_BASE_CFG_T *base_cfg);
OPERATE_RET tkl_mipi_dsi_deinit(VOID_T);
OPERATE_RET tkl_mipi_dsi_tx_cmd(UINT8_T cmd, CONST VOID_T *param, UINT32_T size);

/**
 * @brief Queue one frame for display.
 *
 * @param[in] frame buffer plus its release function
 *
 * @note **Ownership**: when @p frame->free_cb is non-NULL the buffer is handed
 *       over to this call and the caller must not free it, on success *or* on
 *       failure - a failed flush releases it internally. The only exceptions
 *       are OPRT_INVALID_PARM and OPRT_RESOURCE_NOT_READY, which mean the frame
 *       was never accepted and the caller still owns it.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_mipi_dsi_flush(tkl_mipi_dsi_flush_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif
