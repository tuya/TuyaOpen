/**
 * @file tkl_init_wifi.h
 * @brief Common process - tkl init wifi description
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_INIT_WIFI_H__
#define __TKL_INIT_WIFI_H__

#include "tkl_wifi.h"

#ifdef ENABLE_HOSTAPD
#include "tkl_wifi_hostap.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * @brief the description of tuya kernel adapter layer wifi api
 *
 */
typedef struct {
    OPERATE_RET (*init)(WIFI_EVENT_CB cb);
    OPERATE_RET (*scan_ap)(const int8_t *ssid, AP_IF_S **ap_ary, uint32_t *num);
    OPERATE_RET (*release_ap)(AP_IF_S *ap);
    OPERATE_RET (*start_ap)(const WF_AP_CFG_IF_S *cfg);
    OPERATE_RET (*stop_ap)(void);
    OPERATE_RET (*set_cur_channel)(const uint8_t chan);
    OPERATE_RET (*get_cur_channel)(uint8_t *chan);
    OPERATE_RET (*set_sniffer)(const BOOL_T en, const SNIFFER_CALLBACK cb);
    OPERATE_RET (*get_ip)(const WF_IF_E wf, NW_IP_S *ip);
    OPERATE_RET (*get_ipv6)(const WF_IF_E wf, NW_IP_TYPE type, NW_IP_S *ip);
    OPERATE_RET (*set_mac)(const WF_IF_E wf, const NW_MAC_S *mac);
    OPERATE_RET (*get_mac)(const WF_IF_E wf, NW_MAC_S *mac);
    OPERATE_RET (*set_work_mode)(const WF_WK_MD_E mode);
    OPERATE_RET (*get_work_mode)(WF_WK_MD_E *mode);
    OPERATE_RET (*get_connected_ap_info)(FAST_WF_CONNECTED_AP_INFO_T **fast_ap_info);
    OPERATE_RET (*get_bssid)(uint8_t *mac);
    OPERATE_RET (*set_country_code)(const COUNTRY_CODE_E ccode);
    OPERATE_RET (*set_lp_mode)(const BOOL_T en, const uint8_t dtim);
    BOOL_T (*set_rf_calibrated)(void);
    OPERATE_RET (*station_fast_connect)(const FAST_WF_CONNECTED_AP_INFO_T *fast_ap_info);
    OPERATE_RET (*station_connect)(const int8_t *ssid, const int8_t *passwd);
    OPERATE_RET (*station_disconnect)(void);
    OPERATE_RET (*station_get_conn_ap_rssi)(int8_t *rssi);
    OPERATE_RET (*station_get_status)(WF_STATION_STAT_E *stat);
    OPERATE_RET (*send_mgnt)(const uint8_t *buf, const uint32_t len);
    OPERATE_RET (*register_recv_mgnt_callback)(const BOOL_T enable, const WIFI_REV_MGNT_CB recv_cb);
    OPERATE_RET (*ioctl)(WF_IOCTL_CMD_E cmd, void *args);
} TKL_WIFI_DESC_T;

/**
 * @brief register wifi description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_WIFI_DESC_T *tkl_wifi_desc_get(void);

/**
 *
 * @brief the description of tuya kernel adapter layer wifi hostap api
 *
 */
typedef struct {
    OPERATE_RET (*ioctl)(int dev, int vif_index, uint32_t cmd, uint32_t arg);
} TKL_WIFI_HOSTAP_DESC_T;

/**
 * @brief register wifi hostap description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_WIFI_HOSTAP_DESC_T *tkl_wifi_hostap_desc_get(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TKL_INIT_WIFI_H__
