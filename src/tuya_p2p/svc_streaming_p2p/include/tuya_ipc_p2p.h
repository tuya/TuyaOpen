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
    eVideoIFrame,  ///< i frame
    eVideoTsFrame, ///< ts frame
    eAudioFrame,   ///< audio frame
    eCmdFrame,     ///< cmd frame
    eMediaFrameTypeMax
} MEDIA_FRAME_TYPE;

typedef struct tagMediaFrame {
    MEDIA_FRAME_TYPE type; ///< frame type
    uint8_t *data;      ///< fragment data
    uint32_t size;      ///< fragment size
    uint64_t pts;       ///< timestamp is us
    uint64_t timestamp; ///< timestamp is ms
} MEDIA_FRAME;

typedef int (*tuya_p2p_rtc_disconnect_cb_t)();
typedef int (*tuya_p2p_rtc_get_frame_cb_t)(MEDIA_FRAME *pMediaFrame);
typedef int (*tuya_p2p_rtc_live_video_cb_t)(void);

/**
 * @brief Ask the encoder for an immediate key frame.
 *
 * A viewer can only start decoding at a key frame, and after frames have been
 * dropped it can only resume at one. Without this the stream has to wait for
 * the next scheduled key frame, which at a two second GOP means up to two
 * seconds of black screen on connect and the same again after every congestion
 * event. Optional: where it is not provided the wait is simply the old one.
 */
typedef int (*tuya_p2p_rtc_req_i_frame_cb_t)(void);

/**
 * @brief Ask the encoder to produce @p kbps from now on.
 *
 * The transport can only carry what the link allows; when the send queue backs
 * up, the source has to slow down or the queue is worked off by discarding
 * video. This is the one lever that lets the picture degrade smoothly instead.
 * Optional: without it the encoder keeps its configured rate and congestion is
 * handled by dropping frames alone.
 */
typedef int (*tuya_p2p_rtc_set_bitrate_cb_t)(uint32_t kbps);

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
    int max_client_num;                  /**max p2p connect num*/
    TRANS_DEFAULT_QUALITY_E def_live_mode; /** for multi-streaming ipc, the default quality for live preview */
    BOOL_T low_power;
    uint32_t recv_buffer_size; /*recv app data size. if recv_buffer_size = 0,default = 16*1024*/
    TRANS_IPC_AV_INFO_T av_info;
    tuya_p2p_rtc_disconnect_cb_t on_disconnect_callback;
    tuya_p2p_rtc_get_frame_cb_t on_get_video_frame_callback;
    tuya_p2p_rtc_get_frame_cb_t on_get_audio_frame_callback;
    /* Align TuyaOS MEDIA_STREAM_LIVE_VIDEO_START/STOP: app starts/stops H264 feed */
    tuya_p2p_rtc_live_video_cb_t on_live_video_start_callback;
    tuya_p2p_rtc_live_video_cb_t on_live_video_stop_callback;
    /* Align TuyaOS MEDIA_STREAM_SPEAKER_START/STOP + on_recv_audio: downlink intercom */
    tuya_p2p_rtc_live_video_cb_t on_live_audio_start_callback;
    tuya_p2p_rtc_live_video_cb_t on_live_audio_stop_callback;
    tuya_p2p_rtc_get_frame_cb_t  on_recv_audio_frame_callback;
    /* Both optional - see the typedefs. Supplying them turns the transport's
     * congestion state into something the encoder can act on, instead of the
     * stream only ever reacting by dropping what it cannot send. */
    tuya_p2p_rtc_req_i_frame_cb_t on_request_i_frame_callback;
    tuya_p2p_rtc_set_bitrate_cb_t on_set_video_bitrate_callback;
} TUYA_IPC_P2P_VAR_T;

//////////////////////////////external interface////////////////////////////////////////////
OPERATE_RET p2p_init(const TUYA_IPC_P2P_VAR_T *p_var);
OPERATE_RET p2p_rtc_listen_start();
OPERATE_RET p2p_rtc_listen_stop();
/////////////////////////////////////////////////////////////////////////////////

OPERATE_RET tuya_ipc_init_trans_av_info(TRANS_IPC_AV_INFO_T *av_info);
OPERATE_RET tuya_p2p_rtc_register_get_video_frame_cb(tuya_p2p_rtc_get_frame_cb_t pCallback);
OPERATE_RET tuya_p2p_rtc_register_get_audio_frame_cb(tuya_p2p_rtc_get_frame_cb_t pCallback);
int OnGetVideoFrameCallback(MEDIA_FRAME *pMediaFrame);
int OnGetAudioFrameCallback(MEDIA_FRAME *pMediaFrame);

// OPERATE_RET tuya_ipc_tranfser_init(IN const TUYA_IPC_P2P_VAR_T *p_var);
// OPERATE_RET tuya_ipc_tranfser_quit(void);
// OPERATE_RET tuya_ipc_get_client_conn_info(OUT uint32_t *p_client_num, OUT CLIENT_CONNECT_INFO_T **p_p_conn_info);
// OPERATE_RET tuya_ipc_free_client_conn_info(IN CLIENT_CONNECT_INFO_T *p_conn_info);
OPERATE_RET tuya_ipc_tranfser_secret_mode(BOOL_T mode);
OPERATE_RET tuya_ipc_delete_video_finish(const uint32_t client);
OPERATE_RET tuya_ipc_delete_video_finish_v2(const uint32_t client, TUYA_DOWNLOAD_DATA_TYPE type, int success);
OPERATE_RET tuya_ipc_p2p_debug(void);
OPERATE_RET tuya_ipc_p2p_client_connect(int *handle, char *remote_id, char *local_key);
OPERATE_RET tuya_ipc_p2p_client_disconnect(int handle);
OPERATE_RET tuya_ipc_p2p_client_start_prev(int handle);
OPERATE_RET tuya_ipc_p2p_client_stop_prev(int handle);
OPERATE_RET tuya_ipc_p2p_client_start_audio(int handle);
OPERATE_RET tuya_ipc_p2p_client_stop_audio(int handle);
OPERATE_RET tuya_ipc_p2p_client_set_video_clarity_standard(int handle);
OPERATE_RET tuya_ipc_p2p_client_set_video_clarity_high(int handle);
OPERATE_RET tuya_ipc_p2p_client_video_send_start(int handle);
OPERATE_RET tuya_ipc_p2p_client_video_send_stop(int handle);
OPERATE_RET tuya_ipc_p2p_client_audio_send_start(int handle);
OPERATE_RET tuya_ipc_p2p_client_audio_send_stop(int handle);
// OPERATE_RET tuya_ipc_bind_clarity_with_chn(TRANSFER_VIDEO_CLARITY_TYPE_E type, TRANSFER_VIDEO_CLARITY_VALUE_E value);
OPERATE_RET tuya_ipc_p2p_set_limit_mode(BOOL_T islimit);

/***********************************album protocol ****************************************/
OPERATE_RET tuya_ipc_sweeper_convert_file_info(int *fileArray, void **pIndexFileInfo, int *fileInfoLen);
OPERATE_RET tuya_ipc_sweeper_parse_file_info(C2C_CMD_IO_CTRL_ALBUM_DOWNLOAD_START *srcfileInfo,
                                             int *fileArray, int arrSize);
OPERATE_RET tuya_ipc_sweeper_send_data_with_buff(int client, SWEEPER_ALBUM_FILE_TYPE_E type, int fileLen,
                                                 char *fileBuff);
OPERATE_RET tuya_ipc_sweeper_send_finish_2_app(int client);
OPERATE_RET tuya_ipc_stop_send_data_to_app(int client);
OPERATE_RET tuya_sweeper_send_data_with_buff(int client, char *name, int fileLen, char *fileBuff,
                                             int timeout_ms);
OPERATE_RET tuya_p2p_keep_alive(int client);

/* Uplink audio counters, for the app to print alongside its own. */
void tuya_ipc_p2p_audio_stats_get(uint32_t *shed, uint32_t *send_fail, uint32_t *queued);

#ifdef __cplusplus
}
#endif

#endif