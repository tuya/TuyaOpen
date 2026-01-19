/**
 * @file example_ble_peripheral.c
 * @brief BLE Peripheral role implementation for Tuya IoT projects.
 *
 * This file contains the implementation of a BLE (Bluetooth Low Energy) peripheral device using Tuya's IoT SDK.
 * It demonstrates initializing the BLE stack, setting up advertising data, handling BLE events such as connections,
 * disconnections, and data reception. The example aims to provide a basic framework for developers to build upon for
 * creating BLE peripheral devices capable of interacting with other BLE devices in a Tuya IoT ecosystem.
 *
 * Key functionalities include initializing BLE in peripheral mode, setting up advertising packets, responding to
 * connection requests, and handling various BLE-related events to maintain a stable and responsive BLE environment.
 *
 * @note This example is intended for educational purposes and may need to be adapted for production environments.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tal_bluetooth.h"
#include "tkl_output.h"
/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static bool     g_notify_enabled = false;

static uint8_t adv_data_const[31] = {
    0x02, 0x01, 0x06, 0x03, 0x02, 0xFD, 0xFD, 0x17, 0x16, 0x50, 0xFD, 0x41, 0x00, // Frame Control
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static uint8_t scan_rsp_data_const[31] = {
    0x17, // length
    0xFF, 0xD0, 0x07,
    0x00,       // Encry Mode(8)
    0x00, 0x00, // communication way bit0-mesh bit1-wifi bit2-zigbee bit3-NB
    0x00,       // FLAG
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, // 24
    0x09, 0x54, 0x59,
};

static TAL_BLE_PEER_INFO_T sg_ble_peripheral_info;

/***********************************************************
***********************function define**********************
***********************************************************/

// ...existing code...

/**
 * @brief bluetooth event callback function
 *
 * @param[in] p_event: bluetooth event
 * @return none
 */

static void __ble_peripheral_event_callback(TAL_BLE_EVT_PARAMS_T *p_event)
{
    PR_DEBUG("----------ble_peripheral event callback-------");
    PR_DEBUG("ble_peripheral event is : %d", p_event->type);
    switch (p_event->type) {
    case TAL_BLE_STACK_INIT: {
        PR_DEBUG("init Ble/Bt stack and start advertising.");
        if (p_event->ble_event.init == 0) {
            TAL_BLE_DATA_T adv_data;
            TAL_BLE_DATA_T rsp_data;

            adv_data.p_data = adv_data_const;
            adv_data.len = sizeof(adv_data_const);
            rsp_data.p_data = scan_rsp_data_const;
            rsp_data.len = sizeof(scan_rsp_data_const);

            tal_system_sleep(1000);
            tal_ble_advertising_data_set(&adv_data, &rsp_data);
            tal_ble_advertising_start(TUYAOS_BLE_DEFAULT_ADV_PARAM);
        }
        break;
    }
    case TAL_BLE_EVT_PERIPHERAL_CONNECT: {
        PR_DEBUG("ble_peripheral starts to connect...");
        if (p_event->ble_event.connect.result == 0) {
            TAL_BLE_DATA_T read_data;
            uint8_t read_buffer[512];

            memcpy(&sg_ble_peripheral_info, &p_event->ble_event.connect.peer, sizeof(TAL_BLE_PEER_INFO_T));
            memcpy(read_buffer, adv_data_const, sizeof(adv_data_const));
            memcpy(&read_buffer[sizeof(adv_data_const)], scan_rsp_data_const, sizeof(scan_rsp_data_const));

            read_data.p_data = read_buffer;
            read_data.len = sizeof(adv_data_const) + sizeof(scan_rsp_data_const);

            // Verify Read Char
            tal_ble_server_common_read_update(&read_data);
        } else {
            memset(&sg_ble_peripheral_info, 0, sizeof(TAL_BLE_PEER_INFO_T));
        }
        break;
    }
    case TAL_BLE_EVT_DISCONNECT: {
        PR_DEBUG("ble_peripheral disconnect.");
        tal_ble_advertising_start(TUYAOS_BLE_DEFAULT_ADV_PARAM);
        break;
    }
    case TAL_BLE_EVT_CONN_PARAM_UPDATE: {
        PR_DEBUG("Parameter update successfully!");
        // Show Connect Parameters Info
        PR_DEBUG("Conn Param Update: Min = %f ms, Max = %f ms, Latency = %d, Sup = %d ms",
                 p_event->ble_event.conn_param.conn.min_conn_interval * 1.25,
                 p_event->ble_event.conn_param.conn.max_conn_interval * 1.25,
                 p_event->ble_event.conn_param.conn.latency, p_event->ble_event.conn_param.conn.conn_sup_timeout * 10);
        break;
    }
    case TAL_BLE_EVT_MTU_REQUEST: {
        PR_DEBUG("MTU exchange request event.");
        PR_DEBUG("Get Response MTU Size = %d", p_event->ble_event.exchange_mtu.mtu);
        break;
    }
    case TAL_BLE_EVT_SUBSCRIBE: {
        // Subscription event: triggered after client writes CCCD
        bool notify_now = (p_event->ble_event.subscribe.cur_notify != 0);
        PR_NOTICE("SUBSCRIBE char=0x%04X notify:%d->%d indicate:%d->%d",
                  p_event->ble_event.subscribe.char_handle,
                  p_event->ble_event.subscribe.prev_notify,
                  p_event->ble_event.subscribe.cur_notify,
                  p_event->ble_event.subscribe.prev_indicate,
                  p_event->ble_event.subscribe.cur_indicate);

        g_notify_enabled = notify_now;

        if (g_notify_enabled) {
            // Send a sample notification indicating subscription enabled
            const char *ack = "NOTIFY ENABLED";
            TAL_BLE_DATA_T pkt = { (uint16_t)strlen(ack), (uint8_t *)ack};
            tal_ble_server_common_send(&pkt);
        }
        break;
    }
    case TAL_BLE_EVT_WRITE_REQ: {
        int i = 0;
        // Receive data
        PR_DEBUG("Get Device-Write Char Request");
        for (i = 0; i < p_event->ble_event.write_report.report.len; i++) {
            PR_DEBUG("device send data[%d]: %d", i, p_event->ble_event.write_report.report.p_data[i]);
        }
        break;
    }
    default:
        break;
    }
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    /*basic init*/
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();

    /*ble_peripheral init*/
    TUYA_CALL_ERR_LOG(tal_ble_bt_init(TAL_BLE_ROLE_PERIPERAL, __ble_peripheral_event_callback));

    return;
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif