/**
 * @file tal_wifi.h
 * @brief This file contains the structures and functions declarations for
 * managing WiFi functionalities, including scanning for access points, managing
 * WiFi connections, and handling different WiFi events.
 *
 * It provides a comprehensive interface for interacting with the WiFi hardware,
 * allowing for operations such as initializing the WiFi module, scanning for
 * nearby access points, connecting to an access point, setting and getting the
 * current WiFi channel, and enabling or disabling sniffer mode. Additionally,
 * it includes functionalities for managing WiFi MAC and IP addresses.
 *
 * The file is part of the Tuya IoT SDK and is used to abstract the underlying
 * hardware specifics, providing a uniform interface for WiFi operations across
 * different platforms and hardware.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_WIFI_H__
#define __TAL_WIFI_H__

#include "tuya_cloud_types.h"
#include "tkl_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAG_SSID_NUMBER    0
#define TAG_PAYLOAD_NUMBER 221

#define PROBE_REQUEST_TYPE_SUBTYPE    0x0040
#define PROBE_REQSPONSE_TYPE_SUBTYPE  0x0050
#define PROBE_REQUEST_DURATION_ID     0x0
#define PROBE_RESPONSET_DURATION_ID   0x0
#define PROBE_REQUEST_PAYLOAD_LEN_MAX 255
#define BROADCAST_MAC_ADDR            0xFFFFFFFF

/**
 * @brief WIFI chip detects the local AP information structure
 * @struct MIMO_IF_S
 */
typedef enum {
    MIMO_TYPE_NORMAL = 0,
    MIMO_TYPE_HT40,
    MIMO_TYPE_2X2,
    MIMO_TYPE_LDPC,
    MIMO_TYPE_NUM,
} MIMO_TYPE_E;

typedef struct {
    signed char rssi;      /*!< MIMO Packet signal */
    MIMO_TYPE_E type;      /*!< MIMO Packet type */
    unsigned short len;    /*!< MIMO Packet length */
    unsigned char channel; /*!< MIMO Packet channel*/
    unsigned char mcs;
} MIMO_IF_S;

typedef enum {
    WFT_PROBE_REQ = 0x40, ///< Probe request
    WFT_PROBE_RSP = 0x50, ///< Probe response
    WFT_AUTH = 0xB0,      ///< auth
    WFT_BEACON = 0x80,    ///< Beacon
    WFT_DATA = 0x08,      ///< Data
    WFT_QOS_DATA = 0x88,  ///< QOS Data
    WFT_MIMO_DATA = 0xff, ///< MIMO Data
} WLAN_FRM_TP_E;

#pragma pack(1)
typedef struct {
    // 802.11 management
    unsigned char id;
    unsigned char len;
    char data[0]; ///< data
} WLAN_MANAGEMENT_S;

typedef struct {
    unsigned char frame_type;       ///< WLAN Frame type
    unsigned char frame_ctrl_flags; ///< Frame Control flags
    unsigned short duration;        ///< Duration
    unsigned char dest[6];          ///< Destination MAC Address
    unsigned char src[6];           ///< Source MAC Address
    unsigned char bssid[6];         ///< BSSID MAC Address
    unsigned short seq_frag_num;    ///< Sequence and Fragmentation number
} WLAN_PROBE_REQ_IF_S;

typedef struct {
    unsigned char frame_ctrl_flags; ///< Frame Control flags
    unsigned short duration;        ///< Duration
    unsigned char dest[6];          ///< Destination MAC Address
    unsigned char src[6];           ///< Source MAC Address
    unsigned char bssid[6];         ///< BSSID MAC Address
    unsigned short seq_frag_num;    ///< Sequence and Fragmentation number
    unsigned char timestamp[8];     ///< Time stamp
    unsigned short beacon_interval; ///< Beacon Interval
    unsigned short cap_info;        ///< Capability Information
    unsigned char ssid_element_id;  ///< SSID Element ID
    unsigned char ssid_len;         ///< SSID Length
    char ssid[0];                   ///< SSID
} WLAN_BEACON_IF_S;

#define TO_FROM_DS_MASK 0x03
#define TFD_IBSS        0x00 ///< da+sa+bssid
#define TFD_TO_AP       0x01 ///< bssid+sa+da
#define TFD_FROM_AP     0x02 ///< ds+bssid+sa
#define TFD_WDS         0x03 ///< ra+ta+da

typedef unsigned char BC_DA_CHAN_T;
#define BC_TO_AP    0
#define BC_FROM_AP  1
#define BC_CHAN_NUM 2

typedef struct {
    unsigned char addr1[6];
    unsigned char addr2[6];
    unsigned char addr3[6];
} WLAN_COM_ADDR_S;

typedef struct {
    unsigned char bssid[6];
    unsigned char src[6];
    unsigned char dst[6];
} WLAN_TO_AP_ADDR_S;

typedef struct {
    unsigned char dst[6];
    unsigned char bssid[6];
    unsigned char src[6];
} WLAN_FROM_AP_ADDR_S;

typedef union {
    WLAN_COM_ADDR_S com;
    WLAN_TO_AP_ADDR_S to_ap;
    WLAN_FROM_AP_ADDR_S from_ap;
} WLAN_ADDR_U;

typedef struct {
    unsigned char frame_ctrl_flags; ///< Frame Control flags
    unsigned short duration;        ///< Duration
    WLAN_ADDR_U addr;               ///< address
    unsigned short seq_frag_num;    ///< Sequence and Fragmentation number
    unsigned short qos_ctrl;        ///< QoS Control bits
} WLAN_DATA_IF_S;

/*!
\brief WLAN Frame info
\struct WLAN_FRAME_S
*/
typedef struct {
    unsigned char frame_type; ///< WLAN Frame type
    union {
        WLAN_BEACON_IF_S beacon_info; ///< WLAN Beacon info
        WLAN_DATA_IF_S data_info;     ///< WLAN Data info
        MIMO_IF_S mimo_info;          ///< mimo info
    } frame_data;
} WLAN_FRAME_S, *P_WLAN_FRAME_S;

typedef struct {
    unsigned short type_and_subtype;
    unsigned short duration_id;
    unsigned char addr1[6];
    unsigned char addr2[6];
    unsigned char addr3[6];
    unsigned short seq_ctrl;
} PROBE_REQUEST_PACKAGE_HEAD_S;

typedef struct {
    uint16_t type_and_subtype;
    uint16_t duration_id;
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint16_t seq_ctrl;
    uint8_t timestamp[8];     // Time stamp
    uint16_t beacon_interval; // Beacon Interval
    uint16_t cap_info;        // Capability Information
} PROBE_RESPONSE_PACKAGE_HEAD_S;

typedef struct {
    unsigned char index;
    unsigned char len;
    unsigned char ptr[0];
} BEACON_TAG_DATA_UNIT_S;
#pragma pack()

typedef struct {
    PROBE_REQUEST_PACKAGE_HEAD_S pack_head;
    BEACON_TAG_DATA_UNIT_S tag_ssid;
} PROBE_REQUEST_FIX_S;

/**
 * @brief set wifi station event changed callback
 *
 * @param[in]      cb        the wifi station event changed callback
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_init(WIFI_EVENT_CB cb);

/**
 * @brief scan current environment and obtain all the ap
 *        infos in current environment
 *
 * @param[out]      ap_ary      current ap info array
 * @param[out]      num         the num of ar_ary
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_all_ap_scan(AP_IF_S **ap_ary, uint32_t *num);

/**
 * @brief scan current environment and obtain the specific
 *        ap info.
 *
 * @param[in]       ssid        the specific ssid
 * @param[out]      ap          the ap info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_assign_ap_scan(int8_t *ssid, AP_IF_S **ap);

/**
 * @brief release the memory malloced in <tuya_hal_wifi_all_ap_scan>
 *        and <tuya_hal_wifi_assign_ap_scan> if needed. tuya-sdk
 *        will call this function when the ap info is no use.
 *
 * @param[in]       ap          the ap info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_release_ap(AP_IF_S *ap);

/**
 * @brief set wifi interface work channel
 *
 * @param[in]       chan        the channel to set
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_set_cur_channel(uint8_t chan);

/**
 * @brief get wifi interface work channel
 *
 * @param[out]      chan        the channel wifi works
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_get_cur_channel(uint8_t *chan);

/**
 * @brief enable / disable wifi sniffer mode.
 *        if wifi sniffer mode is enabled, wifi recv from
 *        packages from the air, and user shoud send these
 *        packages to tuya-sdk with callback <cb>.
 *
 * @param[in]       en          enable or disable
 * @param[in]       cb          notify callback
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_sniffer_set(BOOL_T en, SNIFFER_CALLBACK cb);

/**
 * @brief get wifi ip info.when wifi works in
 *        ap+station mode, wifi has two ips.
 *
 * @param[in]       wf          wifi function type
 * @param[out]      ip          the ip addr info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_get_ip(WF_IF_E wf, NW_IP_S *ip);

/**
 * @brief set wifi ip info set ip.when wifi works in
 *        ap+station mode, wifi has two ips.
 *
 * @param[in]       wf     wifi function type
 * @param[in]       ip     the ip addr info
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */

OPERATE_RET tal_wifi_set_ip(WF_IF_E wf, NW_IP_S *ip);

/**
 * @brief set wifi mac info.when wifi works in
 *        ap+station mode, wifi has two macs.
 *
 * @param[in]       wf          wifi function type
 * @param[in]       mac         the mac info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_set_mac(WF_IF_E wf, NW_MAC_S *mac);

/**
 * @brief get wifi mac info.when wifi works in
 *        ap+station mode, wifi has two macs.
 *
 * @param[in]       wf          wifi function type
 * @param[out]      mac         the mac info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_get_mac(WF_IF_E wf, NW_MAC_S *mac);

/**
 * @brief set wifi work mode
 *
 * @param[in]       mode        wifi work mode
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_set_work_mode(WF_WK_MD_E mode);

/**
 * @brief get wifi work mode
 *
 * @param[out]      mode        wifi work mode
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_get_work_mode(WF_WK_MD_E *mode);

/**
 * @brief start a soft ap
 *
 * @param[in]       cfg         the soft ap config
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_ap_start(WF_AP_CFG_IF_S *cfg);

/**
 * @brief stop a soft ap
 *
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_ap_stop(void);

/**
 * @brief : get ap info for fast connect
 * @param[out]      fast_ap_info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET
tal_wifi_get_connected_ap_info(FAST_WF_CONNECTED_AP_INFO_T **fast_ap_info);

/**
 * @brief : fast connect
 * @param[in]      fast_ap_info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_fast_station_connect(FAST_WF_CONNECTED_AP_INFO_T *fast_ap_info);

/**
 * @brief connect wifi with ssid and passwd
 *
 * @param[in]       ssid
 * @param[in]       passwd
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_station_connect(int8_t *ssid, int8_t *passwd);

/**
 * @brief disconnect wifi from connect ap
 *
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_station_disconnect(void);

/**
 * @brief get wifi connect rssi
 *
 * @param[out]      rssi        the return rssi
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_station_get_conn_ap_rssi(int8_t *rssi);

/**
 * @brief get wifi bssid
 *
 * @param[out]      mac         uplink mac
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_get_bssid(uint8_t *mac);

/**
 * @brief get wifi station work status
 *
 * @param[out]      stat        the wifi station work status
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_station_get_status(WF_STATION_STAT_E *stat);

/**
 * @brief get wifi error station work status
 *
 * @param[out]      stat        the wifi station work status
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_station_get_err_stat(WF_STATION_STAT_E *stat);

/**
 * @brief set wifi country code
 *
 * @param[in]       ccode  country code
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_set_country_code(char *country_code);

/**
 * @brief send wifi management
 *
 * @param[in]       buf         pointer to buffer
 * @param[in]       len         length of buffer
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_send_mgnt(uint8_t *buf, uint32_t len);

/**
 * @brief register receive wifi management callback
 *
 * @param[in]       enable
 * @param[in]       recv_cb     receive callback
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_register_recv_mgnt_callback(BOOL_T enable, WIFI_REV_MGNT_CB recv_cb);

/**
 * @brief enable wifi lowpower
 *
 * @param[in]       none
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_lp_enable(void);

/**
 * @brief disable wifi lowpower
 *
 * @param[in]       none
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_lp_disable(void);

/**
 * @brief set the wifi low power dtim.
 *
 * @note called before enter wifi low power mode
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
void tal_wifi_set_lps_dtim(uint32_t dtim);

/**
 * @brief do wifi calibration
 *
 * @note called when test wifi
 *
 * @return true on success. faile on failure
 */
BOOL_T tal_wifi_rf_calibrated(void);

/**
 * @brief wifi ioctl
 *
 * @param[in]       cmd     refer to WF_IOCTL_CMD_E
 * @param[in]       args    args associated with the command
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_ioctl(WF_IOCTL_CMD_E cmd, void *args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TAL_WIFI_H__
