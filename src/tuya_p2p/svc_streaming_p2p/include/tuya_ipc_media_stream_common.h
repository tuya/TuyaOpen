#ifndef __TUYA_IPC_MEDIA_STREAM_COMMON_H__
#define __TUYA_IPC_MEDIA_STREAM_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

#ifdef IPC_CHANNEL_NUM
#define P2P_IPC_CHAN_NUM IPC_CHANNEL_NUM
#else
#define P2P_IPC_CHAN_NUM 1
#endif

#define TUYA_CMD_CHANNEL    (0) //信令通道，信令模式参照P2P_CMD_E
#define TUYA_VDATA_CHANNEL  (1) //视频数据通道
#define TUYA_ADATA_CHANNEL  (2) //音频数据通道
#define TUYA_TRANS_CHANNEL  (3) //录像下载
#define TUYA_TRANS_CHANNEL4 (4) //弃用，因APP端被天视通占用
#define TUYA_TRANS_CHANNEL5 (5) //相册功能下载

#if defined(ENABLE_IPC_P2P)
#define TUYA_CHANNEL_MAX (6) // NOTICE：内存不足时可裁剪
#elif defined(ENABLE_XVR_P2P)
#define TUYA_CHANNEL_MAX (200) // NOTICE：内存不足时可裁剪
#else
#define TUYA_CHANNEL_MAX (6) // NOTICE：内存不足时可裁剪
#endif

#define P2P_WR_BF_MAX_SIZE      (128 * 1024) //读写缓冲区最大大小，超过对应阈值，则不发送数据
#define P2P_SEND_REDUNDANCE_LEN (1250 + 100) //冗余长度

#ifdef __cplusplus
}
#endif

#endif /*_TUYA_IPC_MEDIA_STREAM_COMMON_H_*/
