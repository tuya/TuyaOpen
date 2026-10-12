/**
 * @file ble_wifi_backup.c
 * @brief Backup network management over the BLE large data transparent channel.
 *
 * The APP manages the backup network list through subcommand 0x000F of the
 * transparent channel. The requests and replies carry the very same JSON as the
 * cloud protocol, so the parsing and the business logic live in
 * tuya_wifi_backup.c and this file only moves bytes.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "tal_log.h"
#include "tal_memory.h"

#include "ble_channel.h"
#include "ble_protocol.h"
#include "tuya_wifi_backup.h"

#define BLE_WIFI_BACKUP_HDR_LEN 4

/**
 * @brief Send a backup network reply to the APP.
 *
 * @param[in] data the JSON reply
 * @param[in] len  length of @p data in bytes
 *
 * @return OPRT_OK on success
 */
static int __ble_wifi_backup_send(const uint8_t *data, uint32_t len)
{
    uint32_t resp_len = len + BLE_WIFI_BACKUP_HDR_LEN;
    uint8_t *resp     = NULL;

    if (NULL == data || 0 == len) {
        return OPRT_INVALID_PARM;
    }

    /* ble_channle_ack takes the ownership of the buffer */
    resp = (uint8_t *)tal_malloc(resp_len);
    if (NULL == resp) {
        PR_ERR("malloc err");
        return OPRT_MALLOC_FAILED;
    }

    resp[0] = 0x00;
    resp[1] = 0x00;
    resp[2] = 0x00;
    resp[3] = FRM_DATA_TRANS_SUBCMD_WIFI_PROTECT;
    memcpy(resp + BLE_WIFI_BACKUP_HDR_LEN, data, len);

    ble_channle_ack(FRM_UPLINK_TRANSPARENT_SPEC_REQ, resp, resp_len);

    return OPRT_OK;
}

/**
 * @brief Handle one backup network request from the APP.
 *
 * @param[in] data      the JSON request, not NUL terminated
 * @param[in] len       length of @p data in bytes
 * @param[in] user_data unused
 */
static void __ble_wifi_backup_query(void *data, uint32_t len, void *user_data)
{
    OPERATE_RET rt;

    if (NULL == data || 0 == len) {
        return;
    }

    rt = tuya_wifi_backup_proto_process((const char *)data, len, TUYA_WIFI_BACKUP_TRANS_BLE);
    if (OPRT_OK != rt) {
        PR_ERR("wifi backup request failed: %d", rt);
    }
}

void ble_wifi_backup_register(void)
{
    tuya_wifi_backup_sender_set(TUYA_WIFI_BACKUP_TRANS_BLE, __ble_wifi_backup_send);
    ble_channel_add(BLE_CHANNLE_WIFI_PROTECT, __ble_wifi_backup_query, NULL);
}

void ble_wifi_backup_unregister(void)
{
    ble_channel_del(BLE_CHANNLE_WIFI_PROTECT);
    tuya_wifi_backup_sender_set(TUYA_WIFI_BACKUP_TRANS_BLE, NULL);
}
