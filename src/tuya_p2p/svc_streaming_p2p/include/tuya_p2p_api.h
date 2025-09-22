/*
 * tuya_p2p_api.h
 *Copyright(C),2017-2022, TUYA company www.tuya.com
 *
 *FILE description:
 *
 */

#ifndef INCLUDE_COMPONENTS_SVC_STREAMING_P2P_INCLUDE_TUYA_P2P_API_H_
#define INCLUDE_COMPONENTS_SVC_STREAMING_P2P_INCLUDE_TUYA_P2P_API_H_
#ifdef __cplusplus
extern "C" {
#endif
#include "tuya_cloud_types.h"
#include "tuya_ipc_media.h"
#include "tuya_ipc_media_stream_event.h"
#include "tuya_ipc_media_adapter.h"

/**
* @brief initialize tuya P2P suggestion do init after ipc has been activated(mqtt online)
*
* @param[in] p_var:p2p param

* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET tuya_imm_p2p_init(IN CONST TUYA_IPC_P2P_VAR_T *p_var);

/**
* @brief  close all P2P conections, live preivew & playback
*
* @param[in]VOID

* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET tuya_imm_p2p_close(VOID);

/**
 * @brief cur p2p connect num
 *
 * @param[in] VOID
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_imm_p2p_alive_cnt(VOID);

/**
 * @brief close p2p all connect
 *
 * @param[in] VOID
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_imm_p2p_all_stream_close(INT_T close_reason);
/**
 * @brief delete video finish v2
 *
 * @param[in] dev_id:dev_id.ipc device allow to equal NULL;
 * @param[in] client:current connected client number
 * @param[in] type:download type
 * @param[in] success:0 fail 1 success
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_imm_p2p_delete_video_finish(IN CONST CHAR_T *dev_id, IN CONST UINT_T client,
                                             TUYA_DOWNLOAD_DATA_TYPE type, int success);

/**
 * @brief send playback video frame to APP via P2P channel
 *
 * @param[in] dev_id:dev_id.ipc device allow to equal NULL;
 * @param[in] client:client cliend id
 * @param[in] p_video_frame:p_video_frame
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_imm_p2p_playback_send_video_frame(IN CONST CHAR_T *dev_id, IN CONST UINT_T client,
                                                   IN CONST MEDIA_VIDEO_FRAME_T *p_video_frame);

/**
 * @brief send playback audio frame to APP via P2P channel
 *
 * @param[in] dev_id:dev_id.ipc device allow to equal NULL;
 * @param[in] client:client cliend id
 * @param[in] p_audio_frame:p_audio_frame
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_imm_p2p_playback_send_audio_frame(IN CONST CHAR_T *dev_id, IN CONST UINT_T client,
                                                   IN CONST MEDIA_AUDIO_FRAME_T *p_audio_frame);

/**
 * @brief send video frame with encrypt
 *
 * @param[in] client:current connected client number
 * @param[in] reqId:request id
 * @param[in] p_video_frame:video frame
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET
tuya_imm_p2p_playback_send_video_frame_with_encrypt(IN CONST UINT_T client, IN UINT_T reqId,
                                                    IN CONST TRANSFER_MEDIA_FRAME_WIHT_ENCRYPT_T *p_video_frame);

/**
 * @brief send audio frame with encrypt
 *
 * @param[in] client:current connected client number
 * @param[in] reqId:request id
 * @param[in] p_audio_frame:audio frame
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET
tuya_imm_p2p_playback_send_audio_frame_with_encrypt(IN CONST UINT_T client, IN UINT_T reqId,
                                                    IN CONST TRANSFER_MEDIA_FRAME_WIHT_ENCRYPT_T *p_audio_frame);

/**
 * @brief notify client(APP) playback fragment is finished, send frag info to app
 *
 * @param[in] dev_id:dev_id.ipc device allow to equal NULL;
 * @param[in] client:client cliend id
 * @param[in] fgmt:playback time
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_imm_p2p_playback_send_fragment_end(IN CONST CHAR_T *dev_id, IN CONST UINT_T client,
                                                    IN CONST PLAYBACK_TIME_S *fgmt);

/**
 * @brief notify client(APP) playback data is finished, no more data outgoing
 *
 * @param[in] dev_id:dev_id.ipc device allow to equal NULL;
 * @param[in] client:client cliend id
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_imm_p2p_playback_send_finish(IN CONST CHAR_T *dev_id, IN CONST UINT_T client);

/**
 * @brief download data transfer api V2
 *
 * @param[in] dev_id:dev_id.ipc device allow to equal NULL;
 * @param[in] client: current connected client number
 * @param[in] type: frame head info
 * @param[in] pHead: download type
 * @param[in] pData: media data
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_imm_p2p_app_download_data(IN CONST CHAR_T *dev_id, IN CONST UINT_T client,
                                           TUYA_DOWNLOAD_DATA_TYPE type, IN CONST void *pHead, IN CONST CHAR_T *pData);

/**
 * @brief cur download status
 *
 * @param[in] dev_id:dev_id.ipc device allow to equal NULL;
 * @param[in] client: current connected client number
 * @param[in] percent: percent(0-100),cur only 100 in use
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */

OPERATE_RET tuya_imm_p2p_app_download_status(IN CONST CHAR_T *dev_id, IN CONST UINT_T client, IN CONST UINT_T percent);

/**
 * @brief cur download status is over
 *
 * @param[in] dev_id:dev_id.ipc device allow to equal NULL;
 * @param[in] client:current connected client number
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_imm_p2p_app_download_is_send_over(IN CONST CHAR_T *dev_id, IN CONST UINT_T client);

/**
 * @brief album file play data transfer api V2
 *
 * @param[in] dev_id:dev_id.ipc device allow to equal NULL;
 * @param[in] client: current connected client number
 * @param[in] p_frame: media frame
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_imm_p2p_app_album_play_send_data(IN CONST CHAR_T *dev_id, IN CONST UINT_T client,
                                                  IN CONST TUYA_ALBUM_PLAY_FRAME_T *p_frame);

/**
 * @brief notify client(APP) album play data is finished, no more data outgoing
 *
 * @param[in] dev_id:dev_id.ipc device allow to equal NULL;
 * @param[in] client:client cliend id
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_imm_p2p_album_play_send_finish(IN CONST CHAR_T *dev_id, IN CONST UINT_T client);

#ifdef __cplusplus
}
#endif
#endif /* INCLUDE_COMPONENTS_SVC_STREAMING_P2P_INCLUDE_TUYA_P2P_API_H_ */
