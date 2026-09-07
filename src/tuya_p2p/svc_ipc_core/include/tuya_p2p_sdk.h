#ifndef __TUYA_P2P_SDK_H__
#define __TUYA_P2P_SDK_H__
#include "cJSON.h"
#include "tuya_cloud_types.h"
#include "tuya_ipc_p2p.h"

typedef struct {
    char *pk;
    char *firmware_key;
    char *url;
    char *id; // devid
    char *uuid;
    char *hid;
    char *token;
    char *sw_ver;
    char *pv;
    char *bv;
    char *cad_ver;
    char *cd_ver;
    char *modules; // [{"type":3,online:true,"softVer":"1.0"}]
    char *feature; // user self define
    char *auth_key;
    char *options;
    char *dev_sw_ver; // no longer used after cad:1.0.4
} GW_ACTV_IN_PARM_V41_S;

typedef struct tagTuyaIpcSdkVar {
    int (*OnSignalDisconnectCallback)();
    int (*OnGetVideoFrameCallback)(MEDIA_FRAME *pMediaFrame);
    int (*OnGetAudioFrameCallback)(MEDIA_FRAME *pMediaFrame);
    int (*OnLiveVideoStartCallback)(void);
    int (*OnLiveVideoStopCallback)(void);
    /* Downlink intercom (APP -> device speaker), align TuyaOS speaker/recv_audio */
    int (*OnLiveAudioStartCallback)(void);
    int (*OnLiveAudioStopCallback)(void);
    int (*OnRecvAudioFrameCallback)(MEDIA_FRAME *pMediaFrame);

    int (*OnRequestIFrameCallback)(void);
    int (*OnSetVideoBitrateCallback)(uint32_t kbps);
} TUYA_IPC_SDK_VAR_S;

OPERATE_RET TUYA_APP_Start(TUYA_IPC_SDK_VAR_S *pSdkVar);
OPERATE_RET TUYA_APP_End();
OPERATE_RET OnIotInited();
char *gw_active_get_ext_param();
void gw_p2p_mqtt_data_cb(cJSON *root_json);

#endif
