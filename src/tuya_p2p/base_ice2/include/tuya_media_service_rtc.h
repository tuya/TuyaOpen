//
//  ====== token format ======
//  {
//      "username" : "12334939:mbzrxpgjys",
//      "password" : "adfsaflsjfldssia",
//      "ttl" : 86400,
//      "uris" : [
//          "turn:1.2.3.4:9991?transport=udp",
//          "turn:1.2.3.4:9992?transport=tcp",
//          "turns:1.2.3.4:443?transport=tcp"
//      ]
// }
#ifndef __TUYA_MEDIA_SERVICE_RTC_H__
#define __TUYA_MEDIA_SERVICE_RTC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define TUYA_P2P_ID_LEN_MAX         80
#define TUYA_P2P_CHANNEL_NUMBER_MAX 320
#define TUYA_P2P_SESSION_NUMBER_MAX 1024
#define TUYA_P2P_VIDEO_BITRATE_MIN  (600)
#define TUYA_P2P_VIDEO_BITRATE_MAX  (4000)

#define TUYA_P2P_ERROR_SUCCESSFUL                         0
#define TUYA_P2P_ERROR_NOT_INITIALIZED                    -1
#define TUYA_P2P_ERROR_ALREADY_INITIALIZED                -2
#define TUYA_P2P_ERROR_TIME_OUT                           -3
#define TUYA_P2P_ERROR_INVALID_ID                         -4
#define TUYA_P2P_ERROR_INVALID_PARAMETER                  -5
#define TUYA_P2P_ERROR_DEVICE_NOT_ONLINE                  -6
#define TUYA_P2P_ERROR_FAIL_TO_RESOLVE_NAME               -7
#define TUYA_P2P_ERROR_INVALID_PREFIX                     -8
#define TUYA_P2P_ERROR_ID_OUT_OF_DATE                     -9
#define TUYA_P2P_ERROR_NO_RELAY_SERVER_AVAILABLE          -10
#define TUYA_P2P_ERROR_INVALID_SESSION_HANDLE             -11
#define TUYA_P2P_ERROR_SESSION_CLOSED_REMOTE              -12
#define TUYA_P2P_ERROR_SESSION_CLOSED_TIMEOUT             -13
#define TUYA_P2P_ERROR_SESSION_CLOSED_CALLED              -14
#define TUYA_P2P_ERROR_REMOTE_SITE_BUFFER_FULL            -15
#define TUYA_P2P_ERROR_USER_LISTEN_BREAK                  -16
#define TUYA_P2P_ERROR_MAX_SESSION                        -17
#define TUYA_P2P_ERROR_UDP_PORT_BIND_FAILED               -18
#define TUYA_P2P_ERROR_USER_CONNECT_BREAK                 -19
#define TUYA_P2P_ERROR_SESSION_CLOSED_INSUFFICIENT_MEMORY -20
#define TUYA_P2P_ERROR_INVALID_APILICENSE                 -21
#define TUYA_P2P_ERROR_FAIL_TO_CREATE_THREAD              -22
#define TUYA_P2P_ERROR_OUT_OF_SESSION                     -23
#define TUYA_P2P_ERROR_INVALID_PRE_SESSION                -24
#define TUYA_P2P_ERROR_PRE_SESSION_NOT_CONNECTED          -25
#define TUYA_P2P_ERROR_PRE_SESSION_ALREADY_ACTIVE         -26
#define TUYA_P2P_ERROR_PRE_SESSION_NOT_ACTIVE             -27
#define TUYA_P2P_ERROR_PRE_SESSION_SUSPENDED              -28
#define TUYA_P2P_ERROR_OUT_OF_MEMORY                      -29
#define TUYA_P2P_ERROR_HTTP_FAILED                        -30
#define TUYA_P2P_ERROR_PRECONNECT_UNSUPPORTED             -31
#define TUYA_P2P_ERROR_DTLS_HANDSHAKE_FAILED_FINGERPRINT  -32
#define TUYA_P2P_ERROR_GET_TOKEN_TIMEOUT                  -33
#define TUYA_P2P_ERROR_AUTH_FAILED                        -34
#define TUYA_P2P_ERROR_INIT_MBEDTLS_MD_AND_AES_FAILED     -35
#define TUYA_P2P_ERROR_HEARTBEAT_TIMEOUT                  -36
#define TUYA_P2P_ERROR_DTLS_HANDSHAKE_FAILED              -37
#define TUYA_P2P_ERROR_DTLS_HANDSHAKE_TIMEOUT             -38
#define TUYA_P2P_ERROR_REMOTE_NO_RESPONSE                 -39
#define TUYA_P2P_ERROR_PRE_SESSION_RESERVE_TIMEOUT        -40
#define TUYA_P2P_ERROR_RESET                              -41
#define TUYA_P2P_ERROR_UV_TIMER_INIT_FAILED               -42
#define TUYA_P2P_ERROR_SDP_INIT_FAILED                    -43
#define TUYA_P2P_ERROR_SDP_ADD_MEDIA_FAILED               -44
#define TUYA_P2P_ERROR_SDP_ADD_CODEC_FAILED               -45
#define TUYA_P2P_ERROR_CHANNEL_INIT_FAILED                -46
#define TUYA_P2P_ERROR_INVALID_AES_KEY                    -47
#define TUYA_P2P_ERROR_SDP_SET_AES_KEY_FAILED             -48
#define TUYA_P2P_ERROR_INVALID_TOKEN                      -49
#define TUYA_P2P_ERROR_TIME_OUT_NO_ANSWER                 -50
#define TUYA_P2P_ERROR_TIME_OUT_LOCAL_NO_HOST_CAND        -51
#define TUYA_P2P_ERROR_TIME_OUT_LOCAL_NAT                 -52
#define TUYA_P2P_ERROR_TIME_OUT_REMOTE_NAT                -53
#define TUYA_P2P_ERROR_TIME_COWBOY_NO_RESPONSE            -54
#define TUYA_P2P_ERROR_DEVICE_SECRET_MODE                 -102
#define TUYA_P2P_ERROR_DEVICE_CREATE_SEND_THREAD_FAILED   -103
#define TUYA_P2P_ERROR_DEVICE_OUT_OF_SESSION              -104
#define TUYA_P2P_ERROR_DEVICE_AUTH_FAILED                 -105
#define TUYA_P2P_ERROR_DEVICE_SESSION_CLOSED              -106
#define TUYA_P2P_ERROR_DEVICE_CREATE_WEBRTC_THREAD_FAILED -107
#define TUYA_P2P_ERROR_DEVICE_ZOMBI_SESSION               -108
#define TUYA_P2P_ERROR_DEVICE_USER_CLOSE                  -109
#define TUYA_P2P_ERROR_DEVICE_USER_EXIT                   -110
#define TUYA_P2P_ERROR_DEVICE_IN_SECRET_MODE              -111
#define TUYA_P2P_ERROR_DEVICE_CALLING                     -113
#define TUYA_P2P_ERROR_DEVICE_WEBRTC_EXIT                 -300
#define TUYA_P2P_ERROR_REMOTE                             -100

typedef enum tuya_p2p_rtc_security_level {
    TUYA_P2P_SECURITY_LEVEL_0,
    TUYA_P2P_SECURITY_LEVEL_1,
    TUYA_P2P_SECURITY_LEVEL_2,
    TUYA_P2P_SECURITY_LEVEL_3,
    TUYA_P2P_SECURITY_LEVEL_4,
    TUYA_P2P_SECURITY_LEVEL_5,
    /*-----------------------*/
    TUYA_P2P_SECURITY_LEVEL_MAX
} tuya_p2p_rtc_security_level_e;

typedef enum tuya_p2p_rtc_log_level {
    TUYA_P2P_LOG_TRACE,
    TUYA_P2P_LOG_DEBUG,
    TUYA_P2P_LOG_INFO,
    TUYA_P2P_LOG_WARN,
    TUYA_P2P_LOG_ERROR,
    TUYA_P2P_LOG_FATAL
} tuya_p2p_rtc_log_level_e;

typedef enum {
    tuya_p2p_rtc_frame_type_audio,
    tuya_p2p_rtc_frame_type_video_p,
    tuya_p2p_rtc_frame_type_video_i
} tuya_p2p_rtc_frame_type_e;

typedef enum tuya_p2p_rtc_connection_type {
    tuya_p2p_rtc_connection_type_p2p,
    tuya_p2p_rtc_connection_type_webrtc
} tuya_p2p_rtc_connection_type_e;

typedef enum {
    tuya_p2p_rtc_upnp_port_protocol_udp,
    tuya_p2p_rtc_upnp_port_protocol_tcp
} tuya_p2p_rtc_upnp_port_protocol;

#define TUYA_P2P_UPNP_ADDRESS_LENGTH  16 // IP地址长度
#define TUYA_P2P_UPNP_PROTOCOL_LENGTH 4  //协议长度 tcp or udp

typedef struct _tuya_p2p_rtc_upnp_port_link_t {
    char protocol[TUYA_P2P_UPNP_PROTOCOL_LENGTH];
    int external_port;
    char remount_host[TUYA_P2P_UPNP_ADDRESS_LENGTH];
    int internal_port;
    char internal_client[TUYA_P2P_UPNP_ADDRESS_LENGTH];
    int route_level;
    int index;
    struct _tuya_p2p_rtc_upnp_port_link_t *next;
} tuya_p2p_rtc_upnp_port_link_t;

typedef struct tuya_p2p_rtc_audio_codec {
    char name[64];
    int sample_rate;
    int channel_number;
} tuya_p2p_rtc_audio_codec_t;

typedef struct tuya_p2p_rtc_video_codec {
    char name[64];
    int clock_rate;
} tuya_p2p_rtc_video_codec_t;

typedef struct {
    char *buf;
    uint32_t size;
    uint32_t len;
    uint64_t pts;
    uint64_t timestamp;
    tuya_p2p_rtc_frame_type_e frame_type;
} tuya_p2p_rtc_frame_t;

typedef enum rtc_state {
    RTC_STATE_GET_TOKEN,
    RTC_STATE_P2P_CONNECT,
    RTC_STATE_DTLS_SRTP_KEY_NEGO,
    RTC_STATE_STREAM,
    RTC_STATE_FAILED,
    RTC_STATE_NUMBER,
} rtc_state_e;

typedef enum { RTC_PRE_NOT_ACTIVE = 0, RTC_PRE_ACTIVATING, RTC_PRE_ACTIVE, RTC_PRE_SUSPENDING } rtc_active_state_e;

typedef struct {
    int32_t handle;
    int32_t is_pre;
    rtc_state_e state;
    rtc_active_state_e active_state;
    tuya_p2p_rtc_connection_type_e connection_type;
    tuya_p2p_rtc_audio_codec_t audio_codec;
    tuya_p2p_rtc_video_codec_t video_codec;
    char trace_id[128];
    char session_id[64];
    char sub_dev_id[64];
    int32_t stream_type; //码流类型 0 -- 主码流 1 -- 子码流
    int32_t is_replay;
    char start_time[32];
    char end_time[32];
} tuya_p2p_rtc_session_info_t;

// tuya p2p sdk 依赖几个外部服务，通过回调或接口实现：
//  1. 信令发送
//      当 tuya p2p sdk 需要发送信令时，会调用上层实现的 tuya_p2p_rtc_signaling_cb_t 进行发送
//  2. 信令接收
//      当上层收到信令时，通过 tuya_p2p_rtc_set_signaling 接口设置到 tuya p2p sdk
//  3. HTTP 服务
//      当 tuya p2p sdk 需要发送日志时，会调用上层实现的 tuya_p2p_rtc_log_cb_t 进行发送

// 信令回调，用于发送信令
// remote_id: 表示向 remote_id 发送信令
// signaling: 信令内容的起始地址
// len: 信令内容的长度
typedef void (*tuya_p2p_rtc_signaling_cb_t)(char *remote_id, char *signaling, uint32_t len);

// 日志回调，用于发送日志
// log: 日志内容的起始地址
// len: 日志内容的长度
typedef void (*tuya_p2p_rtc_log_cb_t)(int level, char *log, uint32_t len);
typedef int (*tuya_p2p_rtc_log_get_level_cb_t)();

// 鉴权回调，用于验证offer是否合法
// 用 hmac-sha256 对 buf 内容计算 hash，再对 hash 结果计算 base64 编码，
// 计算方法：base64(hmac-sha256(key=password, content=buf, length=len))
// 将结果跟 md 比较, 相同返回 0， 不同返回 -1
typedef int32_t (*tuya_p2p_rtc_auth_cb_t)(char *buf, uint32_t len, char *md, uint32_t md_len);

// http回调，用于发送http请求
// api: http接口名称
// devId: 设备编号
// content: 请求内容
// content_len: content长度
typedef int32_t (*tuya_p2p_rtc_http_cb_t)(char *api, char *devId, char *content, uint32_t content_len);

// crypt callback function
// mode: "aes", "ecb", ...
// crypt: "encrypt", "decrypt"
typedef int (*tuya_p2p_rtc_aes_create_cb_t)(void **handle, char *mode, char *crypt, char *key, int key_bits);
typedef int (*tuya_p2p_rtc_aes_destroy_cb_t)(void *handle);
typedef int (*tuya_p2p_rtc_aes_encrypt_cb_t)(void *handle, int length, char *iv, char *input, char *output);
typedef int (*tuya_p2p_rtc_aes_decrypt_cb_t)(void *handle, int length, char *iv, char *input, char *output);

// upnp callback functions
typedef int (*tuya_p2p_rtc_upnp_alloc_port_cb_t)(tuya_p2p_rtc_upnp_port_protocol protocol, int *local_port,
                                                 char *address, int *port);
typedef int (*tuya_p2p_rtc_upnp_release_port_cb_t)(tuya_p2p_rtc_upnp_port_protocol protocol, int local_port);
typedef int (*tuya_p2p_rtc_upnp_bind_result_cb_t)(tuya_p2p_rtc_upnp_port_protocol protocol, int local_port,
                                                  int error_code);
typedef tuya_p2p_rtc_upnp_port_link_t *(*tuya_p2p_rtc_upnp_request_port_list_cb_t)(
    tuya_p2p_rtc_upnp_port_protocol protocol, unsigned int port);

// session state callback
typedef int (*tuya_p2p_rtc_session_state_cb_t)(char *remote_id, int handle, int is_pre, rtc_state_e state,
                                               rtc_active_state_e active_state, int error);

// localhost callback
// addresses size is 1024
typedef int (*tuya_p2p_rtc_session_get_address_cb_t)(char *address);

typedef struct tuya_p2p_rtc_cb {
    tuya_p2p_rtc_signaling_cb_t on_signaling;         // 信令回调，应用层通过 mqtt 发送信令
    tuya_p2p_rtc_signaling_cb_t on_moto_signaling;    // 信令回调，应用层通过 moto 发送信令
    tuya_p2p_rtc_signaling_cb_t on_lan_signaling;     // 信令回调，应用层通过局域网信令通道发送信令
    tuya_p2p_rtc_log_cb_t on_log;                     // 日志回调，应用层将日志内容上报云端
    tuya_p2p_rtc_log_get_level_cb_t on_log_get_level; // 日志回调，应用层将日志内容上报云端
    tuya_p2p_rtc_auth_cb_t on_auth;                   // 鉴权回调，设备端使用
    tuya_p2p_rtc_http_cb_t on_http; // http回调，app端使用，当tuya p2p sdk 需要http接口时调用
    struct {
        // 外部加解密接口，如不需要请设置为 NULL
        tuya_p2p_rtc_aes_create_cb_t on_create;
        tuya_p2p_rtc_aes_destroy_cb_t on_destroy;
        tuya_p2p_rtc_aes_encrypt_cb_t on_encrypt;
        tuya_p2p_rtc_aes_decrypt_cb_t on_decrypt;
    } aes;
    struct {
        tuya_p2p_rtc_upnp_alloc_port_cb_t on_alloc;                    //获取UPN映射的端口信息
        tuya_p2p_rtc_upnp_release_port_cb_t on_release;                //释放UPNP映射的端口
        tuya_p2p_rtc_upnp_bind_result_cb_t on_bind;                    //将绑定映射端口结果回馈给UPNP模组
        tuya_p2p_rtc_upnp_request_port_list_cb_t on_request_port_list; //获取多级路由映射的地址及端口信息
    } upnp;

    tuya_p2p_rtc_session_state_cb_t on_session_state;
    tuya_p2p_rtc_session_get_address_cb_t on_get_address;
} tuya_p2p_rtc_cb_t;

// p2p sdk 初始化参数
typedef struct tuya_p2p_rtc_options {
    char local_id[TUYA_P2P_ID_LEN_MAX];                  // 本地 id， 设备端填设备 id，客户端填 uid
    tuya_p2p_rtc_cb_t cb;                                // 回调接口
    uint32_t max_channel_number;                         // 每个连接最大的通道数
    uint32_t max_session_number;                         // 允许同时发起或接收几个连接
    uint32_t max_pre_session_number;                     // 允许同时发起或接收几个预连接
    uint32_t send_buf_size[TUYA_P2P_CHANNEL_NUMBER_MAX]; // 各个通道的发送缓冲区大小，单位字节
    uint32_t recv_buf_size[TUYA_P2P_CHANNEL_NUMBER_MAX]; // 各个通道的接收缓冲区大小，单位字节
    uint32_t video_bitrate_kbps;                         // 设备端填写视频的码率，客户端不需要设置
    uint32_t preconnect_enable;                          // 是否开启预连接，1:开启，0:关闭
    uint32_t fragement_len;                              // 数据发送接口分包长度
} tuya_p2p_rtc_options_t;

// 获取 p2p sdk 版本号
uint32_t tuya_p2p_rtc_get_version();
// 获取 p2p sdk 能力集
uint32_t tuya_p2p_rtc_get_skill();
// p2p sdk 初始化
int32_t tuya_p2p_rtc_init(tuya_p2p_rtc_options_t *opt);
// p2p sdk 反初始化
int32_t tuya_p2p_rtc_deinit();
int32_t tuya_p2p_rtc_reset(tuya_p2p_rtc_options_t *opt);
// 向 p2p sdk 发送信令，应用层收到 p2p 信令时，调用该接口
// remote_id: 暂未使用
// msg: 信令内容，json 格式的字符串
// msglen: 信令长度
// 返回值: 0
int32_t tuya_p2p_rtc_set_signaling(char *remote_id, char *msg, uint32_t msglen);
// 向对方发起预连接
// remote_id: 接收mqtt信令的id
// dev_id: 设备id
// 返回值: 0
int32_t tuya_p2p_rtc_pre_connect(char *remote_id, char *dev_id);
// 关闭预连接
// remote_id: 接收mqtt信令的id
// 返回值: 0
int32_t tuya_p2p_rtc_pre_connect_close(char *remote_id, int32_t reason);
// 向对方发起连接，atop接口由p2p库触发，在connect后调用
// remote_id: 接收mqtt信令的id
// dev_id: 设备id
// skill: 设备能力集，从云端获取到的 skill 字段，json 格式的字符串
// skill_len: skill的长度
// token: 从云端获取到的 p2pConfig 字段，json 格式的字符串
// token_len: token 的长度
// trace_id: 全链路日志的 id，可以为空
// lan_mode: 局域网优先模式
// timeout_ms: 连接超时时间，一般是 15s
// 返回值：< 0 表示失败，>=0 表示连接成功，意义跟 socket fd 相似
int32_t tuya_p2p_rtc_connect_v2(char *remote_id, char *dev_id, char *skill, uint32_t skill_len, char *token,
                                uint32_t token_len, char *trace_id, int lan_mode, int timeout_ms);
// 向对方发起连接
// remote_id: 对方的 id，一般是设备id
// token: 从云端获取到的 p2pConfig 字段，json 格式的字符串
// token_len: token 的长度
// trace_id: 全链路日志的 id，可以为空
// lan_mode: 局域网优先模式
// timeout_ms: 连接超时时间，一般是 15s
// 返回值：< 0 表示失败，>=0 表示连接成功，意义跟 socket fd 相似
int32_t tuya_p2p_rtc_connect(char *remote_id, char *token, uint32_t token_len, char *trace_id, int lan_mode,
                             int timeout_ms);
// 取消正在尝试的连接，调用该接口后，tuya_p2p_rtc_connect 接口立刻返回失败；
// 已经建立成功的连接不受影响
int32_t tuya_p2p_rtc_connect_break();
// 取消正在尝试的指定连接，调用该接口后，tuya_p2p_rtc_connect 接口立刻返回失败；
// 已经建立成功的连接不受影响
// trace_id: 全链路日志的 id，指定要取消的连接
int32_t tuya_p2p_rtc_connect_break_one(char *trace_id);
// http响应
// api: http接口名称
// code: http响应状态码
// result: http响应内容
// result_len: result长度
int32_t tuya_p2p_rtc_set_http_result(char *api, uint32_t code, char *result, uint32_t result_len);
// http响应
// api: http接口名称
// code: http响应状态码
// content: on_http请求内容
// content_len: content长度
// result: http响应内容
// result_len: result长度
int32_t tuya_p2p_rtc_set_http_result_v2(char *api, uint32_t code, char *dev_id, char *content, uint32_t content_len,
                                        char *result, uint32_t result_len);
// 接收一个连接，类似于 socket 的 accept 接口
// 返回值：< 0 表示失败，>= 0 表示成功
int32_t tuya_p2p_rtc_listen();
// 中断listen, 调用该接口后，tuya_p2p_rtc_listen 立刻返回失败
int32_t tuya_p2p_rtc_listen_break();
// 获取连接相关的信息，设备端使用，用于获取对应连接是 p2p 连接还是 webrtc 连接
int32_t tuya_p2p_rtc_get_session_info(int32_t handle, tuya_p2p_rtc_session_info_t *info);
// 获取会话列表
// 返回json格式的字符串，需调用tuya_p2p_rtc_free_session_list释放
char *tuya_p2p_rtc_get_session_list();
// 释放会话列表
// session_list: tuya_p2p_rtc_get_session_list的返回值
void tuya_p2p_rtc_free_session_list(char *session_list);
// 检查连接是否还活着
// handle：会话句柄
// 返回值：< 0 表示会话异常，0表示会话正常
int32_t tuya_p2p_rtc_check(int32_t handle);
// 关闭连接或挂起预连接
int32_t tuya_p2p_rtc_close(int32_t handle, int32_t reason);
// 发送音视频帧（webrtc 专用）
int32_t tuya_p2p_rtc_send_frame(int32_t handle, tuya_p2p_rtc_frame_t *frame);
// 发送音频帧（webrtc 专用）
int32_t tuya_p2p_rtc_recv_frame(int32_t handle, tuya_p2p_rtc_frame_t *frame);
// 向对方发送数据
// handle: 连接句柄
// channel_id: 通道号
// buf: 指向需要发送的内容
// len: 需要发送的数据长度
// timeout_ms: 接口阻塞时间，单位毫秒
// 返回值
// >=0: 发送成功的字节数
// TUYA_P2P_ERROR_TIME_OUT：发送超时
// 其他: 发送失败，并且连接已断开
int32_t tuya_p2p_rtc_send_data(int32_t handle, uint32_t channel_id, char *buf, int32_t len, int32_t timeout_ms);
// 接收数据
// handle: 连接句柄
// channel_id: 通道号
// buf: 接收缓冲区
// len: 接收缓冲区的长度
// timeout_ms: 接口阻塞时间，单位毫秒
// 返回值
// 0: 接收成功，*len 表示收到的字节数
// TUYA_P2P_ERROR_TIME_OUT：接收超时
// 其他：接收失败,并且连接已断开
int32_t tuya_p2p_rtc_recv_data(int32_t handle, uint32_t channel_id, char *buf, int32_t *len, int32_t timeout_ms);
// 检查某个连接当前收发缓冲区的情况：
// handle: 连接句柄
// channel_id: 通道号
// write_size: 函数返回后，更新为当前已写入发送缓冲区，但未发送成功的字节数
// read_size: 函数返回后，更新未当前已接收成功，但未被应用层读走的字节数
// send_free_size: 函数返回后，更新发送缓存区剩余空间
// 返回值：未定义
int32_t tuya_p2p_rtc_check_buffer(int32_t handle, uint32_t channel_id, uint32_t *write_size, uint32_t *read_size,
                                  uint32_t *send_free_size);
// 通知 p2p sdk 某个设备刚上线
// 主要用于低功耗设备
int32_t tuya_p2p_rtc_set_remote_online(char *remote_id);
int32_t tuya_p2p_getwaitsnd(int32_t handle, uint32_t channel_id);
int32_t tuya_p2p_log_set_level(tuya_p2p_rtc_log_level_e level);

#ifdef __cplusplus
}
#endif

#endif
