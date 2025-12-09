/**
 * @file ble_netcfg.c
 * @brief Implementation of BLE network configuration.
 * This file contains the logic for handling BLE-based network configuration
 * processes, including parsing network configuration commands, managing network
 * configuration sessions, and invoking callbacks upon the completion of network
 * configuration.
 *
 * It utilizes the Tuya BLE SDK to facilitate the network configuration of
 * devices over BLE, ensuring a seamless setup process for connecting Tuya smart
 * devices to the network.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "netcfg.h"
#include "tal_event_info.h"
#include "ble_mgr.h"
#include "tuya_register_center.h"
#include "ble_channel.h"

typedef struct {
    netcfg_args_t netcfg_args;
    netcfg_info_t netcfg_info;
    netcfg_finish_cb_t netcfg_finish_cb;
} ble_netcfg_t;

static ble_netcfg_t g_bt_netcfg_handle;

extern int tuya_ble_adv_update(void);

static void __handle_net_cfg(void *data, void *user_data)
{
    uint8_t result = 0;
    uint8_t resp[5];

    cJSON *json = cJSON_Parse(data);
    if (NULL == json) {
        PR_ERR(" json parse error.");
        result = (uint8_t)OPRT_CJSON_PARSE_ERR;
        goto __exit;
    }

    if (cJSON_GetObjectItem(json, "ssid") == NULL || cJSON_GetObjectItem(json, "token") == NULL) {
        PR_ERR(" json get error.");
        result = (uint8_t)OPRT_CJSON_GET_ERR;
        goto __exit;
    }

    char *ssid = cJSON_GetObjectItem(json, "ssid")->valuestring;
    char *token = cJSON_GetObjectItem(json, "token")->valuestring;
    char *passwd = NULL;
    if (cJSON_GetObjectItem(json, "pwd") != NULL) {
        passwd = cJSON_GetObjectItem(json, "pwd")->valuestring;
    }

    if (NULL == passwd) {
        PR_ERR(" json get passwd error.");
        result = (uint8_t)OPRT_CJSON_GET_ERR;
        goto __exit;
    }

    PR_NOTICE("cfg ssid:%s, passwd:%s, token:%s", ssid, passwd, token);
    strncpy((char *)g_bt_netcfg_handle.netcfg_info.ssid, ssid, WIFI_SSID_LEN + 1);
    g_bt_netcfg_handle.netcfg_info.s_len = strlen(ssid);
    strncpy((char *)g_bt_netcfg_handle.netcfg_info.passwd, passwd, WIFI_PASSWD_LEN + 1);
    g_bt_netcfg_handle.netcfg_info.p_len = strlen(passwd);
    strncpy((char *)g_bt_netcfg_handle.netcfg_info.token, token, WL_TOKEN_LEN + 1);
    g_bt_netcfg_handle.netcfg_info.t_len = strlen(token);
    g_bt_netcfg_handle.netcfg_finish_cb(NETCFG_TUYA_BLE, &g_bt_netcfg_handle.netcfg_info);

    cJSON *reg = cJSON_GetObjectItem(json, "reg");
    if (reg) {
        tuya_register_center_save(RCS_APP, reg);
    }

__exit:
    if (json) {
        cJSON_Delete(json);
    }
    resp[1] = 0x00; // for blt timer task, set as not subpacket, not response
    resp[2] = 0x00;
    resp[3] = FRM_DATA_TRANS_SUBCMD_BT_NETCFG;
    resp[4] = result; // result code: 0x00 - success, other - fail
    tuya_ble_send(FRM_UPLINK_TRANSPARENT_REQ, 0, resp, 5);
}

/**
 * @brief Starts the BLE network configuration.
 *
 * This function starts the BLE network configuration process.
 *
 * @param type The type of network configuration.
 * @param cb The callback function to be called when the network configuration
 * is finished.
 * @param args Additional arguments to be passed to the callback function.
 * @return The result of the operation.
 */
int ble_netcfg_start(int type, netcfg_finish_cb_t cb, void *args)
{
    int ret = OPRT_OK;
    PR_DEBUG("bt netcfg start");
    /*init callback */
    g_bt_netcfg_handle.netcfg_finish_cb = cb;
    ble_channel_add(BLE_CHANNLE_NETCFG, __handle_net_cfg, NULL);

    tuya_ble_adv_update();

    return ret;
}

/**
 * @brief Stops the BLE network configuration.
 *
 * This function stops the BLE network configuration by deleting the BLE channel
 * used for network configuration.
 *
 * @param type The type of BLE network configuration.
 * @return The result of the operation. Returns OPRT_OK if successful, otherwise
 * an error code.
 */
int ble_netcfg_stop(int type)
{
    int ret = OPRT_OK;

    PR_DEBUG("bt netcfg stop");
    ble_channel_del(BLE_CHANNLE_NETCFG);

    return ret;
}

/**
 * @brief Initializes the BLE network configuration.
 *
 * This function initializes the BLE network configuration by clearing the
 * g_bt_netcfg_handle structure and registering the BLE network configuration
 * with the netcfg_register function.
 *
 * @param[in] netcfg_args Pointer to the netcfg_args_t structure containing the
 * network configuration arguments.
 *
 * @return Returns the result of the netcfg_register function.
 */
int ble_netcfg_init(netcfg_args_t *netcfg_args)
{
    memset(&g_bt_netcfg_handle, 0, sizeof(g_bt_netcfg_handle));

    return netcfg_register(NETCFG_TUYA_BLE, ble_netcfg_start, ble_netcfg_stop);
}
