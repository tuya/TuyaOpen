/**
 * @file tal_wifi.c
 * @brief This file provides the implementation of Tuya's abstracted WiFi
 * functionalities, including initialization, scanning for access points,
 * connecting to WiFi networks, managing WiFi modes, and handling WiFi events.
 *        It serves as an interface between Tuya's SDK and the underlying WiFi
 * driver, offering a unified API for WiFi operations across different
 * platforms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_iot_config.h"
#include "tal_wifi.h"
#include "tal_api.h"
#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
#include "tuya_wlan_auth.h"
#endif

#define TAL_WIFI_CHECK_PARM(__EXPRESS)                                                                                 \
    if (!(__EXPRESS))                                                                                                  \
    return OPRT_INVALID_PARM

#define TAL_WIFI_LOCK() tal_mutex_lock(s_tal_wifi.mutex)

#define TAL_WIFI_UNLOCK() tal_mutex_unlock(s_tal_wifi.mutex)

typedef struct scan_sniffer_t {
    AP_IF_S *pApInfo;
} scan_sniffer_s;

typedef struct {
    MUTEX_HANDLE mutex;
    SNIFFER_CALLBACK snif_cb;
    BOOL_T set_mode_done;
    uint32_t lp_disable_cnt;
    uint32_t lps_dtim;
} TAL_WIFI_T;

static TAL_WIFI_T s_tal_wifi = {0};

OPERATE_RET tal_wifi_init(WIFI_EVENT_CB cb)
{
    TAL_WIFI_CHECK_PARM(cb);

    OPERATE_RET op_ret = tal_mutex_create_init(&s_tal_wifi.mutex);
    if (OPRT_OK != op_ret) {
        PR_ERR("create mutex fail");
        return op_ret;
    }

#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    return tuya_wpa_supp_evt_init(cb);
#else
    return tkl_wifi_init(cb);
#endif
}

/**
 * @brief scan current environment and obtain all the ap
 *        infos in current environment
 *
 * @param[out]      ap_ary      current ap info array
 * @param[out]      num         the num of ar_ary
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_all_ap_scan(AP_IF_S **ap_ary, uint32_t *num)
{
    OPERATE_RET op_ret;

    TAL_WIFI_CHECK_PARM(ap_ary && num);

    TAL_WIFI_LOCK();
#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    op_ret = tuya_wpa_supp_scan(NULL, ap_ary, num);
#else
    op_ret = tkl_wifi_scan_ap(NULL, ap_ary, num);
#endif
    TAL_WIFI_UNLOCK();

    return op_ret;
}
/**
 * @brief scan current environment and obtain the specific
 *        ap info.
 *
 * @param[in]       ssid        the specific ssid
 * @param[out]      ap          the ap info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_assign_ap_scan(int8_t *ssid, AP_IF_S **ap)
{
    OPERATE_RET op_ret = OPRT_OK;

    TAL_WIFI_CHECK_PARM(ssid && ap);

    SNIFFER_CALLBACK snif_cb = NULL;
    WF_WK_MD_E cur_mode;

    TAL_WIFI_LOCK();
    tal_wifi_get_work_mode(&cur_mode);
    if (WWM_POWERDOWN == cur_mode) {
        tal_wifi_set_work_mode(WWM_STATION);
    } else if (WWM_SNIFFER == cur_mode) {
        snif_cb = s_tal_wifi.snif_cb;
        tal_wifi_sniffer_set(FALSE, NULL);
        tal_wifi_set_work_mode(WWM_STATION);
    }
    uint32_t num = 0;

#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    op_ret = tuya_wpa_supp_scan((char *)ssid, ap, &num);
#else
    op_ret = tkl_wifi_scan_ap((int8_t *)ssid, ap, &num);
#endif
    //! find better rssi for ssid
    if (OPRT_OK == op_ret && num > 1 && *ap) {
        AP_IF_S *list = *ap;
        AP_IF_S *item = list;
        while (--num) {
            PR_DEBUG("ap addr %08x, rssi %d, ssid %s", &list[num], list[num].rssi, list[num].ssid);
            if (item->rssi < list[num].rssi) {
                item = &list[num];
            }
        }
        PR_DEBUG("ap addr %08x, rssi %d, ssid %s", list, list->rssi, list->ssid);
        //! exchange rssi on list[0]
        if (item != list) {
            memcpy(list, item, sizeof(AP_IF_S));
        }
        PR_DEBUG("ap addr %08x, max rssi %d", list, list->rssi);
    }
    if (WWM_POWERDOWN == cur_mode) {
        tal_wifi_set_work_mode(WWM_POWERDOWN);
    } else if (WWM_SNIFFER == cur_mode) {
        tal_wifi_set_work_mode(WWM_SNIFFER);
        tal_wifi_sniffer_set(TRUE, snif_cb);
    }
    TAL_WIFI_UNLOCK();

    return op_ret;
}

/**
 * @brief release the memory malloced in <tuya_hal_wifi_all_ap_scan>
 *        and <tuya_hal_wifi_assign_ap_scan> if needed. tuya-sdk
 *        will call this function when the ap info is no use.
 *
 * @param[in]       ap          the ap info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_release_ap(AP_IF_S *ap)
{
    TAL_WIFI_CHECK_PARM(ap);
#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    return tuya_wpa_supp_release_ap(ap);
#else
    return tkl_wifi_release_ap(ap);
#endif
}
/**
 * @brief set wifi interface work channel
 *
 * @param[in]       chan        the channel to set
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_set_cur_channel(uint8_t chan)
{
    return tkl_wifi_set_cur_channel(chan);
}
/**
 * @brief get wifi interface work channel
 *
 * @param[out]      chan        the channel wifi works
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_get_cur_channel(uint8_t *chan)
{
    TAL_WIFI_CHECK_PARM(chan);

    return tkl_wifi_get_cur_channel(chan);
}

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
OPERATE_RET tal_wifi_sniffer_set(BOOL_T en, SNIFFER_CALLBACK cb)
{
    OPERATE_RET op_ret = OPRT_OK;

    TAL_WIFI_LOCK();
    op_ret = tkl_wifi_set_sniffer(en, cb);
    TAL_WIFI_UNLOCK();
    if (OPRT_OK == op_ret) {
        s_tal_wifi.snif_cb = cb;
    }

    return op_ret;
}

/**
 * @brief set wifi ip info.when wifi works in
 *        ap+station mode, wifi has two ips.
 *
 * @param[in]       wf     wifi function type
 * @param[in]       ip     the ip addr info
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_wifi_set_ip(WF_IF_E wf, NW_IP_S *ip)
{
    TAL_WIFI_CHECK_PARM(ip);
#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    return tuya_wpas_set_ip(wf, ip);
#else
    return tkl_wifi_set_ip(wf, ip);
#endif
}

/**
 * @brief get wifi ip info.when wifi works in
 *        ap+station mode, wifi has two ips.
 *
 * @param[in]       wf          wifi function type
 * @param[out]      ip          the ip addr info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_get_ip(WF_IF_E wf, NW_IP_S *ip)
{
    TAL_WIFI_CHECK_PARM(ip);

#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    return tuya_wpas_get_ip(wf, ip);
#else
    return tkl_wifi_get_ip(wf, ip);
#endif
}

/**
 * @brief set wifi mac info.when wifi works in
 *        ap+station mode, wifi has two macs.
 *
 * @param[in]       wf          wifi function type
 * @param[in]       mac         the mac info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_set_mac(WF_IF_E wf, NW_MAC_S *mac)
{
    TAL_WIFI_CHECK_PARM(mac);

    return tkl_wifi_set_mac(wf, mac);
}

/**
 * @brief get wifi mac info.when wifi works in
 *        ap+station mode, wifi has two macs.
 *
 * @param[in]       wf          wifi function type
 * @param[out]      mac         the mac info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_get_mac(WF_IF_E wf, NW_MAC_S *mac)
{
    TAL_WIFI_CHECK_PARM(mac);

    return tkl_wifi_get_mac(wf, mac);
}

/**
 * @brief set wifi work mode
 *
 * @param[in]       mode        wifi work mode
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_set_work_mode(WF_WK_MD_E mode)
{
    OPERATE_RET op_ret;

    TAL_WIFI_LOCK();
    op_ret = tkl_wifi_set_work_mode(mode);
    s_tal_wifi.set_mode_done = TRUE;
    TAL_WIFI_UNLOCK();

    return op_ret;
}

/**
 * @brief get wifi work mode
 *
 * @param[out]      mode        wifi work mode
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_get_work_mode(WF_WK_MD_E *mode)
{
    TAL_WIFI_CHECK_PARM(mode);

    // wifi status not set, work in unknown mode
    if (!s_tal_wifi.set_mode_done) {
        *mode = WWM_UNKNOWN;
        return OPRT_OK;
    }

    return tkl_wifi_get_work_mode(mode);
}

/**
 * @brief : get ap info for fast connect
 * @param[out]      fast_ap_info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET
tal_wifi_get_connected_ap_info(FAST_WF_CONNECTED_AP_INFO_T **fast_ap_info)
{
    TAL_WIFI_CHECK_PARM(fast_ap_info);

#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    return tuya_wpa_supp_get_conn_ap_info(fast_ap_info);
#else
    return tkl_wifi_get_connected_ap_info(fast_ap_info);
#endif
}

/**
 * @brief : fast connect
 * @param[in]      fast_ap_info
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_fast_station_connect(FAST_WF_CONNECTED_AP_INFO_T *fast_ap_info)
{
    TAL_WIFI_CHECK_PARM(fast_ap_info);

#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    tuya_wpa_supp_stop();
    return tuya_wpa_supp_fast_connect(fast_ap_info);
#else
    return tkl_wifi_station_fast_connect(fast_ap_info);
#endif
}

/**
 * @brief connect wifi with ssid and passwd
 *
 * @param[in]       ssid
 * @param[in]       passwd
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_station_connect(int8_t *ssid, int8_t *passwd)
{
    TAL_WIFI_CHECK_PARM(ssid);

#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    tuya_wpa_supp_stop();
    return tuya_wpas_sta_connect((char *)ssid, (char *)passwd);
#else
    return tkl_wifi_station_connect((int8_t *)ssid, (int8_t *)passwd);
#endif
}

/**
 * @brief disconnect wifi from connect ap
 *
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_station_disconnect(void)
{
#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    return tuya_wpa_supp_stop();
#else
    return tkl_wifi_station_disconnect();
#endif
}

/**
 * @brief get wifi connect rssi
 *
 * @param[out]      rssi        the return rssi
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_station_get_conn_ap_rssi(int8_t *rssi)
{
    TAL_WIFI_CHECK_PARM(rssi);

#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    return tuya_wpa_supp_get_conn_ap_rssi((char *)rssi);
#else
    return tkl_wifi_station_get_conn_ap_rssi(rssi);
#endif
}

/**
 * @brief get wifi bssid
 *
 * @param[out]      mac         uplink mac
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_get_bssid(uint8_t *mac)
{
    TAL_WIFI_CHECK_PARM(mac);

#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    return tuya_wpa_supp_get_bssid((uint8_t *)mac);
#else
    return tkl_wifi_get_bssid(mac);
#endif
}

/**
 * @brief get wifi station work status
 *
 * @param[out]      stat        the wifi station work status
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_station_get_status(WF_STATION_STAT_E *stat)
{
    TAL_WIFI_CHECK_PARM(stat);

#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    return tuya_wpa_supp_get_sta_status(stat);
#else
    return tkl_wifi_station_get_status(stat);
#endif
}

/**
 * @brief start a soft ap
 *
 * @param[in]       cfg         the soft ap config
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_ap_start(WF_AP_CFG_IF_S *cfg)
{
    TAL_WIFI_CHECK_PARM(cfg);

#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    return tuya_hostap_start(cfg);
#else
    return tkl_wifi_start_ap(cfg);
#endif
}

/**
 * @brief stop a soft ap
 *
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_ap_stop(void)
{
#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    return tuya_hostap_stop();
#else
    return tkl_wifi_stop_ap();
#endif
}

/**
 * @brief get wifi bssid
 *
 * @param[out]      mac         uplink mac
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_get_ap_mac(uint8_t *mac)
{
    TAL_WIFI_CHECK_PARM(mac);

#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    return tuya_wpa_supp_get_bssid((uint8_t *)mac);
#else
    return tkl_wifi_get_bssid(mac);
#endif
}

/**
 * @brief set wifi country code
 *
 * @param[in]       ccode  country code
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_set_country_code(char *country_code)
{
    int i;
    COUNTRY_CODE_E index = COUNTRY_CODE_CN;
    char *map[] = {"CN", "US", "JP", "EU"};

    for (i = 0; country_code && i < CNTSOF(map); i++) {
        if (0 == strcmp(map[i], country_code)) {
            index = i;
            break;
        }
    }

    return tkl_wifi_set_country_code(index);
}

/**
 * @brief send wifi management
 *
 * @param[in]       buf         pointer to buffer
 * @param[in]       len         length of buffer
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_send_mgnt(uint8_t *buf, uint32_t len)
{
    TAL_WIFI_CHECK_PARM(buf);

#if defined(TUYA_HOSTAPD_SUPPORT) && (TUYA_HOSTAPD_SUPPORT == 1)
    return tuya_hostap_send_raw_frame(INVALID_IF_INDEX, buf, len);
#else
    return tkl_wifi_send_mgnt(buf, len);
#endif
}

/**
 * @brief register receive wifi management callback
 *
 * @param[in]       enable
 * @param[in]       recv_cb     receive callback
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_register_recv_mgnt_callback(BOOL_T enable, WIFI_REV_MGNT_CB recv_cb)
{
    TAL_WIFI_CHECK_PARM(recv_cb);

    return tkl_wifi_register_recv_mgnt_callback(enable, recv_cb);
}

/**
 * @brief Sets the LPS DTIM value.
 *
 * This function sets the LPS (Low Power Solution) DTIM (Delivery Traffic
 * Indication Message) value. The DTIM value determines the interval at which
 * the AP (Access Point) sends beacon frames containing buffered multicast or
 * broadcast data.
 *
 * @param dtim The DTIM value to set.
 */
void tal_wifi_set_lps_dtim(uint32_t dtim)
{
    s_tal_wifi.lps_dtim = dtim;
}

/**
 * @brief enable wifi lowpower
 *
 * @param[in]       none
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_lp_enable(void)
{
    OPERATE_RET op_ret = OPRT_OK;
    uint32_t dtim = (s_tal_wifi.lps_dtim == 0) ? 1 : s_tal_wifi.lps_dtim;

    TAL_WIFI_LOCK();
    if (s_tal_wifi.lp_disable_cnt > 0) {
        s_tal_wifi.lp_disable_cnt--;
    }
    PR_DEBUG("<tal_wifi_lpen> disable_cnt:%d", s_tal_wifi.lp_disable_cnt);
    if (!s_tal_wifi.lp_disable_cnt) {
        op_ret = tkl_wifi_set_lp_mode(TRUE, dtim);
    }
    TAL_WIFI_UNLOCK();
    if (OPRT_OK != op_ret) {
        PR_ERR("tal_wifi_lp: set wifi lp mode fail(%d)", op_ret);
    }

    return op_ret | tal_cpu_lp_enable();
}

/**
 * @brief disable wifi lowpower
 *
 * @param[in]       none
 * @return  OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_lp_disable(void)
{
    OPERATE_RET op_ret = OPRT_OK;

    op_ret = tal_cpu_lp_disable();
    TAL_WIFI_LOCK();
    if (!s_tal_wifi.lp_disable_cnt++) {
        op_ret |= tkl_wifi_set_lp_mode(FALSE, 0);
    }
    PR_DEBUG("<tal_wifi_lp_dis> disable_cnt:%d", s_tal_wifi.lp_disable_cnt);
    TAL_WIFI_UNLOCK();
    if (OPRT_OK != op_ret) {
        PR_ERR("tal_wifi_lp_dis: set wifi lp mode fail(%d)", op_ret);
    }
    return op_ret;
}

/**
 * @brief do wifi calibration
 *
 * @note called when test wifi
 *
 * @return true on success. faile on failure
 */
BOOL_T tal_wifi_rf_calibrated(void)
{
    return tkl_wifi_set_rf_calibrated();
}

/**
 * @brief wifi ioctl
 *
 * @param[in]       cmd     refer to WF_IOCTL_CMD_E
 * @param[in]       args    args associated with the command
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_wifi_ioctl(WF_IOCTL_CMD_E cmd, void *args)
{
    return tkl_wifi_ioctl(cmd, args);
}
