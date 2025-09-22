#ifndef __TUYA_IPC_MEDIA_ADAPTER_H__
#define __TUYA_IPC_MEDIA_ADAPTER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tuya_ipc_media.h"
#include "tuya_ipc_media_stream_event.h"

#define MAX_DECODER_CNT 10

/** @enum MEDIA_RECV_VIDEO_FRAME_TYPE_E
 * @brief frame type
 */
typedef enum {
    TUYA_VIDEO_FRAME_PBFRAME, ///< P/B frame */
    TUYA_VIDEO_FRAME_IFRAME,  ///< I frame */
} MEIDA_RECV_VIDEO_FRAME_TYPE_E;

/** @enum STREAM_SOURCE_OPEN_TYPE_E
 * @brief open for read or write
 */
typedef enum {
    STREAM_READ = 0,  ///< open for read
    STREAM_WRITE = 1, ///< open for write
} STREAM_SOURCE_OPEN_TYPE_E;

/**************************struct define***************************************/

/** @struct MEDIA_VIDEO_FRAME_T
 * @brief video frame information
 */
typedef struct {
    TUYA_CODEC_ID_E video_codec;                    ///< video codec
    MEIDA_RECV_VIDEO_FRAME_TYPE_E video_frame_type; ///< video frame type
    UINT_T width;                                   ///< video width
    UINT_T height;                                  ///< video height
    UINT_T fps;                                     ///< video fps
    BYTE_T *p_video_buf;                            ///< video data buf
    UINT_T buf_len;                                 ///< video buf len
    UINT64_T pts;                                   ///< video timestamp in us
    UINT64_T timestamp;                             ///< video timestamp in ms
    VOID *p_reserved;                               ///< reserved
} MEDIA_VIDEO_FRAME_T;

/** @struct MEDIA_AUDIO_FRAME_T
 * @brief audio frame information
 */
typedef struct {
    TUYA_CODEC_ID_E audio_codec;          ///< audio codec
    TUYA_AUDIO_SAMPLE_E audio_sample;     ///< audio sample
    TUYA_AUDIO_DATABITS_E audio_databits; ///< audio databits
    TUYA_AUDIO_CHANNEL_E audio_channel;   ///< audio channel
    BYTE_T *p_audio_buf;                  ///< audio data buffer
    UINT_T buf_len;                       ///< audio buf len
    UINT64_T pts;                         ///< audio timestamp is us
    UINT64_T timestamp;                   ///< audio timestamp is ms
    VOID *p_reserved;                     ///< reserved
} MEDIA_AUDIO_FRAME_T;

/** @struct MEDIA_FILE_DATA_T
 */
typedef struct {
    INT_T session_id;
    TY_DATA_TRANSFER_STAT status;
    CHAR_T *file_buf;
    INT_T file_len;
    VOID *fragment_info;
    VOID *trans_info;
} MEDIA_FILE_DATA_T;

/** @struct MEDIA_AUDIO_DECODE_INFO_T
 * @brief audio decode information for talking
 */
typedef struct {
    BOOL_T enable;                        ///< enable or not
    TUYA_CODEC_ID_E audio_codec;          ///< audio codec type supported
    TUYA_AUDIO_SAMPLE_E audio_sample;     ///< audio sample rate
    TUYA_AUDIO_DATABITS_E audio_databits; ///< audio databits
    TUYA_AUDIO_CHANNEL_E audio_channel;   ///< audio channel number
} MEDIA_AUDIO_DECODE_INFO_T;

/** @struct DEVICE_MEDIA_INFO_T
 */
typedef struct {
    IPC_MEDIA_INFO_T av_encode_info;             ///< encoder info
    MEDIA_AUDIO_DECODE_INFO_T audio_decode_info; ///< decoder info
    INT_T max_pic_len;                           ///< max picture size, in KB
    INT_T decoder_cnt;                           ///< other decoder cnt
    TUYA_DECODER_T *decoders;                    ///< other decoders
} DEVICE_MEDIA_INFO_T;

/** @struct MEDIA_ALLOC_RESOURCE_T
 * @brief memory resource of a stream
 */
typedef struct {
    INT_T need_memory; ///< memory resource need to alloc
} MEDIA_ALLOC_RESOURCE_T;

typedef VOID *MEDIA_USER_HANDLE;

typedef VOID *MEDIA_STREAM_HANDLE;

/** @brief callback to recv audio frame
 * @param[in]  device    device number
 * @param[in]  channel   channel number
 * @param[in]  media_audio_frame  audio frame info
 */
typedef VOID (*MEDIA_RECV_AUDIO_CB)(IN INT_T device, IN INT_T channel, IN CONST MEDIA_AUDIO_FRAME_T *media_audio_frame);

/** @brief callback to recv video frame
 * @param[in]  device  device number
 * @param[in]  channel channel number
 * @param[in]  media_video_frame  video frame info
 */
typedef VOID (*MEDIA_RECV_VIDEO_CB)(IN INT_T device, IN INT_T channel, IN CONST MEDIA_VIDEO_FRAME_T *media_video_frame);

/** @brief callback to recv file
 * @param[in]  device  device number
 * @param[in]  channel channel number
 * @param[in]  media_file_data  file data info
 */
typedef VOID (*MEDIA_RECV_FILE_CB)(IN INT_T device, IN INT_T channel, IN CONST MEDIA_FILE_DATA_T *media_file_data);

/** @brief callback to get a snapshot
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @param[in] pic_buf  picture buffer
 * @param[in] pic_len  picture length
 */
typedef VOID (*MEDIA_GET_SNAPSHOT_CB)(IN INT_T device, IN INT_T channel, INOUT CHAR_T *pic_buf, INOUT INT_T *pic_len);

/** @brief callback when media info change
 * @param[in]  info new media info
 */
typedef VOID (*MEDIA_INFO_CB)(IN INT_T device, IN INT_T channel, IN CONST DEVICE_MEDIA_INFO_T info);
typedef VOID (*MEDIA_CLOSE_CB)(IN INT_T device, IN INT_T channel);

/** @struct TUYA_IPC_MEDIA_ADAPTER_VAR_T
 * @brief media adapter paramters
 */
typedef struct {
    MEDIA_GET_SNAPSHOT_CB get_snapshot_cb; ///< snapshot callback
    MEDIA_RECV_VIDEO_CB on_recv_video_cb;  ///< recv video callback
    MEDIA_RECV_AUDIO_CB on_recv_audio_cb;  ///< recv audio callback
    MEDIA_RECV_FILE_CB on_recv_file_cb;    ///< recv file callback
    INT_T available_media_memory;          ///< max memory size of P2P/webrtc(MB). 0 means default, 5MB
} TUYA_IPC_MEDIA_ADAPTER_VAR_T;

/** @struct FRAME_FRAGMENT_T
 * @brief fragme fragment info
 */
typedef struct {
    MEDIA_FRAME_TYPE_E type; ///< frame type
    UCHAR_T *data;           ///< fragment data
    UINT_T size;             ///< fragment size
    UINT64_T pts;            ///< timestamp is us
    UINT64_T timestamp;      ///< timestamp is ms
    UINT_T seq_no;           ///< sequence number, start from 1, keep same within one frame
    UCHAR_T frag_status;     ///< fragment flag:0-no fragment，1-first fragment，2-middle fragment，3-last fragment
    UCHAR_T frag_no;         ///< fragment number, start from 0
} FRAME_FRAGMENT_T;

/** @struct TUYA_IPC_MEDIA_SOURCE_T
 * @brief media source interface for stream operation
 */
typedef struct {
    /** @brief get a frame fragment
     * @param[in] handle    handle returned by open_stream
     * @param[in] is_retry  whether to retry to get the last frame
     * @param[out] p_media_frame  the frame fragment
     * @return error code
     * - OPRT_OK  success
     * - Others  fail
     */
    OPERATE_RET (*get_media_frame)(MEDIA_STREAM_HANDLE handle, BOOL_T is_retry, FRAME_FRAGMENT_T *p_media_frame);

    /** @brief get a video/audio frame fragment syncd by timestamp
     * @param[in] v_handle  video handle return by open_stream
     * @param[in] a_handle  audio handle return by open_stream
     * @param[in] is_retry whether to retry to get the last frame
     * @param[in] p_media_frame the frame fragment
     * @return error code
     * - OPRT_OK  success
     * - Others  fail
     */
    OPERATE_RET (*get_av_frame)
    (MEDIA_STREAM_HANDLE v_handle, MEDIA_STREAM_HANDLE a_handle, BOOL_T is_retry, FRAME_FRAGMENT_T *p_media_frame);

    /** @brief open a stream for read or write
     * @param[in] device  device number
     * @param[in] channel channel number
     * @param[in] stream  stream number
     * @param[in] open_type open for read or write
     * @return stream handle
     */
    MEDIA_STREAM_HANDLE (*open_stream)
    (INT_T device, INT_T channel, IPC_STREAM_E stream, STREAM_SOURCE_OPEN_TYPE_E open_type);

    /** @brief close a stream
     * @param[in]  handle  handle return by open_stream
     * @return error code
     * - OPRT_OK  success
     * - Others  fail
     */
    OPERATE_RET (*close_stream)(MEDIA_STREAM_HANDLE handle);

    /** @brief seek backward several frames in stream
     * @param[in] handle handle return by open_stream
     * @param[in] frame_num frame number to backward to
     * @param[in] check_overlap whether check overlap, if set to true, it will not seek to frames that have got
     * @return error code
     * - OPRT_OK  success
     * - Others  fail
     */
    OPERATE_RET (*seek_frame)(MEDIA_STREAM_HANDLE handle, UINT_T frame_num, BOOL_T check_overlap);

    /** @brief sync video and audio stream
     * @param[in] v_handle video handle
     * @param[in] a_handle audio handle
     * @return error code
     * - OPRT_OK  success
     * - Others  fail
     */
    OPERATE_RET (*sync_stream)(MEDIA_STREAM_HANDLE v_handle, MEDIA_STREAM_HANDLE a_handle);

    /** @brief seek frames in stream by timestamp
     * @param[in] handle handle return by open_stream
     * @param[in] frame_timestamp_ms frame time to backward to
     * @param[in] check_overlap whether check overlap, if set to true, it will not seek to frames that have got
     * @return error code
     * - OPRT_OK  success
     * - Others  fail
     */
    OPERATE_RET (*seek_frame_by_time)(MEDIA_STREAM_HANDLE handle, UINT64_T frame_timestamp_ms, BOOL_T check_overlap);

} TUYA_IPC_MEDIA_SOURCE_T;

/**************************function define***************************************/

/** @brief media adapter module init
 * @param[in] p_var init parameters
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_init(TUYA_IPC_MEDIA_ADAPTER_VAR_T *p_var);

/** @brief alloc media resource
 * @param[in] resource  media source
 * @param[out] user_handle user handle
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_alloc_resource(MEDIA_ALLOC_RESOURCE_T resource, MEDIA_USER_HANDLE *user_handle);

/** @brief release media resource
 * @param[in] user_handle user handle return by alloc_resource
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_release_resource(MEDIA_USER_HANDLE user_handle);

/** @brief get a snapshot
 * @param[in] device device number
 * @param[in] channel channel number
 * @param[out] buf  image buffer
 * @param[out] buf_len image size
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_snapshot_get(INT_T device, INT_T channel, CHAR_T **buf, INT_T *buf_len);

/** @brief delete a snapshot
 * @param[in] buf the image buffer returned by snapshot_get
 */
VOID tuya_ipc_media_adapter_snapshot_delete(CHAR_T *buf);

/** @brief get max picture length that sdk can handle
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @return max picture length
 */
INT_T tuya_ipc_media_adapter_get_max_pic_len(INT_T device, INT_T channel);

/** @brief get max frame length that sdk can handle
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @param[in] stream  stream number
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_get_max_frame(INT_T device, INT_T channel, IPC_STREAM_E stream);

/** @brief get media info
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @param[out] media_info media info
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_get_media_info(INT_T device, INT_T channel, DEVICE_MEDIA_INFO_T *media_info);

/** @brief set media info
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @param[in] media_info media info
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_set_media_info(INT_T device, INT_T channel, DEVICE_MEDIA_INFO_T media_info);

/** @brief attach to a media info, when the media change, it will receive a callback
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @param[in] media_cb  callback
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_attach_media_info(INT_T device, INT_T channel, MEDIA_INFO_CB media_cb,
                                                     MEDIA_CLOSE_CB media_close_cb);

/** @brief detach from a media info
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @param[in] media_cb  callback
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_detach_media_info(INT_T device, INT_T channel, MEDIA_INFO_CB media_cb,
                                                     MEDIA_CLOSE_CB media_close_cb);

/** @brief start speaker
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @param[in] user_handle  handle return by alloc_resource
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_speak_start(INT_T device, INT_T channel, MEDIA_USER_HANDLE user_handle);

/** @brief stop speaker
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @param[in] user_handle  handle return by alloc_resource
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_speak_stop(INT_T device, INT_T channel, MEDIA_USER_HANDLE user_handle);

/** @brief start display
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @param[in] user_handle  handle return by alloc_resource
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_display_start(INT_T device, INT_T channel, MEDIA_USER_HANDLE user_handle);

/** @brief stop display
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @param[in] user_handle  handle return by alloc_resource
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_display_stop(INT_T device, INT_T channel, MEDIA_USER_HANDLE user_handle);

/** @brief output audio frame to application
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @param[in] audio audio frame
 * @param[in] user_handle handle returned by alloc_resource
 */
VOID tuya_ipc_media_adapter_audio_output(INT_T device, INT_T channel, MEDIA_AUDIO_FRAME_T *audio,
                                         MEDIA_USER_HANDLE user_handle);

/** @brief output video frame to application
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @param[in] video video frame
 * @param[in] user_handle handle returned by alloc_resource
 */
VOID tuya_ipc_media_adapter_video_output(INT_T device, INT_T channel, MEDIA_VIDEO_FRAME_T *video,
                                         MEDIA_USER_HANDLE user_handle);

/** @brief output file data to application
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @param[in] file file data
 * @param[in] user_handle handle returned by alloc_resource
 */
VOID tuya_ipc_media_adapter_file_output(INT_T device, INT_T channel, MEDIA_FILE_DATA_T *file,
                                        MEDIA_USER_HANDLE user_handle);

/** @brief get media source
 * @return media source
 */
TUYA_IPC_MEDIA_SOURCE_T *tuya_ipc_media_adapter_get_media_source();

/** @brief register media source
 * @param[in] media_source
 * @return error code
 * - OPRT_OK  success
 * - Others  fail
 */
OPERATE_RET tuya_ipc_media_adapter_register_media_source(TUYA_IPC_MEDIA_SOURCE_T *media_frame_instance);

/** @brief check if output device busy
 * @param[in] device  device number
 * @param[in] channel  channel number
 * @return TRUE or FALSE
 */
BOOL_T tuya_ipc_media_adapter_output_device_is_busy(INT_T device, INT_T channel);

#ifdef __cplusplus
}
#endif

#endif /*_TUYA_IPC_SKILL_H_*/
