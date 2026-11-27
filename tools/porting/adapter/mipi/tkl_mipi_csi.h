/**
 * @file tkl_mipi_csi.h
 * @brief TuyaOS Kernel Layer (TKL) - MIPI CSI camera abstraction interface
 * @version 0.1
 * @date 2026-04-09
 *
 * @copyright Copyright 2021-2026 Tuya Inc. All Rights Reserved.
 *
 * This header defines the MIPI CSI camera TKL interface that every platform
 * adaptor must implement. A new platform porter provides only a
 * tkl_mipi_csi.c that satisfies the contracts below; no changes to upper
 * layers are required.
 *
 * ============================================================
 * Porting guide for a new platform
 * ============================================================
 *
 * 1. Output channels
 *    A camera exposes logical output channels addressed by
 *    TUYA_CAMERA_FRAME_CHANNEL_E. What each channel carries is part of the
 *    interface contract; which peripheral implements it is not:
 *
 *      ch0  full-size frames from the main path       pull or push
 *      ch1  full frames from the secondary path       pull or push
 *      ch2  display-bound frames                      push only, cb required
 *      ch3  encoded H.264 bitstream                   push only, cb required
 *
 *    ch2 and ch3 are fed by the same main path. Changing the main-path
 *    geometry (set_pp on ch3) therefore briefly interrupts ch2 as well.
 *
 *    The pipeline shape below is the reference topology. The peripheral
 *    names are descriptive, not contractual - a port maps them onto its own
 *    hardware:
 *
 *      sensor -> ISP
 *        |
 *        +-- secondary path (SP) ----> ch1   complete frames    get_frame / cb
 *        |
 *        \-- main path (MP, NV12) ---> ch0   complete frames    get_frame / cb
 *            |
 *            +-- display pipeline ---> ch2   display frames     cb required
 *            |
 *            \-- H.264 encoder ------> ch3   H.264 bitstream    cb required
 *
 * 2. Channel availability
 *    This version accepts channels 0..3. The main-path tap (ch0) is not
 *    available yet: get_frame(ch0), set_pp(ch0, ...) and cancel/resume of ch0
 *    are rejected. Do not build on ch0 until it is enabled.
 *
 * 3. Buffer ownership
 *    - get_frame(): the adaptor allocates the returned TUYA_MEDIA_BUF_T. The
 *      caller releases both frame and struct with buf->free_cb(buf), and must
 *      never call free() on either.
 *    - Push channels: the buffer handed to cb is valid only for the duration
 *      of the call. To keep it, the callee takes ownership and releases it
 *      later through buf->free_cb.
 *    - init() deep-copies the outputs array, so the caller may release its
 *      cfg as soon as init() returns.
 *
 * 4. Callbacks and threading
 *    event_cb and every TUYA_MEDIA_BUF_CB run on a vendor or delivery thread.
 *    They must not block, allocate memory, print, or re-enter this module.
 *    Channel identity travels in the event value (TUYA_CAMERA_EVENT_E), so
 *    the event callback takes no channel argument.
 *
 * 5. Unsupported behaviour
 *    A channel or feature a port does not implement must be rejected with
 *    OPRT_NOT_SUPPORTED rather than silently ignored. ch2 and ch3 share one
 *    display / encode peripheral: when the other peripheral already holds it,
 *    resume fails with OPRT_RESOURCE_NOT_READY and the caller is expected to
 *    retry.
 */

#ifndef __TKL_MIPI_CSI_H__
#define __TKL_MIPI_CSI_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif



/** Sensor description and requested outputs.
 *
 *  @note @ref outputs is a caller-allocated array of TUYA_MEDIA_OUTPUT_CFG_T,
 *        one entry per wanted channel. init() deep-copies it, so the caller
 *        may release the cfg after init() returns.
 *  @note @ref event_cb reports lifecycle and error events
 *        (TUYA_CAMERA_EVENT_E); @ref event_arg is passed back to it unchanged.
 *  @note A field a channel does not honor must be 0 - init() rejects the
 *        entry otherwise. See TUYA_MEDIA_OUTPUT_CFG_T for the per-channel
 *        rules.
 */
typedef struct {
    TUYA_CAMERA_FRAME_CFG_T   sensor_cfg;

    UINT8_T                   output_num;
    TUYA_MEDIA_OUTPUT_CFG_T  *outputs;
    TUYA_CAMERA_EVENT_CB      event_cb;
    VOID_T                   *event_arg;
} TUYA_MIPI_CSI_BASE_CFG_T;

/**
 * @brief Initialize the sensor and pipelines.
 *
 * No output is produced until tkl_mipi_csi_start() is called.
 *
 * @param[in] cfg sensor description and requested output channels; must not
 *                be NULL
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_init(CONST TUYA_MIPI_CSI_BASE_CFG_T *cfg);

/**
 * @brief Start all enabled outputs.
 *
 * Idempotent after init().
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_start(VOID_T);

/**
 * @brief Stop streaming while keeping the sensor initialized.
 *
 * Idempotent. Use tkl_mipi_csi_deinit() to also release the sensor.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_stop(VOID_T);

/**
 * @brief Stop, power down, and release all camera resources.
 *
 * Cancels every channel.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_deinit(VOID_T);

/**
 * @brief Change one output channel's resolution while it is running.
 *
 * Per-channel behaviour:
 *  - ch1: switches live.
 *  - ch2: rebuilds the display pipeline briefly.
 *  - ch3: rebuilds the main path. This is the authoritative main-path
 *    geometry, and since one main path feeds both ch2 and ch3, ch2 is
 *    briefly interrupted too.
 *  - ch0: rejected - the main-path tap is not available in this version.
 *
 * While STOPPED the value is recorded and applied at the next start().
 *
 * @param[in] channel output channel to reconfigure
 * @param[in] width   target width in pixels
 * @param[in] height  target height in pixels
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_set_pp(TUYA_CAMERA_FRAME_CHANNEL_E channel, UINT16_T width, UINT16_T height);

/**
 * @brief Adjust the encoder bitrate.
 *
 * Requires ch3 configured and streaming.
 *
 * @param[in] bitrate target bitrate in bps
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_set_bitrate(UINT32_T bitrate);

/**
 * @brief Set the I-frame interval for ch3.
 *
 * The encoder accepts a new GOP only at build time, so a running stream
 * refuses the call: invoke it while STOPPED - the value is recorded and
 * applied at the next start(). gop 0 restores the encoder default.
 *
 * @param[in] gop I-frame interval; 0 = encoder default
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_set_gop(UINT32_T gop);

/**
 * @brief Force an IDR frame.
 *
 * Requires ch3 running.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_force_idr(VOID_T);

/**
 * @brief Submit or clear one business-composed overlay bitmap.
 *
 * ch2 addresses the display path, ch3 the encoder path; ch0/ch1 carry no
 * overlay and are rejected. See TUYA_OSD_T for the bitmap formats each
 * backend accepts.
 *
 * @param[in] channel ch2 or ch3
 * @param[in] osd     overlay description; a NULL @ref TUYA_OSD_T.data clears
 *                    the position
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_set_osd(TUYA_CAMERA_FRAME_CHANNEL_E channel,
                                 CONST TUYA_OSD_T *osd);

/**
 * @brief Pull one complete frame from a pull channel.
 *
 * ch1 is the pull channel in this version. ch0 stays rejected until the
 * main-path tap is available, and the push channels ch2/ch3 never park
 * frames. A channel configured with cb is push-only and rejects get_frame().
 * The adaptor allocates the returned buffer; release both frame and struct
 * via buf->free_cb(buf).
 *
 * @param[in]  channel    pull channel to read from
 * @param[out] out        receives the frame; must not be NULL
 * @param[in]  timeout_ms wait budget in milliseconds; 0 = do not wait
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_get_frame(TUYA_CAMERA_FRAME_CHANNEL_E channel, TUYA_MEDIA_BUF_T **out, UINT32_T timeout_ms);

/**
 * @brief Resume one configured channel's output after cancel_channel().
 *
 * Rebuilds from the init-time configuration. Idempotent when the channel is
 * already running. ch2/ch3 re-claim the shared display / encode peripheral
 * and fail with OPRT_RESOURCE_NOT_READY when the other one holds it.
 *
 * @param[in] channel channel to resume
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_resume_channel(TUYA_CAMERA_FRAME_CHANNEL_E channel);

/**
 * @brief Cancel one configured channel's output while the camera keeps
 *        running.
 *
 * Per-channel behaviour:
 *  - ch1: the secondary path closes, get_frame() returns
 *    OPRT_RESOURCE_NOT_READY.
 *  - ch2: the display pipeline is torn down and ch3 keeps streaming.
 *  - ch3: the encoder is torn down and ch2 keeps displaying.
 *  - ch0: rejected.
 *
 * Idempotent. deinit() / stop() cancel everything anyway.
 *
 * @param[in] channel channel to cancel
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_cancel_channel(TUYA_CAMERA_FRAME_CHANNEL_E channel);

/**
 * @brief Set sensor horizontal mirror and vertical flip at runtime.
 *
 * @param[in] hmirror TRUE = mirror horizontally
 * @param[in] vflip   TRUE = flip vertically
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_set_mirror_flip(BOOL_T hmirror, BOOL_T vflip);

/**
 * @brief Get the current runtime mirror / flip state.
 *
 * Either output may be NULL when that value is not wanted.
 *
 * @param[out] hmirror horizontal mirror state; may be NULL
 * @param[out] vflip   vertical flip state; may be NULL
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_get_mirror_flip(BOOL_T *hmirror, BOOL_T *vflip);

/**
 * @brief Get the ISP exposure-normalized luminance.
 *
 * @param[out] luminance receives the luminance value; must not be NULL
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_get_exposure_luminance(UINT32_T *luminance);

/**
 * @brief Switch the ISP output between gray and color.
 *
 * Implemented through chroma saturation on the ISP.
 *
 * @param[in] enable TRUE = gray, FALSE = color
 *
 * @return OPRT_OK on success. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET tkl_mipi_csi_set_color_to_gray(BOOL_T enable);

#ifdef __cplusplus
}
#endif

#endif /* __TKL_MIPI_CSI_H__ */
