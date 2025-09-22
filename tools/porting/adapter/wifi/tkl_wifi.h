/**
 * @file tkl_wifi.h
 * @brief Common process - adapter the wi-fi api
 * @version 0.1
 * @date 2020-11-09
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_WIFI_H__
#define __TKL_WIFI_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* tuya sdk definition of wifi ap info */
#define WIFI_SSID_LEN   32 // tuya sdk definition WIFI SSID MAX LEN
#define WIFI_PASSWD_LEN 64 // tuya sdk definition WIFI PASSWD MAX LEN

typedef struct {
    uint8_t channel;                 ///< AP channel
    int8_t rssi;                     ///< AP rssi
    uint8_t bssid[6];                ///< AP bssid
    uint8_t ssid[WIFI_SSID_LEN + 1]; ///< AP ssid array
    uint8_t s_len;                   ///< AP ssid len
    uint8_t security;                // refer to WF_AP_AUTH_MODE_E
    uint8_t resv1;
    uint8_t data_len;
    uint8_t data[0];
} AP_IF_S;

typedef enum { COUNTRY_CODE_CN, COUNTRY_CODE_US, COUNTRY_CODE_JP, COUNTRY_CODE_EU } COUNTRY_CODE_E;

/* tuya sdk definition of wifi function type */
typedef enum {
    WF_STATION = 0, ///< station type
    WF_AP,          ///< ap type
} WF_IF_E;

/* tuya sdk definition of wifi encryption type */
typedef enum {
    WAAM_OPEN = 0,     ///< open
    WAAM_WEP,          ///< WEP
    WAAM_WPA_PSK,      ///< WPA—PSK
    WAAM_WPA2_PSK,     ///< WPA2—PSK
    WAAM_WPA_WPA2_PSK, ///< WPA/WPA2
    WAAM_WPA_WPA3_SAE,
    WAAM_UNKNOWN, // unknown
} WF_AP_AUTH_MODE_E;

/* tuya sdk definition of wifi work mode */
typedef enum {
    WWM_POWERDOWN = 0, ///< wifi work in powerdown mode
    WWM_SNIFFER,       ///< wifi work in sniffer mode
    WWM_STATION,       ///< wifi work in station mode
    WWM_SOFTAP,        ///< wifi work in ap mode
    WWM_STATIONAP,     ///< wifi work in station+ap mode
    WWM_UNKNOWN,       ///< wifi work in unknown mode
} WF_WK_MD_E;

/* tuya sdk definition of ap config info */
typedef struct {
    uint8_t ssid[WIFI_SSID_LEN + 1];     ///< ssid
    uint8_t s_len;                       ///< len of ssid
    uint8_t passwd[WIFI_PASSWD_LEN + 1]; ///< passwd
    uint8_t p_len;                       ///< len of passwd
    uint8_t chan;                        ///< channel. default:6
    WF_AP_AUTH_MODE_E md;                ///< encryption type
    uint8_t ssid_hidden;                 ///< ssid hidden  default:0
    uint8_t max_conn;                    ///< max sta connect nums default:1
    uint16_t ms_interval;                ///< broadcast interval default:100
    NW_IP_S ip;                          ///< ip info for ap mode
} WF_AP_CFG_IF_S;

/* tuya sdk definition of wifi station work status */
typedef enum {
    WSS_IDLE = 0,     ///< not connected
    WSS_CONNECTING,   ///< connecting wifi
    WSS_PASSWD_WRONG, ///< passwd not match
    WSS_NO_AP_FOUND,  ///< ap is not found
    WSS_CONN_FAIL,    ///< connect fail
    WSS_CONN_SUCCESS, ///< connect wifi success
    WSS_GOT_IP,       ///< get ip success
    WSS_DHCP_FAIL,    ///< dhcp fail
} WF_STATION_STAT_E;

typedef struct {
    char ip[16];
    char mask[16];
    char gw[16];
    char dns[16];
} FAST_DHCP_INFO_T;
/* for fast connect*/
typedef struct {
    uint32_t len;    ///< data len
    uint8_t data[0]; ///< data buff
} FAST_WF_CONNECTED_AP_INFO_T;
typedef struct {
    FAST_DHCP_INFO_T fast_dhcp;
    FAST_WF_CONNECTED_AP_INFO_T fast_mac;
} FAST_CONNECTED_INFO_T;
/* tuya sdk definition of wifi event notify */
typedef enum {
    WFE_CONNECTED,
    WFE_CONNECT_FAILED,
    WFE_DISCONNECTED,
} WF_EVENT_E;

/* tuya sdk definition of wifi disconnect reason */
typedef enum {
    TUYA_WLAN_REASON_NONE,
    TUYA_WLAN_REASON_UNSPECIFIED,                ///< Unspecified reason
    TUYA_WLAN_REASON_SSID_NOT_FOUND,             ///< Cannot found the ssid for wlan scan
    TUYA_WLAN_REASON_SAE_AUTH_FAILED,            ///< WPA3 SAE auth failed
    TUYA_WLAN_REASON_ASSOC_REJECTED_TEMPORARILY, ///< Station has already been associated with MFP and SA Query timeout
                                                 ///< has not been reached. Reject the association attempt temporarily
                                                 ///< and start SA Query, if one is not pending.
    TUYA_WLAN_REASON_NOT_AUTHED,                 ///< Station tried to associate before authentication
    TUYA_WLAN_REASON_INVALID_RSN_IE,             ///< No WPA/RSN IE in association request
    TUYA_WLAN_REASON_GROUP_CIPHER_INVALID,       ///< Group cipher invalid of WPA/RSN IE in in association request
    TUYA_WLAN_REASON_PAIRWISE_CIPHER_INVALID,    ///< Pairwise cipher invalid of WPA/RSN IE in association request
    TUYA_WLAN_REASON_UNSPPORTED_RSN_IE_VERSION,  ///< Invalid WPA/RSN IE version
    TUYA_WLAN_REASON_AKMP_INVALID, ///< WPA/RSN information element rejected because of invalid auth key management
    TUYA_WLAN_REASON_INVALID_RSN_IE_CAP,          ///< Invalid RSN Capabilities
    TUYA_WLAN_REASON_CIPHER_REJECTED_PER_POLICY,  ///< Station tried to use TKIP with HT association
    TUYA_WLAN_REASON_PMF_REQUIRED,                ///< Rejected because of invalid PMF required
    TUYA_WLAN_REASON_PREV_AUTH_INVALID,           ///< SA Query timed out
    TUYA_WLAN_REASON_NOT_ASSOCED,                 ///< Station tried to 4-way handshake before associate
    TUYA_WLAN_REASON_WRONG_PASSWORD,              ///<  Wrong password for 4-way handshake
    TUYA_WLAN_REASON_MIC_FAILURE,                 ///< Michael check failed
    TUYA_WLAN_REASON_RSN_IE_IN_4WAYS_MISMATCH,    ///< RSN IE mismatch between AP and station of 4-way handshake
    TUYA_WLAN_REASON_4WAYS_HANDSHAKE_TIMEOUT,     ///< Timeout of 4-way handshake
    TUYA_WLAN_REASON_INACTIVITY_DISCONNECT,       ///< Station disconnected to AP beacause of in activity
    TUYA_WLAN_REASON_DEAUTH_LEAVING,              ///< Deauth the station because it was left
    TUYA_WLAN_REASON_AP_UNABLE_TO_HANDLE_NEW_STA, ///< Association denied because AP is unable to handle additional
                                                  ///< associated STAs
    TUYA_WLAN_REASON_MAX,
} WF_DISCONN_REASON_E;

/* tuya sdk definition of wifi ioctl cmd */
typedef enum {
    WFI_BEACON_CMD,
    WFI_GET_LAST_DISCONN_REASON, ///< Get WiFi last disconnect reason
    WFI_AP_GET_STALIST_CMD,      ///< Get Sta List Cmd
    WFI_CONNECT_CMD,
} WF_IOCTL_CMD_E;

typedef struct {
    uint8_t *ssid;
    uint8_t *passwd;
    uint8_t channel;
} WF_IOCTL_CONN_T;

typedef enum {
    IPV4_DHCP_SUCC,
    IPV4_DHCP_FAIL,
    IPV6_LL_SUCC,
    IPV6_LL_FAIL,
    IPV6_DHCP_SUCC,
    IPV6_DHCP_FAIL,
} LWIP_EVENT_E;

typedef struct {
    uint8_t ssid[WIFI_SSID_LEN + 1];
    uint8_t channel;
    NW_MAC_S mac;
    uint32_t vsie_data_len;
    uint8_t *vsie_data;
} WF_IOCTL_BEACON_T;

typedef struct {
    NW_IP_S sta_ip;
    NW_MAC_S sta_mac;
} WF_STA_INFO_S;

typedef struct {
    WF_STA_INFO_S *array;
    uint32_t num;
} WF_STA_LIST_S;
/**
 * @brief callback function: SNIFFER_CALLBACK
 *        when wifi sniffers package from air, notify tuyaos
 *        with this callback. the package should include
 * @param[in]       buf         the buf wifi recv
 * @param[in]       len         the len of buf
 */
typedef void (*SNIFFER_CALLBACK)(const uint8_t *buf, const uint16_t len, const int8_t rssi);

/**
 * @brief callback function: WIFI_REV_MGNT_CB
 *        when receive wifi management frame, notify tuyaos
 *        with this callback.
 * @param[in]       buf         the buf wifi recv
 * @param[in]       len         the len of buf
 */
typedef void (*WIFI_REV_MGNT_CB)(uint8_t *buf, uint32_t len);

/**
 * @brief callback function: WIFI_STATUS_CHANGE_CB
 *        when wifi connect status changed, notify tuyaos
 *        with this callback.
 * @param[out]       is_up         the wifi link status is up or not
 */
typedef void (*WIFI_EVENT_CB)(WF_EVENT_E event, void *arg);

/**
 * @brief set wifi station work status changed callback
 *
 * @param[in]      cb        the wifi station work status changed callback
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_init(WIFI_EVENT_CB cb);

/**
 * @brief scan current environment and obtain the ap
 *        infos in current environment
 *
 * @param[in]       ssid        the specific ssid
 * @param[out]      ap_ary      current ap info array
 * @param[out]      num         the num of ar_ary
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 *
 * @note if ssid == NULL means scan all ap, otherwise means scan the specific ssid
 */
OPERATE_RET tkl_wifi_scan_ap(const int8_t *ssid, AP_IF_S **ap_ary, uint32_t *num);

/**
 * @brief release the memory malloced in <tkl_wifi_ap_scan>
 *        if needed. tuyaos will call this function when the
 *        ap info is no use.
 *
 * @param[in]       ap          the ap info
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_release_ap(AP_IF_S *ap);

/**
 * @brief start a soft ap
 *
 * @param[in]       cfg         the soft ap config
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_start_ap(const WF_AP_CFG_IF_S *cfg);

/**
 * @brief stop a soft ap
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_stop_ap(void);

/**
 * @brief set wifi interface work channel
 *
 * @param[in]       chan        the channel to set
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_set_cur_channel(const uint8_t chan);

/**
 * @brief get wifi interface work channel
 *
 * @param[out]      chan        the channel wifi works
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_get_cur_channel(uint8_t *chan);

/**
 * @brief enable / disable wifi sniffer mode.
 *        if wifi sniffer mode is enabled, wifi recv from
 *        packages from the air, and user shoud send these
 *        packages to tuya-sdk with callback <cb>.
 *
 * @param[in]       en          enable or disable
 * @param[in]       cb          notify callback
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_set_sniffer(const BOOL_T en, const SNIFFER_CALLBACK cb);

/**
 * @brief get wifi ip info.when wifi works in
 *        ap+station mode, wifi has two ips.
 *
 * @param[in]       wf          wifi function type
 * @param[out]      ip          the ip addr info
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_get_ip(const WF_IF_E wf, NW_IP_S *ip);

/**
 * @brief get wifi ip info.when wifi works in
 *        ap+station mode, wifi has two ips.
 *
 * @param[in]       wf          wifi function type
 * @param[in]       wf          wifi ip type
 * @param[out]      ip          the ip addr info
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_get_ipv6(const WF_IF_E wf, NW_IP_TYPE type, NW_IP_S *ip);

/**
 * @brief wifi set ip
 *
 * @param[in]       wf     wifi function type
 * @param[in]       ip     the ip addr info
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_set_ip(const WF_IF_E wf, NW_IP_S *ip);

/**
 * @brief set wifi mac info.when wifi works in
 *        ap+station mode, wifi has two macs.
 *
 * @param[in]       wf          wifi function type
 * @param[in]       mac         the mac info
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_set_mac(const WF_IF_E wf, const NW_MAC_S *mac);

/**
 * @brief get wifi mac info.when wifi works in
 *        ap+station mode, wifi has two macs.
 *
 * @param[in]       wf          wifi function type
 * @param[out]      mac         the mac info
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_get_mac(const WF_IF_E wf, NW_MAC_S *mac);

/**
 * @brief set wifi work mode
 *
 * @param[in]       mode        wifi work mode
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_set_work_mode(const WF_WK_MD_E mode);

/**
 * @brief get wifi work mode
 *
 * @param[out]      mode        wifi work mode
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_get_work_mode(WF_WK_MD_E *mode);

/**
 * @brief : get ap info for fast connect
 * @param[out]      fast_ap_info
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_get_connected_ap_info(FAST_WF_CONNECTED_AP_INFO_T **fast_ap_info);

/**
 * @brief get wifi bssid
 *
 * @param[out]      mac         uplink mac
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_get_bssid(uint8_t *mac);

/**
 * @brief set wifi country code
 *
 * @param[in]       ccode  country code
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_set_country_code(const COUNTRY_CODE_E ccode);

/**
 * @brief do wifi calibration
 *
 * @note called when test wifi
 *
 * @return true on success. faile on failure
 */
OPERATE_RET tkl_wifi_set_rf_calibrated(void);

/**
 * @brief set wifi lowpower mode
 *
 * @param[in]       enable      enbale lowpower mode
 * @param[in]       dtim     the wifi dtim
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_set_lp_mode(const BOOL_T enable, const uint8_t dtim);

/**
 * @brief : fast connect
 * @param[in]      fast_ap_info
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_station_fast_connect(const FAST_WF_CONNECTED_AP_INFO_T *fast_ap_info);

/**
 * @brief connect wifi with ssid and passwd
 *
 * @param[in]       ssid
 * @param[in]       passwd
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_station_connect(const int8_t *ssid, const int8_t *passwd);

/**
 * @brief disconnect wifi from connect ap
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_station_disconnect(void);

/**
 * @brief get wifi connect rssi
 *
 * @param[out]      rssi        the return rssi
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_station_get_conn_ap_rssi(int8_t *rssi);

/**
 * @brief get wifi station work status
 *
 * @param[out]      stat        the wifi station work status
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_station_get_status(WF_STATION_STAT_E *stat);

/**
 * @brief send wifi management
 *
 * @param[in]       buf         pointer to buffer
 * @param[in]       len         length of buffer
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_send_mgnt(const uint8_t *buf, const uint32_t len);

/**
 * @brief register receive wifi management callback
 *
 * @param[in]       enable
 * @param[in]       recv_cb     receive callback
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_register_recv_mgnt_callback(const BOOL_T enable, const WIFI_REV_MGNT_CB recv_cb);

/**
 * @brief wifi ioctl
 *
 * @param[in]       cmd     refer to WF_IOCTL_CMD_E
 * @param[in]       args    args associated with the command
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_wifi_ioctl(WF_IOCTL_CMD_E cmd, void *args);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
