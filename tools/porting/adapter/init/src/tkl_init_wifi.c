/**
 * @file tkl_init_wifi.h
 * @brief Common process - tkl init wifi description
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_init_wifi.h"

const TKL_WIFI_DESC_T TKL_WIFI = {
    .init = tkl_wifi_init,
    .set_cur_channel = tkl_wifi_set_cur_channel,
    .get_cur_channel = tkl_wifi_get_cur_channel,
    .set_sniffer = tkl_wifi_set_sniffer,
    .set_mac = tkl_wifi_set_mac,
    .get_mac = tkl_wifi_get_mac,
    .set_work_mode = tkl_wifi_set_work_mode,
    .get_work_mode = tkl_wifi_get_work_mode,
    .set_country_code = tkl_wifi_set_country_code,
    .set_lp_mode = tkl_wifi_set_lp_mode,
    .set_rf_calibrated = tkl_wifi_set_rf_calibrated,
    .send_mgnt = tkl_wifi_send_mgnt,
    .register_recv_mgnt_callback = tkl_wifi_register_recv_mgnt_callback,
    .ioctl = tkl_wifi_ioctl,
#ifndef TUYA_HOSTAPD_SUPPORT
    .scan_ap = tkl_wifi_scan_ap,
    .start_ap = tkl_wifi_start_ap,
    .stop_ap = tkl_wifi_stop_ap,
    .release_ap = tkl_wifi_release_ap,
    .station_connect = tkl_wifi_station_connect,
    .station_fast_connect = tkl_wifi_station_fast_connect,
    .station_disconnect = tkl_wifi_station_disconnect,
    .get_ip = tkl_wifi_get_ip,
    .get_bssid = tkl_wifi_get_bssid,
    .station_get_status = tkl_wifi_station_get_status,
    .get_connected_ap_info = tkl_wifi_get_connected_ap_info,
    .station_get_conn_ap_rssi = tkl_wifi_station_get_conn_ap_rssi,
#endif
};

/**
 * @brief register wifi description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE TKL_WIFI_DESC_T *tkl_wifi_desc_get()
{
    return (TKL_WIFI_DESC_T *)&TKL_WIFI;
}

#ifdef TUYA_HOSTAPD_SUPPORT
const TKL_WIFI_HOSTAP_DESC_T c_wifi_hostap_desc = {
    .ioctl = tkl_hostap_ioctl_inet,
};

/**
 * @brief register wifi hostap description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE TKL_WIFI_HOSTAP_DESC_T *tkl_wifi_hostap_desc_get()
{
    return (TKL_WIFI_HOSTAP_DESC_T *)&c_wifi_hostap_desc;
}
#endif
