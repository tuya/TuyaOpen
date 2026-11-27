/**
 * @file ble_wifi_backup.h
 * @brief Backup network management over the BLE large data transparent channel.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __BLE_WIFI_BACKUP_H__
#define __BLE_WIFI_BACKUP_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hook the backup network protocol onto the BLE transparent channel.
 */
void ble_wifi_backup_register(void);

/**
 * @brief Detach the backup network protocol from the BLE transparent channel.
 */
void ble_wifi_backup_unregister(void);

#ifdef __cplusplus
}
#endif

#endif /* __BLE_WIFI_BACKUP_H__ */
