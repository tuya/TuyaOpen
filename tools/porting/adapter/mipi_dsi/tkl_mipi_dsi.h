/**
* @file tkl_mipi_dsi.h
* @brief Common process - MIPI-DSI display process
* @version 0.1
* @date 2026-10-09
*
* @copyright Copyright 2021-2026 Tuya Inc. All Rights Reserved.
*
* A MIPI-DSI panel has no GRAM: it carries a DPI video stream and is refreshed
* continuously from a host-side frame buffer, exactly like a parallel RGB panel.
* That is why this contract is shaped like tkl_rgb.h rather than like a
* command-bus one - the platform never issues per-pixel writes, it hands the
* controller a buffer address and lets it scan out.
*
* Keeping the frame buffer on the caller's side is the point of the shape: the
* buffer the application draws into is the buffer the controller reads, so there
* is no per-frame copy. A contract that made the platform own the buffer would
* cost a full-frame memcpy on every refresh.
*/
#ifndef __TKL_MIPI_DSI_H__
#define __TKL_MIPI_DSI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

typedef enum {
    MIPI_DSI_OUTPUT_FINISH = 0,
} TUYA_MIPI_DSI_EVENT_E;

typedef void (*TUYA_MIPI_DSI_ISR_CB)(TUYA_MIPI_DSI_EVENT_E event);

typedef struct {
    uint16_t                 width;      ///< panel width in pixels
    uint16_t                 height;     ///< panel height in pixels
    TUYA_DISPLAY_PIXEL_FMT_E fmt;        ///< frame buffer pixel format
    TUYA_DISPLAY_ROTATION_E  rotation;   ///< panel rotation
    bool                     is_swap;    ///< swap the two colour bytes
} TUYA_MIPI_DSI_BASE_CFG_T;

/**
 * @brief mipi dsi init
 *
 * The panel is fixed by the board profile, so the controller can only ever scan
 * that one geometry - it cannot be told a smaller one. width and height are
 * therefore validated against the panel the platform was built for and a
 * mismatch is refused rather than accepted, because accepting it would let the
 * caller size its frame buffer for a smaller area than the controller reads.
 *
 * @param[in] cfg: panel configuration
 *
 * @return OPRT_OK on success, OPRT_NOT_SUPPORTED for a geometry the panel does
 *         not have. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_mipi_dsi_init(TUYA_MIPI_DSI_BASE_CFG_T *cfg);

/**
 * @brief mipi dsi deinit
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_mipi_dsi_deinit(void);

/**
 * @brief register mipi dsi frame-end cb
 *
 * The callback fires when the controller has **retired** a buffer, which is what
 * makes the previously displayed one drawable again. It is a frame-sync signal,
 * not a data path: the caller decides what to do with it.
 *
 * It is only called for a frame that actually swapped a buffer. A frame the
 * controller scanned from the same buffer it already had retires nothing, so the
 * callback does not fire and the caller must not treat any buffer as reusable
 * from it. Callers that alternate two buffers can therefore rely on one
 * notification per swap.
 *
 * @param[in] cb: callback
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_mipi_dsi_irq_cb_register(TUYA_MIPI_DSI_ISR_CB cb);

/**
 * @brief ppi set
 *
 * Recorded and reported only - the panel timing is fixed by its init table, so
 * there is nothing to program. The value is still validated against the panel
 * for the same reason as in init().
 *
 * @param[in] width: ppi : width
 * @param[in] height: ppi : height
 *
 * @return OPRT_OK on success, OPRT_NOT_SUPPORTED for a geometry the panel does
 *         not have. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_mipi_dsi_ppi_set(uint16_t width, uint16_t height);

/**
 * @brief pixel mode set
 *
 * @param[in] mode: mode, such as 565 or 888
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_mipi_dsi_pixel_mode_set(TUYA_DISPLAY_PIXEL_FMT_E mode);

/**
 * @brief mipi dsi base addr set
 *
 * Hands the controller the buffer to scan out. The caller keeps ownership; the
 * buffer must stay valid and cache-clean until the next frame-end callback.
 *
 * @param[in] addr : base addr
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_mipi_dsi_base_addr_set(uint32_t addr);

/**
 * @brief mipi dsi transfer start
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_mipi_dsi_display_transfer_start(void);

/**
 * @brief mipi dsi transfer stop
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_mipi_dsi_display_transfer_stop(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
