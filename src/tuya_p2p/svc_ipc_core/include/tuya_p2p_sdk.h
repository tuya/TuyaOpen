#ifndef __TUYA_P2P_SDK_H__
#define __TUYA_P2P_SDK_H__
#include "cJSON.h"
#include "tuya_cloud_types.h"
#include "tuya_ipc_p2p.h"

typedef struct {
    CHAR_T *pk;
    CHAR_T *firmware_key;
    CHAR_T *url;
    CHAR_T *id; // devid
    CHAR_T *uuid;
    CHAR_T *hid;
    CHAR_T *token;
    CHAR_T *sw_ver;
    CHAR_T *pv;
    CHAR_T *bv;
    CHAR_T *cad_ver;
    CHAR_T *cd_ver;
    CHAR_T *modules; // [{"type":3,online:true,"softVer":"1.0"}]
    CHAR_T *feature; // user self define
    CHAR_T *auth_key;
    CHAR_T *options;
    CHAR_T *dev_sw_ver; // no longer used after cad:1.0.4
} GW_ACTV_IN_PARM_V41_S;

typedef struct tagTuyaIpcSdkVar {
    INT_T (*OnSignalDisconnectCallback)();
    INT_T (*OnGetVideoFrameCallback)(MEDIA_FRAME *pMediaFrame);
    INT_T (*OnGetAudioFrameCallback)(MEDIA_FRAME *pMediaFrame);
} TUYA_IPC_SDK_VAR_S;

OPERATE_RET TUYA_APP_Start(TUYA_IPC_SDK_VAR_S *pSdkVar);
OPERATE_RET TUYA_APP_End();
OPERATE_RET OnIotInited();
CHAR_T *gw_active_get_ext_param();
VOID gw_p2p_mqtt_data_cb(IN cJSON *root_json);

#endif
