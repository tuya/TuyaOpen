#ifndef __TUYA_IPC_P2P_INNER_H__
#define __TUYA_IPC_P2P_INNER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tuya_ipc_p2p_common.h"

// #define ERROR_P2P_SUCCESSFUL                        0           /** p2p operation success*/
// #define ERROR_P2P_NOT_INITIALIZED                  -1          /** p2p has not init*/
// #define ERROR_P2P_ALREADY_INITIALIZED              -2          /** p2p has inited*/
// #define ERROR_P2P_TIME_OUT                         -3          /** p2p has inited*/
// #define ERROR_P2P_INVALID_ID                       -4          /** p2p invalid id*/
// #define ERROR_P2P_INVALID_PARAMETER                -5          /** p2p invalid param*/
// #define ERROR_P2P_DEVICE_NOT_ONLINE                -6          /** device outline*/
// #define ERROR_P2P_FAIL_TO_RESOLVE_NAME             -7          /** p2p name err*/
// #define ERROR_P2P_INVALID_PREFIX                   -8          /** p2p prefix err*/
// #define ERROR_P2P_ID_OUT_OF_DATE                   -9          /** device outline*/
// #define ERROR_P2P_NO_RELAY_SERVER_AVAILABLE        -10         /** server no relay*/
// #define ERROR_P2P_INVALID_SESSION_HANDLE           -11         /** invalid session*/
// #define ERROR_P2P_SESSION_CLOSED_REMOTE            -12         /** remote close*/
// #define ERROR_P2P_SESSION_CLOSED_TIMEOUT           -13         /** close timeout*/
// #define ERROR_P2P_SESSION_CLOSED_CALLED            -14         /** close called*/
// #define ERROR_P2P_REMOTE_SITE_BUFFER_FULL          -15         /** remote buffer full*/
// #define ERROR_P2P_USER_LISTEN_BREAK                -16         /** listen break*/
// #define ERROR_P2P_MAX_SESSION                      -17         /** limit max session*/
// #define ERROR_P2P_UDP_PORT_BIND_FAILED             -18         /** port bind fail*/
// #define ERROR_P2P_USER_CONNECT_BREAK               -19         /** connect err*/
// #define ERROR_P2P_SESSION_CLOSED_INSUFFICIENT_MEMORY   -20     /** memory insufficent*/
// #define ERROR_P2P_INVALID_APILICENSE               -21         /** invalid apilicense*/
// #define ERROR_P2P_FAIL_TO_CREATE_THREAD            -22         /** create pthread fail*/

#define C2C_MAJOR_VERSION 1
#define C2C_MINOR_VERSION 2

// 涂鸦固定协议头
typedef struct {
    unsigned int type;       //请求类型 (0: request, 1:response)
    unsigned short high_cmd; //主命令参见 TY_MAIN_CMD_TYPE_E;
    unsigned short low_cmd;  //后2字节: 子命令
    unsigned int length;
} C2C_CMD_FIXED_HEADER_T;

typedef struct C2C_AV_TRANS_FIXED_HEADER_ {
    unsigned int request_id;
    unsigned int reserve1;
    unsigned long long int time_ms;
    int extension_length;
    unsigned int reserve2;
} C2C_AV_TRANS_FIXED_HEADER;

typedef enum {
    TY_EXT_VIDEO_PARAM = 0x01,
    TY_EXT_AUDIO_PARAM = 0x02,
} TY_AV_EXTENSION_TYPE_T;

// 视频播放命令[回放、直播]
// https://wiki.tuya-inc.com:7799/page/74675515
typedef enum {
    TY_CMD_IO_CTRL_VIDEO_PLAY,      // 0 开始
    TY_CMD_IO_CTRL_VIDEO_PAUSE,     // 1 暂停
    TY_CMD_IO_CTRL_VIDEO_RESUME,    // 2 继续回放，直播不用
    TY_CMD_IO_CTRL_VIDEO_STOP,      // 3 停止
    TY_CMD_IO_CTRL_AUDIO_MIC_START, // 4 IPC -> APP，开始伴音
    TY_CMD_IO_CTRL_AUDIO_MIC_STOP,  // 5 IPC -> APP，结束伴音

    TY_CMD_IO_CTRL_VIDEO_PLAY_V2 = 20,            // 20 开始
    TY_CMD_IO_CTRL_PLAYBACK_START_WITH_MODE = 21, // 21 回放开始支持播放模式
    TY_CMD_IO_CTRL_VIDEO_SEND_START = 50,         // 50 视频准备好发送
    TY_CMD_IO_CTRL_VIDEO_SEND_STOP = 51,          // 51 视频停止发送
    TY_CMD_IO_CTRL_AUDIO_SEND_START = 52,         // 52 音频准备好发送
    TY_CMD_IO_CTRL_AUDIO_SEND_STOP = 53,          // 53 音频停止发送
    TY_CMD_IO_CTRL_VIDEO_SEND_PAUSE = 54,         // 54 发送端暂停发送视频
    TY_CMD_IO_CTRL_VIDEO_SEND_RESUME = 55,        // 55 发送端恢复发送视频
} TY_CMD_IO_CTRL_VIDEO_E;

typedef struct {
    unsigned int channel;
    unsigned int operation; // 参见 TY_CMD_IO_CTRL_VIDEO_E
} C2C_TRANS_CTRL_VIDEO_REQ_T;

typedef struct {
    unsigned int channel;
    unsigned int operation; // 参见 TY_CMD_IO_CTRL_AUDIO_OP_E
} C2C_TRANS_CTRL_AUDIO_REQ_T;

typedef enum {
    TY_C2C_CMD_IO_CTRL_COMMAND_INVALID, // 0无效命令
    TY_C2C_CMD_IO_CTRL_COMMAND_RECV,    // 1命令已收到
    TY_C2C_CMD_IO_CTRL_COMMAND_FAILED,  // 2命令执行失败 (通话对讲：camera被其他人占据)
    TY_C2C_CMD_IO_CTRL_COMMAND_SUCCESS, // 3 命令已完成
    TY_C2C_CMD_IO_CTRL_COMMAND_BUSY, /* 4命令已完成，通话对讲：操作错误 自己本来就处于对讲状态
                                        设备端更新reqid，app端重新开启mic  */
} TY_C2C_CMD_IO_CTRL_STATUS_CODE_E;

//通用回复结构体
typedef struct {
    unsigned int channel;
    int result; // 参见 TY_C2C_CMD_IO_CTRL_STATUS_CODE_E
} C2C_CMD_IO_CTRL_COM_RESP_T;

// 音频 C2C_CMD_QUERY_AUDIO_PARAMS
// request
typedef struct {
    unsigned int channel;
} C2C_TRANS_QUERY_AUDIO_PARAM_REQ_T;

// response
typedef struct {
    unsigned int type;        // 参见 TY_AV_CODEC_ID
    unsigned int sample_rate; // 参见 TRANSFER_AUDIO_SAMPLE_E
    unsigned int bitwidth;    // 参见 TRANSFER_AUDIO_DATABITS_E
    unsigned int channel_num; // 参见 TRANSFER_AUDIO_CHANNEL_E
} AUDIO_PARAM_T;

typedef struct {
    unsigned int channel;
    unsigned int count;
    AUDIO_PARAM_T audioParams[0];
} C2C_TRANS_QUERY_AUDIO_PARAM_RESP_E;

typedef enum {
    TY_VIDEO_CLARITY_INNER_PROFLOW = 0x1,  /**< 省流量 */
    TY_VIDEO_CLARITY_INNER_STANDARD = 0x2, /**< 标清 */
    TY_VIDEO_CLARITY_INNER_HIGH = 0x4,     /**< 高清 */
    TY_VIDEO_CLARITY_S_INNER_HIGH = 0x8,   /**< 超清 */
    TY_VIDEO_CLARITY_SS_INNER_HIGH = 0x10, /**< 超超清 */
} TRANSFER_VIDEO_CLARITY_TYPE_INNER_E;

typedef enum {
    TY_AV_CODEC_VIDEO_UNKOWN = 0,
    TY_AV_CODEC_VIDEO_MPEG4 = 0x10,
    TY_AV_CODEC_VIDEO_H263 = 0x11,
    TY_AV_CODEC_VIDEO_H264 = 0x12,
    TY_AV_CODEC_VIDEO_MJPEG = 0x13,
    TY_AV_CODEC_VIDEO_H265 = 0x14,

    TY_AV_CODEC_AUDIO_ADPCM = 0x80,
    TY_AV_CODEC_AUDIO_PCM = 0x81,
    TY_AV_CODEC_AUDIO_AAC_RAW = 0x82,
    TY_AV_CODEC_AUDIO_AAC_ADTS = 0x83,
    TY_AV_CODEC_AUDIO_AAC_LATM = 0x84,
    TY_AV_CODEC_AUDIO_G711U = 0x85, // 10
    TY_AV_CODEC_AUDIO_G711A = 0x86,
    TY_AV_CODEC_AUDIO_G726 = 0x87,
    TY_AV_CODEC_AUDIO_SPEEX = 0x88,
    TY_AV_CODEC_AUDIO_MP3 = 0x89,

    TY_AV_CODEC_MAX = 0xFF
} TY_AV_CODEC_ID;

typedef struct {
    //视频部分参数
    TY_AV_CODEC_ID video_codec[8];
    UINT_T fps[8];
    UINT_T gop[8];
    UINT_T bitrate[8]; // kbps
    UINT_T width[8];
    UINT_T height[8];
    //音频部分参数
    TY_AV_CODEC_ID audio_codec;
    TRANSFER_AUDIO_SAMPLE_E audio_sample;
    TRANSFER_AUDIO_DATABITS_E audio_databits;
    TRANSFER_AUDIO_CHANNEL_E audio_channel;
} TRANS_IPC_AV_INFO_T;

// request 视频清晰度查询
typedef struct {
    unsigned int channel;
} C2C_TRANS_QUERY_VIDEO_CLARITY_REQ_T;

// response
typedef struct {
    unsigned int channel;
    unsigned int sp_mode;  //支持的清晰度模式
    unsigned int cur_mode; //当前清晰度，参见TRANSFER_VIDEO_CLARITY_TYPE_INNER_E
} C2C_TRANS_QUERY_VIDEO_CLARITY_RESP_T;

// 设置清晰度
typedef struct {
    unsigned int channel;
    unsigned int mode; //清晰度，参见TRANSFER_VIDEO_CLARITY_TYPE_INNER_E
} C2C_TRANS_CTRL_VIDEO_CLARITY_T;

// 视频流信息参数 C2C_CMD_QUERY_VIDEO_STREAM_PARAMS
// request
typedef struct {
    unsigned int channel;
} C2C_TRANS_QUERY_VIDEO_PARAM_REQ_T;

// response
typedef struct {
    unsigned int codec_type; // 参见 TY_AV_CODEC_ID
    unsigned int width;
    unsigned int height;
    unsigned int frame_rate;
} VIDEO_PARAM_T;

typedef struct {
    unsigned int channel;
    unsigned int count;
    VIDEO_PARAM_T VideoParams[0];
} C2C_TRANS_QUERY_VIDEO_PARAM_RESP_T;

typedef struct {
    unsigned int version; //高位主版本号，低16位次版本号
} C2C_CMD_PROTOCOL_VERSION_T;

typedef enum {
    // 透传
    TY_C2C_CMD_QUERY_TEXT, // 客户端向设备端透传字符串. [拓展某些功能时，避免重编sdk]

    // 查询类
    TY_C2C_CMD_QUERY_FIXED_ABILITY, // 1, 查询设备能力集，子类型见 TY_CMD_QUERY_IPC_FIXED_ABILITY_TYPE_E
    TY_C2C_CMD_QUERY_AUDIO_PARAMS,  // 2, 查询音频参数，音频信息类型见 TY_CMD_QUERY_AUDIO_PARAMS
    TY_C2C_CMD_QUERY_PLAYBACK_INFO, // 3, 查询SD卡回放信息，参数见
    TY_C2C_CMD_QUERY_VIDEO_STREAM_PARAMS, // 4, 查询视频模式信息 { [通道号,高清/标准/流畅,宽高,编码类型],
                                          // [通道号,高清/标准/流畅,宽高,编码类型],
                                          // [通道号,高清/标准/流畅,宽高,编码类型] }
    TY_C2C_CMD_QUERY_VIDEO_CLARITY, // 5, 查询清晰度

    // IO控制类
    TY_C2C_CMD_IO_CTRL_VIDEO,         // 6, 直播命令，子类型见 TY_CMD_IO_CTRL_VIDEO_E
    TY_C2C_CMD_IO_CTRL_PLAYBACK,      // 7, 回放命令，子类型见 TY_CMD_IO_CTRL_VIDEO_E
    TY_C2C_CMD_IO_CTRL_AUDIO,         // 8, 音频命令，子类型见 TY_CMD_IO_CTRL_AUDIO
    TY_C2C_CMD_IO_CTRL_VIDEO_CLARITY, // 9, 设置清晰度

    TY_C2C_CMD_PROTOCOL_VERSION, // 10, 协议版本
    // for download for feit
    TY_C2C_CMD_IO_CTRL_PLAYBACK_DOWNLOAD, // 11 下载命令，子类型见 TY_CMD_IO_CTRL_DOWNLOAD_OP_E

    // for camera of GW
    TY_C2C_CMD_QUERY_AUDIO_PARAMS_GW, // 12, 查询音频参数，音频信息类型见 TY_CMD_QUERY_AUDIO_PARAMS .for camera of GW
    TY_C2C_CMD_QUERY_PLAYBACK_INFO_GW, // 13, 查询SD卡回放信息，参数见
    TY_C2C_CMD_QUERY_VIDEO_STREAM_PARAMS_GW, // 14, 查询视频模式信息 { [通道号,高清/标准/流畅,宽高,编码类型],
                                             // [通道号,高清/标准/流畅,宽高,编码类型],
                                             // [通道号,高清/标准/流畅,宽高,编码类型] }
    TY_C2C_CMD_QUERY_VIDEO_CLARITY_GW,   // 15, 查询清
    TY_C2C_CMD_IO_CTRL_VIDEO_GW,         // 16, 直播命令，子类型见 TY_CMD_IO_CTRL_VIDEO_E
    TY_C2C_CMD_IO_CTRL_PLAYBACK_GW,      // 17, 回放命令，子类型见 TY_CMD_IO_CTRL_VIDEO_E
    TY_C2C_CMD_IO_CTRL_AUDIO_GW,         // 18, 音频命令，子类型见 TY_CMD_IO_CTRL_AUDIO
    TY_C2C_CMD_IO_CTRL_VIDEO_CLARITY_GW, // 19, 设置清晰度
    TY_C2C_CMD_QUERY_FIXED_ABILITY_GW, // 20, 查询设备能力集，子类型见 TY_CMD_QUERY_IPC_FIXED_ABILITY_TYPE_E

    TY_C2C_CMD_CHAN_SWITCH = 51,               // 51 一路多通道设备设置通道
    TY_C2C_CMD_IO_CTRL_PLAYBACK_EXT0 = 100,    // 100 回放速度控制 扩展
    TY_C2C_CMD_IO_CTRL_PLAYBACK_GW_EXT0 = 101, // 101 回放速度控制  扩展基站
    // for p2p new authorization
    TY_C2C_CMD_AUTHORIZATION = 250,
    TY_C2C_CMD_SUB_BINDS_INFO = 300,
} TY_MAIN_CMD_TYPE_E;

typedef enum tagTransferVideoClarityType {
    eVideoClarityStandard = 0,
    eVideoClarityHigh,
    eVideoClarityThird,
    eVideoClarityFourth,
    eVideoClarityMax
} TRANSFER_VIDEO_CLARITY_TYPE;

typedef enum tagIpcStreamType {
    eIpcStreamVideoMain, ///< first video stream
    eIpcStreamVideoSub,  ///< second video stream
    eIpcStreamVideo3rd,  ///< third video stream
    eIpcStreamVideo4th,  ///< forth video stream
    eIpcStreamVideoMax = 8,
    eIpcStreamAudioMain, ///< first audio stream
    eIpcStreamAudioSub,  ///< second audio stream
    eIpcStreamAudio3rd,  ///< third audio stream
    eIpcStreamAudio4th,  ///< forth audio stream
    eIpcStreamMax = 16,
} IPC_STREAM_TYPE;

#ifdef __cplusplus
}
#endif

#endif