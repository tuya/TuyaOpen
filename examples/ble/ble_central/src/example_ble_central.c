/**
 * @file example_ble_central.c
 * @brief Bluetooth Low Energy (BLE) central role implementation example.
 *
 * This file demonstrates the implementation of a BLE central device using Tuya's APIs.
 * It includes initializing the BLE stack, scanning for BLE advertisements, parsing advertisement data, and managing BLE
 * connections. The example showcases how to set up a BLE central device to interact with BLE peripheral devices,
 * facilitating communication and data exchange in IoT applications.
 *
 * The implementation covers essential BLE operations such as starting and stopping scans, handling advertisement
 * reports, and initiating connections with peripheral devices. It also demonstrates how to configure scanning
 * parameters and process advertisement packets to extract useful information such as device addresses, advertisement
 * types, and signal strength (RSSI).
 *
 * @note This example is designed to provide a basic understanding of operating a BLE central device using Tuya's BLE
 * APIs and should be adapted to fit specific application requirements.
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

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief bluebooth event callback function
 *
 * @param[in] p_event: bluebooth event
 * @return none
 */
static void __ble_central_event_callback(TAL_BLE_EVT_PARAMS_T *p_event)
{
    PR_DEBUG("----------ble_central event callback-------");
    PR_DEBUG("ble_central event is : %d", p_event->type);
    switch (p_event->type) {
    case TAL_BLE_EVT_ADV_REPORT: {
        int i = 0;

        /*printf peer addr and addr type*/
        PR_DEBUG_RAW("Scanf device peer addr: ");
        for (i = 0; i < 6; i++) {
            PR_DEBUG_RAW("  %d", p_event->ble_event.adv_report.peer_addr.addr[i]);
        }
        PR_DEBUG_RAW(" \r\n");

        if (TAL_BLE_ADDR_TYPE_RANDOM == p_event->ble_event.adv_report.peer_addr.type) {
            PR_DEBUG("Peer addr type is random address");
        } else {
            PR_DEBUG("Peer addr type is public address");
        }

        /*printf ADV type*/
        switch (p_event->ble_event.adv_report.adv_type) {
        case TAL_BLE_ADV_DATA: {
            PR_DEBUG("ADV data only!");
            break;
        }

        case TAL_BLE_RSP_DATA: {
            PR_DEBUG("Scan Response Data only!");
            break;
        }

        case TAL_BLE_ADV_RSP_DATA: {
            PR_DEBUG("ADV data and Scan Response Data!");
            break;
        }
        default:
            break;
        }

        /*printf ADV rssi*/
        PR_DEBUG("Received Signal Strength Indicator : %d", p_event->ble_event.adv_report.rssi);

        /*printf ADV data*/
        PR_DEBUG("Advertise packet data length : %d", p_event->ble_event.adv_report.data_len);
        PR_DEBUG_RAW("Advertise packet data: ");
        for (i = 0; i < p_event->ble_event.adv_report.data_len; i++) {
            PR_DEBUG_RAW("  0x%02X", p_event->ble_event.adv_report.p_data[i]);
        }
        PR_DEBUG_RAW(" \r\n");

        break;
    }
    default:
        break;
    }

    tal_ble_scan_stop();
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;
    TAL_BLE_SCAN_PARAMS_T scan_cfg;
    memset(&scan_cfg, 0, sizeof(TAL_BLE_SCAN_PARAMS_T));

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

    /*ble_central init*/
    PR_NOTICE("ble central init start");
    TUYA_CALL_ERR_GOTO(tal_ble_bt_init(TAL_BLE_ROLE_CENTRAL, __ble_central_event_callback), __EXIT);

    /*start scan*/
    scan_cfg.type = TAL_BLE_SCAN_TYPE_ACTIVE;
    scan_cfg.scan_interval = 0x400;
    scan_cfg.scan_window = 0x400;
    scan_cfg.timeout = 0xFFFF;
    scan_cfg.filter_dup = 0;
    TUYA_CALL_ERR_GOTO(tal_ble_scan_start(&scan_cfg), __EXIT);

    PR_NOTICE("ble central init success");

__EXIT:
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