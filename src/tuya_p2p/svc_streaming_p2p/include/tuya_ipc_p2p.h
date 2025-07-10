#ifndef __TUYA_IPC_P2P2_H__
#define __TUYA_IPC_P2P2_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tuya_ipc_p2p_inner.h"
#include "tuya_ipc_media_adapter.h"

#define RTC_CLOSE_REASON_SECRET_MODE        (2)
#define RTC_CLOSE_REASON_THREAD_CREATE_FAIL (3)
#define RTC_CLOSE_REASON_SESSION_FULL       (4)
#define RTC_CLOSE_REASON_AUTH_FAIL          (5)
#define RTC_CLOSE_REASON_WEBRTC_THREAD_FAIL (7)
#define RTC_CLOSE_REASON_ZOMBIE_SESSION     (8)
#define RTC_CLOSE_REASON_USER_CLOSE         (9)
#define RTC_CLOSE_REASON_P2P_EXIT           (10)
#define RTC_CLOSE_REASON_BE_SECRET_MODE     (11)
#define RTC_CLOSE_REASON_RECV_ERR           (12)
#define RTC_CLOSE_REASON_MALLOC_ERR         (14)
#define RTC_CLOSE_REASON_RESTRICT_MODE      (15)

typedef enum tagMediaFrameType {
    eVideoPBFrame = 0, ///< p frame
    eVideoIFrame,      ///< i frame
    eVideoTsFrame,     ///< ts frame
    eAudioFrame,       ///< audio frame
    eCmdFrame,         ///< cmd frame
    eMediaFrameTypeMax
} MEDIA_FRAME_TYPE;

typedef struct tagMediaFrame {
    MEDIA_FRAME_TYPE type; ///< frame type
    UCHAR_T *data;         ///< fragment data
    UINT_T size;           ///< fragment size
    UINT64_T pts;          ///< timestamp is us
    UINT64_T timestamp;    ///< timestamp is ms
} MEDIA_FRAME;

typedef INT_T (*tuya_p2p_rtc_disconnect_cb_t)();
typedef INT_T (*tuya_p2p_rtc_get_frame_cb_t)(MEDIA_FRAME *pMediaFrame);

/**
 * @enum TRANS_DEFAULT_QUALITY_E
 *
 * @brief default quality for live P2P transferring
 */
typedef enum {
    TRANS_DEFAULT_STANDARD = 0, /**ex. 640*480, 15fps */
    TRANS_DEFAULT_HIGH,         /** ex. 1920*1080, 20fps */
    TRANS_DEFAULT_THIRD,
    TRANS_DEFAULT_FOURTH,
    TRANS_DEFAULT_MAX
} TRANS_DEFAULT_QUALITY_E;

typedef struct {
    INT_T max_client_num;                  /**max p2p connect num*/
    TRANS_DEFAULT_QUALITY_E def_live_mode; /** for multi-streaming ipc, the default quality for live preview */
    BOOL_T low_power;
    UINT_T recv_buffer_size; /*recv app data size. if recv_buffer_size = 0,default = 16*1024*/
    TRANS_IPC_AV_INFO_T av_info;
    tuya_p2p_rtc_disconnect_cb_t on_disconnect_callback;
    tuya_p2p_rtc_get_frame_cb_t on_get_video_frame_callback;
    tuya_p2p_rtc_get_frame_cb_t on_get_audio_frame_callback;
} TUYA_IPC_P2P_VAR_T;

//////////////////////////////external interface////////////////////////////////////////////
OPERATE_RET p2p_init(IN CONST TUYA_IPC_P2P_VAR_T *p_var);
OPERATE_RET p2p_rtc_listen_start();
OPERATE_RET p2p_rtc_listen_stop();
/////////////////////////////////////////////////////////////////////////////////

// OPERATE_RET tuya_ipc_init_trans_av_info(TRANS_IPC_AV_INFO_T *av_info);
OPERATE_RET tuya_p2p_rtc_register_get_video_frame_cb(tuya_p2p_rtc_get_frame_cb_t pCallback);
OPERATE_RET tuya_p2p_rtc_register_get_audio_frame_cb(tuya_p2p_rtc_get_frame_cb_t pCallback);
INT_T OnGetVideoFrameCallback(MEDIA_FRAME *pMediaFrame);
INT_T OnGetAudioFrameCallback(MEDIA_FRAME *pMediaFrame);

// OPERATE_RET tuya_ipc_tranfser_init(IN CONST TUYA_IPC_P2P_VAR_T *p_var);
// OPERATE_RET tuya_ipc_tranfser_quit(VOID);
// OPERATE_RET tuya_ipc_get_client_conn_info(OUT UINT_T *p_client_num, OUT CLIENT_CONNECT_INFO_T **p_p_conn_info);
// OPERATE_RET tuya_ipc_free_client_conn_info(IN CLIENT_CONNECT_INFO_T *p_conn_info);
OPERATE_RET tuya_ipc_tranfser_secret_mode(BOOL_T mode);
OPERATE_RET tuya_ipc_delete_video_finish(IN CONST UINT_T client);
OPERATE_RET tuya_ipc_delete_video_finish_v2(IN CONST UINT_T client, TUYA_DOWNLOAD_DATA_TYPE type, int success);
OPERATE_RET tuya_ipc_p2p_debug(VOID);
OPERATE_RET tuya_ipc_p2p_client_connect(OUT INT_T *handle, IN char *remote_id, IN char *local_key);
OPERATE_RET tuya_ipc_p2p_client_disconnect(int handle);
OPERATE_RET tuya_ipc_p2p_client_start_prev(INT_T handle);
OPERATE_RET tuya_ipc_p2p_client_stop_prev(INT_T handle);
OPERATE_RET tuya_ipc_p2p_client_start_audio(INT_T handle);
OPERATE_RET tuya_ipc_p2p_client_stop_audio(INT_T handle);
OPERATE_RET tuya_ipc_p2p_client_set_video_clarity_standard(INT_T handle);
OPERATE_RET tuya_ipc_p2p_client_set_video_clarity_high(INT_T handle);
OPERATE_RET tuya_ipc_p2p_client_video_send_start(INT_T handle);
OPERATE_RET tuya_ipc_p2p_client_video_send_stop(INT_T handle);
OPERATE_RET tuya_ipc_p2p_client_audio_send_start(INT_T handle);
OPERATE_RET tuya_ipc_p2p_client_audio_send_stop(INT_T handle);
// OPERATE_RET tuya_ipc_bind_clarity_with_chn(TRANSFER_VIDEO_CLARITY_TYPE_E type, TRANSFER_VIDEO_CLARITY_VALUE_E value);
OPERATE_RET tuya_ipc_p2p_set_limit_mode(BOOL_T islimit);

/***********************************album protocol ****************************************/
OPERATE_RET tuya_ipc_sweeper_convert_file_info(IN INT_T *fileArray, OUT VOID **pIndexFileInfo, OUT INT_T *fileInfoLen);
OPERATE_RET tuya_ipc_sweeper_parse_file_info(IN C2C_CMD_IO_CTRL_ALBUM_DOWNLOAD_START *srcfileInfo,
                                             INOUT INT_T *fileArray, IN INT_T arrSize);
OPERATE_RET tuya_ipc_sweeper_send_data_with_buff(IN INT_T client, SWEEPER_ALBUM_FILE_TYPE_E type, IN INT_T fileLen,
                                                 IN CHAR_T *fileBuff);
OPERATE_RET tuya_ipc_sweeper_send_finish_2_app(IN INT_T client);
OPERATE_RET tuya_ipc_stop_send_data_to_app(IN INT_T client);
OPERATE_RET tuya_sweeper_send_data_with_buff(IN INT_T client, IN CHAR_T *name, IN INT_T fileLen, IN CHAR_T *fileBuff,
                                             IN INT_T timeout_ms);
OPERATE_RET tuya_p2p_keep_alive(IN INT_T client);

#ifdef __cplusplus
}
#endif

#endif