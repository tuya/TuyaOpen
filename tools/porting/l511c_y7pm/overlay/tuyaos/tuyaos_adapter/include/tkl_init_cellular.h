#ifndef L511C_TKL_INIT_CELLULAR_H
#define L511C_TKL_INIT_CELLULAR_H

/* Minimal TuyaOpen TAL descriptor surface used by tal_cellular.c. The generic
 * init header pulls optional call/SMS/player TKL APIs that this vendor SDK
 * does not ship; keep this platform bridge limited to the cellular data path.
 */
#include "tkl_cellular.h"

typedef struct {
    OPERATE_RET (*init)(TKL_CELLULAR_BASE_CFG_T *cfg);
    OPERATE_RET (*get_status)(TKL_CELLULAR_STAT_E *status);
    OPERATE_RET (*set_status_cb)(TKL_CELLULAR_STATUS_CHANGE_CB callback);
    OPERATE_RET (*get_ip)(NW_IP_S *ip);
    OPERATE_RET (*get_ipv6)(NW_IP_TYPE type, NW_IP_S *ip);
    OPERATE_RET (*get_ccid)(char *ccid);
    OPERATE_RET (*get_rssi)(char *rssi);
    OPERATE_RET (*get_volt)(uint32_t *voltage);
    OPERATE_RET (*get_imei)(char *imei);
    OPERATE_RET (*get_sn)(char *sn);
    OPERATE_RET (*get_sw_ver)(char *version);
} TKL_CELLULAR_DESC_T;

TKL_CELLULAR_DESC_T *tkl_cellular_desc_get(void);

#endif /* L511C_TKL_INIT_CELLULAR_H */
