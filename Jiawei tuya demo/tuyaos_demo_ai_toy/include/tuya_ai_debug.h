#ifndef __TUYA_AI_DEBUG_H__
#define __TUYA_AI_DEBUG_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TUYA_UPLOAD_DEBUG 0
#define TY_AI_UPLOAD_CLOUD_IGNORED 0

typedef struct {
    UINT8_T *buf;
    UINT_T len;
} TY_AI_AUDIO_FRAME_T;

typedef enum {
    DEBUG_UPLOAD_STREAM_TYPE_RAW = 0,
    DEBUG_UPLOAD_STREAM_TYPE_MIC = 1,
    DEBUG_UPLOAD_STREAM_TYPE_REF = 2,
    DEBUG_UPLOAD_STREAM_TYPE_AEC = 3,
    _DEBUG_UPLOAD_STREAM_TYPE_MAX,
} DEBUG_UPLOAD_STREAM_TYPE;

#define DEBUG_UPLOAD_STREAM_TYPE_MAX (_DEBUG_UPLOAD_STREAM_TYPE_MAX - 1)

/**
 * @brief 将音频数据写入环形缓冲区
 *
 * @param[in] type 音频流类型
 * @param[in] buf 数据缓冲区
 * @param[in] len 数据长度
 *
 * @return 写入的数据长度
 */
INT_T ty_debug_audio_stream_write(DEBUG_UPLOAD_STREAM_TYPE type, CHAR_T *buf, UINT_T len);

/**
 * @brief 从环形缓冲区读取音频数据
 *
 * @param[in] type 音频流类型
 * @param[out] buf 数据缓冲区
 * @param[in] len 要读取的长度
 *
 * @return 实际读取的数据长度
 */
INT_T ty_debug_audio_stream_read(DEBUG_UPLOAD_STREAM_TYPE type, CHAR_T *buf, UINT_T len);

/**
 * @brief 查看环形缓冲区中的数据但不取出
 *
 * @param[in] type 音频流类型
 * @param[out] buf 数据缓冲区
 * @param[in] len 要查看的长度
 *
 * @return 实际查看的数据长度
 */
INT_T ty_debug_audio_stream_peek(DEBUG_UPLOAD_STREAM_TYPE type, CHAR_T *buf, UINT_T len);

/**
 * @brief 清空指定类型的环形缓冲区
 *
 * @param[in] type 音频流类型
 *
 * @return OPERATE_RET
 */
OPERATE_RET ty_debug_audio_stream_clear(DEBUG_UPLOAD_STREAM_TYPE type);

/**
 * @brief 获取环形缓冲区中已使用的大小
 *
 * @param[in] type 音频流类型
 *
 * @return 已使用的大小
 */
INT_T ty_debug_audio_stream_get_size(DEBUG_UPLOAD_STREAM_TYPE type);

/**
 * @brief 建立TCP连接
 *
 * @return OPERATE_RET
 */
OPERATE_RET ty_debug_tcp_connect(VOID);

/**
 * @brief 发送TCP数据
 *
 * @param[in] type 音频流类型
 * @param[in] data 要发送的数据
 * @param[in] len 数据长度
 *
 * @return 发送的数据长度
 */
INT_T ty_debug_tcp_send(INT_T type, CHAR_T *data, INT_T len);

/**
 * @brief 关闭所有TCP连接
 *
 * @return OPERATE_RET
 */
OPERATE_RET ty_debug_tcp_close_all(VOID);

/**
 * @brief 关闭指定类型的TCP连接
 *
 * @param[in] type 音频流类型
 *
 * @return OPERATE_RET
 */
OPERATE_RET ty_debug_tcp_close(INT_T type);

OPERATE_RET ty_debug_init(VOID);

/**
 * @brief 开始上传回调函数
 *
 * @return OPERATE_RET
 */
OPERATE_RET ty_debug_upload_start_cb(VOID);

/**
 * @brief 数据上传回调函数
 *
 * @param[in] buf 数据缓冲区
 * @param[in] len 数据长度
 *
 * @return OPERATE_RET
 */
OPERATE_RET ty_debug_upload_data_cb(CHAR_T *buf, UINT_T len);

/**
 * @brief 停止上传回调函数
 *
 * @return OPERATE_RET
 */
OPERATE_RET ty_debug_upload_stop_cb(VOID);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_AI_DEBUG_H__ */
