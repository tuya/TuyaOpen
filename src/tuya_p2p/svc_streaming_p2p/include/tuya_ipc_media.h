#ifndef _TUYA_IPC_MEDIA_H_
#define _TUYA_IPC_MEDIA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/** @brief default max frame size that can be put into ring buffer
 */
#define MAX_MEDIA_FRAME_SIZE (300 * 1024)

/** @enum MEDIA_FRAME_TYPE_E
 * @brief media frame type
 */
typedef enum {
    E_VIDEO_PB_FRAME = 0, ///< p frame
    E_VIDEO_I_FRAME,      ///< i frame
    E_VIDEO_TS_FRAME,     ///< ts frame
    E_AUDIO_FRAME,        ///< audio frame
    E_CMD_FRAME,          ///< cmd frame
    E_MEDIA_FRAME_TYPE_MAX
} MEDIA_FRAME_TYPE_E;

/** @enum IPC_STREAM_E
 * @brief stream type
 */
typedef enum {
    E_IPC_STREAM_VIDEO_MAIN, ///< first video stream
    E_IPC_STREAM_VIDEO_SUB,  ///< second video stream
    E_IPC_STREAM_VIDEO_3RD,  ///< third video stream
    E_IPC_STREAM_VIDEO_4TH,  ///< forth video stream
    E_IPC_STREAM_VIDEO_MAX = 8,
    E_IPC_STREAM_AUDIO_MAIN, ///< first audio stream
    E_IPC_STREAM_AUDIO_SUB,  ///< second audio stream
    E_IPC_STREAM_AUDIO_3RD,  ///< third audio stream
    E_IPC_STREAM_AUDIO_4TH,  ///< forth audio stream
    E_IPC_STREAM_MAX = 16,
} IPC_STREAM_E;

/** @enum TUYA_CODEC_ID_E
 * @warning sync with tuya cloud and app, should not be changed
 */
typedef enum {
    TUYA_CODEC_VIDEO_MPEG4 = 0,
    TUYA_CODEC_VIDEO_H263,
    TUYA_CODEC_VIDEO_H264,
    TUYA_CODEC_VIDEO_MJPEG,
    TUYA_CODEC_VIDEO_H265,
    TUYA_CODEC_VIDEO_YUV420,
    TUYA_CODEC_VIDEO_YUV422,
    TUYA_CODEC_VIDEO_MAX = 99,

    TUYA_CODEC_AUDIO_ADPCM, // 100
    TUYA_CODEC_AUDIO_PCM,
    TUYA_CODEC_AUDIO_AAC_RAW,
    TUYA_CODEC_AUDIO_AAC_ADTS,
    TUYA_CODEC_AUDIO_AAC_LATM,
    TUYA_CODEC_AUDIO_G711U, // 105
    TUYA_CODEC_AUDIO_G711A,
    TUYA_CODEC_AUDIO_G726,
    TUYA_CODEC_AUDIO_SPEEX,
    TUYA_CODEC_AUDIO_MP3,
    TUYA_CODEC_AUDIO_G722, // 110
    TUYA_CODEC_AUDIO_MAX = 199,
    TUYA_CODEC_INVALID
} TUYA_CODEC_ID_E;

/** @enum TUYA_AUDIO_SAMPLE_E
 * @brief audio sample rate
 */
typedef enum {
    TUYA_AUDIO_SAMPLE_8K = 8000,
    TUYA_AUDIO_SAMPLE_11K = 11000,
    TUYA_AUDIO_SAMPLE_12K = 12000,
    TUYA_AUDIO_SAMPLE_16K = 16000,
    TUYA_AUDIO_SAMPLE_22K = 22000,
    TUYA_AUDIO_SAMPLE_24K = 24000,
    TUYA_AUDIO_SAMPLE_32K = 32000,
    TUYA_AUDIO_SAMPLE_44K = 44000,
    TUYA_AUDIO_SAMPLE_48K = 48000,
    TUYA_AUDIO_SAMPLE_MAX = 0xFFFFFFFF
} TUYA_AUDIO_SAMPLE_E;

/** @enum TUYA_VIDEO_BITRATE_E
 * @brief video bitrate, in kb
 */
typedef enum {
    TUYA_VIDEO_BITRATE_64K = 64,
    TUYA_VIDEO_BITRATE_128K = 128,
    TUYA_VIDEO_BITRATE_256K = 256,
    TUYA_VIDEO_BITRATE_512K = 512,
    TUYA_VIDEO_BITRATE_768K = 768,
    TUYA_VIDEO_BITRATE_1M = 1024,
    TUYA_VIDEO_BITRATE_1_5M = 1536,
    TUYA_VIDEO_BITRATE_2M = 2048, // maximum 2Mbps stream is supported, as consideration of cloud storage order price
    TUYA_VIDEO_BITRATE_5M = 5120
} TUYA_VIDEO_BITRATE_E; // Kbps

/** @enum TUYA_AUDIO_DATABITS_E
 * @brief audio databits
 */
typedef enum {
    TUYA_AUDIO_DATABITS_8 = 8,
    TUYA_AUDIO_DATABITS_16 = 16,
    TUYA_AUDIO_DATABITS_MAX = 0xFF
} TUYA_AUDIO_DATABITS_E;

/** @enum TUYA_AUDIO_CHANNEL_E
 * @brief audio channel
 */
typedef enum {
    TUYA_AUDIO_CHANNEL_MONO,
    TUYA_AUDIO_CHANNEL_STERO,
} TUYA_AUDIO_CHANNEL_E;

/** @struct IPC_MEDIA_INFO_T
 * @brief media info of encoder
 */
typedef struct {
    BOOL_T stream_enable[E_IPC_STREAM_MAX]; ///< set to true if this stream has data

    UINT_T video_fps[E_IPC_STREAM_VIDEO_MAX];                   ///< video fps
    UINT_T video_gop[E_IPC_STREAM_VIDEO_MAX];                   ///< video gop size
    TUYA_VIDEO_BITRATE_E video_bitrate[E_IPC_STREAM_VIDEO_MAX]; ///< video bitrate
    UINT_T video_width[E_IPC_STREAM_VIDEO_MAX];                 ///< video width
    UINT_T video_height[E_IPC_STREAM_VIDEO_MAX];                ///< video height
    UINT_T video_freq[E_IPC_STREAM_VIDEO_MAX];                  ///< video frequency
    TUYA_CODEC_ID_E video_codec[E_IPC_STREAM_VIDEO_MAX];        ///< video codec

    TUYA_CODEC_ID_E audio_codec[E_IPC_STREAM_MAX];          ///< audio codec
    UINT_T audio_fps[E_IPC_STREAM_MAX];                     ///< audio fps
    TUYA_AUDIO_SAMPLE_E audio_sample[E_IPC_STREAM_MAX];     ///< audio sample
    TUYA_AUDIO_DATABITS_E audio_databits[E_IPC_STREAM_MAX]; ///< audio databits
    TUYA_AUDIO_CHANNEL_E audio_channel[E_IPC_STREAM_MAX];   ///< audio channel
} IPC_MEDIA_INFO_T;

/** @enum VIDEO_AVC_PROFILE_TYPE_E
 * @brief video profile type
 */
typedef enum {
    VIDEO_AVC_PROFILE_BASE_LINE = 0x01,
    VIDEO_AVC_PROFILE_MAIN = 0x02,
    VIDEO_AVC_PROFILE_EXTENDED = 0x04,
    VIDEO_AVC_PROFILE_HIGH = 0x08,
    VIDEO_AVC_PROFILE_HIGH10 = 0x10,
    VIDEO_AVC_PROFILE_HIGH422 = 0x20,
    VIDEO_AVC_PROFILE_HIGH444 = 0x40,
    VIDEO_AVC_PROFILE_KHRONOS_EXTENTIONS = 0x6F000000,
    VIDEO_AVC_PROFILE_VENDOR_START_UNUSED = 0x7F000000,
    VIDEO_AVC_PROFILE_MAX = 0x7FFFFFFF,
} VIDEO_AVC_PROFILE_TYPE_E;

/** @struct TUYA_VIDEO_DECODER_DESC_T
 * @brief video decoder information
 */
typedef struct {
    VIDEO_AVC_PROFILE_TYPE_E profile; ///< video profile type
    UINT_T height;                    ///< video height
    UINT_T width;                     ///< video width
} TUYA_VIDEO_DECODER_DESC_T;

/** @struct TUYA_AUDIO_DECODER_DESC_T
 * @brief audio decoder information
 */
typedef struct {
    TUYA_AUDIO_SAMPLE_E sample;     ///< audio sample
    TUYA_AUDIO_DATABITS_E databits; ///< audio databits
} TUYA_AUDIO_DECODER_DESC_T;

/** @union TUYA_DECODER_DESC_U
 * @brief decoder information
 */
typedef union {
    TUYA_VIDEO_DECODER_DESC_T v_decoder; ///< video decoder information
    TUYA_AUDIO_DECODER_DESC_T a_decoder; ///< audio decoder information
} TUYA_DECODER_DESC_U;

/** @struct TUYA_DECODER_T
 * @brief decoder information
 */
typedef struct {
    TUYA_CODEC_ID_E codec_id;         ///< decoder codec id
    TUYA_DECODER_DESC_U decoder_desc; ///< decoder information
} TUYA_DECODER_T;

/** @struct STORAGE_FRAME_HEAD_T
 * @brief storage frame head
 */
typedef struct {
    UINT_T type;        ///< type
    UINT_T size;        ///< size
    UINT64_T timestamp; ///< timestamp
    UINT64_T pts;       ///< pts
} STORAGE_FRAME_HEAD_T;

/** @struct MEDIA_FRAME_T
 * @brief media frame
 */
typedef struct {
    MEDIA_FRAME_TYPE_E type; ///< frame type
    BYTE_T *p_buf;           ///< frame data buf
    UINT_T size;             ///< frame size
    UINT64_T pts;            ///< timestamp in us
    UINT64_T timestamp;      ///< timestamp is ms
} MEDIA_FRAME_T;

#ifdef __cplusplus
}
#endif

#endif /*_TUYA_IPC_MEDIA_H_*/
