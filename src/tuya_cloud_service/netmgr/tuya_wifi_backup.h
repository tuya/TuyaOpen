/**
 * @file tuya_wifi_backup.h
 * @brief Backup network (backup AP) service for the WiFi connection.
 *
 * The backup network keeps a small list of known good APs besides the one the
 * device is currently using. When the current AP becomes unreachable the device
 * falls back to one of the backups, and the APP can manage that list (query,
 * add, delete, switch manually) over BLE or over the cloud.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_WIFI_BACKUP_H__
#define __TUYA_WIFI_BACKUP_H__

#include "tuya_cloud_types.h"
#include "tal_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of APs kept in the backup list.
 *
 * The oldest entry is dropped when the list is full. It can be overridden by
 * defining TUYA_WIFI_BACKUP_AP_NUM, otherwise the Kconfig value is used.
 */
#ifndef TUYA_WIFI_BACKUP_AP_NUM
#ifdef WIFI_BACKUP_AP_NUM
#define TUYA_WIFI_BACKUP_AP_NUM WIFI_BACKUP_AP_NUM
#else
#define TUYA_WIFI_BACKUP_AP_NUM 3
#endif
#endif

#define TUYA_WIFI_BACKUP_VERSION 1

/**
 * @brief State reported while the backup list is being switched.
 *
 */
enum tuya_wfpt_state {
    TUYA_WFPT_SWICTH_START,    //!< a switch/connect attempt started
    TUYA_WFPT_SWICTH_ORIGINAL, //!< rolled back to the original AP
    TUYA_WFPT_SWICTH_SUCCESS,  //!< the target AP is connected
    TUYA_WFPT_SWICTH_FALIED,   //!< every candidate failed
};

typedef uint8_t tuya_wfpt_state_t;
typedef void    tuya_wfpt_state_cb_t(tuya_wfpt_state_t state);

/**
 * @brief Link type reported by @ref tuya_wifi_backup_conf_attr.
 *
 */
typedef enum {
    TUYA_WFPT_WIFI_MODE    = 0,
    TUYA_WFPT_WIRED_MODE   = 1,
    TUYA_WFPT_4G_MODE      = 2,
    TUYA_WFPT_INVALID_MODE = 0xFF
} tuya_wfpt_mode_t;

#define TUYA_WFPT_BACKUP_FLAG (1 << 0) //!< the link itself is a backup link
#define TUYA_WFPT_SWITCH_FLAG (1 << 1)

/**
 * @brief Link attributes reported to the APP by "wifiInfo".
 *
 */
typedef struct {
    uint8_t  network; //!< @ref tuya_wfpt_mode_t
    int8_t   rssi;
    uint32_t flags;
} tuya_wfpt_attr_t;

/**
 * @brief Transport a backup-network request arrived on.
 *
 */
typedef enum {
    TUYA_WIFI_BACKUP_TRANS_CLOUD = 0,
    TUYA_WIFI_BACKUP_TRANS_BLE   = 1,
} tuya_wifi_backup_trans_t;

/**
 * @brief Send a reply to the APP over the BLE transparent channel.
 *
 * @param[in] data the JSON reply
 * @param[in] len  length of @p data in bytes
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef int (*tuya_wifi_backup_sender_t)(const uint8_t *data, uint32_t len);

/**
 * @brief Start the backup network service.
 *
 * Loads the persisted backup list and registers the cloud protocol handlers.
 * Called by the WiFi connection once the WiFi driver is up.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_wifi_backup_init(void);

/**
 * @brief Disable the backup network service.
 *
 * @note The service is enabled by default. Call this before
 *       @ref tuya_wifi_backup_init to turn it off for this boot.
 */
void tuya_wifi_backup_disable(void);

/**
 * @brief Whether the backup network service is enabled.
 *
 * @return true if enabled, false otherwise
 */
bool tuya_wifi_backup_is_enable(void);

/**
 * @brief Register a callback that may override the reported link attributes.
 *
 * @param[in] conf_attr the callback, called for every "wifiInfo" request
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_wifi_backup_conf_attr(int (*conf_attr)(tuya_wfpt_attr_t *attr));

/**
 * @brief Register a callback notified on every backup switch state change.
 *
 * @param[in] cb the callback
 */
void tuya_wifi_backup_state_cb_set(tuya_wfpt_state_cb_t *cb);

/**
 * @brief Whether a switch or a backup connect attempt is in progress.
 *
 * @return OPRT_OK when busy, OPRT_COM_ERROR otherwise
 */
OPERATE_RET tuya_wifi_backup_is_switching(void);

/**
 * @brief Number of backup APs, excluding the one currently connected.
 *
 * @return the count
 */
uint8_t tuya_wifi_backup_count_get(void);

/**
 * @brief Add the currently connected AP to the backup list.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_wifi_backup_current_ap_add(void);

/**
 * @brief Check whether the currently connected AP is in the backup list.
 *
 * @return OPRT_OK if present, OPRT_NOT_FOUND if not, error code on parameter error
 */
OPERATE_RET tuya_wifi_backup_current_ap_in_list(void);

/**
 * @brief Switch to the given AP, adding the previous one to the backup list.
 *
 * @param[in] ssid   target AP ssid
 * @param[in] passwd target AP password
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_wifi_backup_switch(const char *ssid, const char *passwd);

/**
 * @brief Report every known AP: the current one first, then the backups.
 *
 * @param[in] cb called once per AP
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_wifi_backup_list_get(void (*cb)(const char *ssid, const char *passwd));

/**
 * @brief Add an AP to the backup list without connecting to it.
 *
 * @param[in] ssid   AP ssid
 * @param[in] passwd AP password, NULL is treated as an open network
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_wifi_backup_test_add(const char *ssid, const char *passwd);

/**
 * @brief Drop every backup AP, both in memory and on flash.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_wifi_backup_clear(void);

/**
 * @brief Try to reconnect using the backups that are present in a scan result.
 *
 * Candidates are ordered with the current AP first, then by descending RSSI.
 *
 * @param[in] ap_list scanned APs
 * @param[in] ap_num  number of scanned APs
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_wifi_backup_ap_check(AP_IF_S *ap_list, uint8_t ap_num);

/**
 * @brief Register the sender used to reply over the BLE transparent channel.
 *
 * @param[in] trans  transport the sender serves
 * @param[in] sender the sender, NULL to unregister
 */
void tuya_wifi_backup_sender_set(tuya_wifi_backup_trans_t trans, tuya_wifi_backup_sender_t sender);

/**
 * @brief Handle one APP request, either "wifiInfo", "wifiList", "wifiSecure",
 *        "wifiSettingBackup", "wifiSettingNew", "wifiSwitchBackup" or
 *        "wifiSwitchNew".
 *
 * @param[in] json  the request, {"reqType":"..","data":{..}}
 * @param[in] len   length of @p json in bytes, it is not required to be NUL terminated
 * @param[in] trans transport the request arrived on
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_wifi_backup_proto_process(const char *json, uint32_t len, tuya_wifi_backup_trans_t trans);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_WIFI_BACKUP_H__ */
