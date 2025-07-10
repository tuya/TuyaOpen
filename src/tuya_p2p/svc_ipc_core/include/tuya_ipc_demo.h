#ifndef __TUYA_IPC_DEMO_H__
#define __TUYA_IPC_DEMO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_ipc_p2p.h"

void tuya_ipc_demo_start();
void tuya_ipc_demo_end();
INT_T Demo_OnSignalDisconnectCallback();
INT_T Demo_OnGetVideoFrameCallback(MEDIA_FRAME *pMediaFrame);
INT_T Demo_OnGetAudioFrameCallback(MEDIA_FRAME *pMediaFrame);

#ifdef __cplusplus
}
#endif

#endif /*_TUYA_IPC_DEMO_H_*/
