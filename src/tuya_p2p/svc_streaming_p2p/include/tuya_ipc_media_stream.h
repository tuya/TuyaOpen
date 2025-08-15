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
    INT_T max_client_num;                  /** max client number supported in p2p and webrtc streaming */
    TRANS_DEFAULT_QUALITY_E def_live_mode; /** for multi-streaming ipc, the default quality for live preview */
    BOOL_T low_power;                      /** whether is lowpower device */
    UINT_T recv_buffer_size;               /*recv app data size. if recv_buffer_size = 0,default = 16*1024*/
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
INT_T tuya_ipc_get_client_online_num();

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
OPERATE_RET tuya_ipc_media_playback_send_video_frame(IN CONST UINT_T client,
                                                     IN CONST MEDIA_VIDEO_FRAME_T *p_video_frame);
OPERATE_RET
tuya_ipc_media_playback_send_video_frame_with_encrypt(IN CONST UINT_T client, IN UINT_T reqId,
                                                      IN CONST TRANSFER_MEDIA_FRAME_WIHT_ENCRYPT_T *p_video_frame);

/**
 * @brief send playback audio frame to APP via P2P channel
 *
 * @param[in] client:client cliend id
 * @param[in] p_audio_frame:p_audio_frame
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ipc_media_playback_send_audio_frame(IN CONST UINT_T client,
                                                     IN CONST MEDIA_AUDIO_FRAME_T *p_audio_frame);
OPERATE_RET
tuya_ipc_media_playback_send_audio_frame_with_encrypt(IN CONST UINT_T client, IN UINT_T reqId,
                                                      IN CONST TRANSFER_MEDIA_FRAME_WIHT_ENCRYPT_T *p_audio_frame);

/**
 * @brief notify client(APP) playback fragment is finished, send frag info to app
 *
 * @param[in] client:client cliend id
 * @param[in] fgmt:playback time
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ipc_media_playback_send_fragment_end(IN CONST UINT_T client, IN CONST PLAYBACK_TIME_S *fgmt);

/**
 * @brief notify client(APP) playback data is finished, no more data outgoing
 *
 * @param[in] client:client cliend id
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ipc_media_playback_send_finish(IN CONST UINT_T client);

/**
 * @brief put log to tuya cloud service.
 *
 * @param level
 * @param log
 * @param log_len
 * @return VOID
 */
// VOID tuya_imm_media_online_log_print(IMM_SERVICE_LOG_LV_T level, CHAR_T *p, CHAR_T* pFmt, ...);

#ifdef __cplusplus
}
#endif

#endif /*__TUYA_IPC_MEDIA_STREAM_H__*/
