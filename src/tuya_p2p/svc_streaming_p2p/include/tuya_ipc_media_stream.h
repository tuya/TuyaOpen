#ifndef _TUYA_IPC_MEDIA_STREAM_H_
#define _TUYA_IPC_MEDIA_STREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tuya_ipc_media_adapter.h"
#include "tuya_ipc_media_stream_event.h"
#include "tuya_ipc_p2p.h"
//#include "tuya_imm_service_log.h"

/** @struct MEDIA_STREAM_VAR_T
 * @brief media stream parameter
 */
typedef struct {
    MEDIA_STREAM_EVENT_CB on_event_cb;     /** p2p event callback function */
    int max_client_num;                  /** max client number supported in p2p and webrtc streaming */
    TRANS_DEFAULT_QUALITY_E def_live_mode; /** for multi-streaming ipc, the default quality for live preview */
    BOOL_T low_power;                      /** whether is lowpower device */
    uint32_t recv_buffer_size;               /*recv app data size. if recv_buffer_size = 0,default = 16*1024*/
} MEDIA_STREAM_VAR_T;

/** @brief media stream module init
 * @param[in] stream_var initialize parameter
 * @return error code
 * - OPRT_OK success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_stream_init(MEDIA_STREAM_VAR_T *stream_var);

/** @brief get number of clients currently streaming
 * @return number of clients
 */
int tuya_ipc_get_client_online_num();

/** @brief pause media streaming
 * @return error code
 * - OPRT_OK success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_service_pause();

/** @brief resume media streaming
 * @return error code
 * - OPRT_OK success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_service_resume();

/** @brief uninitialize stream module
 * @return error code
 * - OPRT_OK success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_stream_deinit();

/**
 * @brief send playback video frame to APP via P2P channel
 *
 * @param[in] client:client cliend id
 * @param[in] p_video_frame:p_video_frame
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ipc_media_playback_send_video_frame(const uint32_t client,
                                                     const MEDIA_VIDEO_FRAME_T *p_video_frame);
OPERATE_RET
tuya_ipc_media_playback_send_video_frame_with_encrypt(const uint32_t client, uint32_t reqId,
                                                      const TRANSFER_MEDIA_FRAME_WIHT_ENCRYPT_T *p_video_frame);

/**
 * @brief send playback audio frame to APP via P2P channel
 *
 * @param[in] client:client cliend id
 * @param[in] p_audio_frame:p_audio_frame
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ipc_media_playback_send_audio_frame(const uint32_t client,
                                                     const MEDIA_AUDIO_FRAME_T *p_audio_frame);
OPERATE_RET
tuya_ipc_media_playback_send_audio_frame_with_encrypt(const uint32_t client, uint32_t reqId,
                                                      const TRANSFER_MEDIA_FRAME_WIHT_ENCRYPT_T *p_audio_frame);

/**
 * @brief notify client(APP) playback fragment is finished, send frag info to app
 *
 * @param[in] client:client cliend id
 * @param[in] fgmt:playback time
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ipc_media_playback_send_fragment_end(const uint32_t client, const PLAYBACK_TIME_S *fgmt);

/**
 * @brief notify client(APP) playback data is finished, no more data outgoing
 *
 * @param[in] client:client cliend id
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ipc_media_playback_send_finish(const uint32_t client);

/**
 * @brief Clear P2P AV send buffers (VDATA/ADATA) for current session
 * @return none
 * @note Call on real PB (re)start; skip when App re-sends same-seg START without mid seek
 */
void tuya_ipc_media_p2p_clear_send(void);

/**
 * @brief Drop unsent AV bytes without rebuilding KCP (safe mid-stream, e.g. PB seek)
 */
void tuya_ipc_media_p2p_drop_unsent(void);

/**
 * @brief Notify App that playback video is ready (TY_CMD_IO_CTRL_VIDEO_SEND_START=50).
 * @note Call after the first I-frame of a START/switch has gone on the wire.
 *       Do not send COMMAND_SUCCESS on START — App treats that as playback finished.
 */
void tuya_ipc_media_p2p_video_send_start(void);

/**
 * @brief put log to tuya cloud service.
 *
 * @param level
 * @param log
 * @param log_len
 * @return void
 */
// void tuya_imm_media_online_log_print(IMM_SERVICE_LOG_LV_T level, char *p, char* pFmt, ...);

#ifdef __cplusplus
}
#endif

#endif /*__TUYA_IPC_MEDIA_STREAM_H__*/
