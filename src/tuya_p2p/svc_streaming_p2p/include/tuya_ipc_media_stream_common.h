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

#define TUYA_CMD_CHANNEL    (0) // Signaling channel, signal mode refer to P2P_CMD_E
#define TUYA_VDATA_CHANNEL  (1) // Video data channel
#define TUYA_ADATA_CHANNEL  (2) // Audio data channel
#define TUYA_TRANS_CHANNEL  (3) // Video download
#define TUYA_TRANS_CHANNEL4 (4) // Deprecated, occupied by TianShiTong on APP side
#define TUYA_TRANS_CHANNEL5 (5) // Album function download

#if defined(ENABLE_IPC_P2P)
#define TUYA_CHANNEL_MAX (6) // NOTICE: Can be reduced when memory is insufficient
#elif defined(ENABLE_XVR_P2P)
#define TUYA_CHANNEL_MAX (200) // NOTICE: Can be reduced when memory is insufficient
#else
#define TUYA_CHANNEL_MAX (6) // NOTICE: Can be reduced when memory is insufficient
#endif

#define P2P_WR_BF_MAX_SIZE      (128 * 1024) // Maximum size of read/write buffer, no data sent when exceeding threshold
#define P2P_SEND_REDUNDANCE_LEN (1250 + 100) // Redundant length

#ifdef __cplusplus
}
#endif

#endif /*_TUYA_IPC_MEDIA_STREAM_COMMON_H_*/
