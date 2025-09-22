/**
 * @file tal_bluetooth.c
 * @brief This file contains the Bluetooth Low Energy (BLE) utility functions
 * and event callbacks for handling BLE operations in the Tuya Application
 * Layer. It includes functions for UUID conversion, GAP (Generic Access
 * Profile) event handling, and GATT (Generic Attribute Profile) event handling.
 *        The GAP event callback handles various BLE events such as connection,
 * disconnection, advertisement report, and connection parameter updates. The
 * GATT event callback manages events related to GATT operations like MTU
 * exchange and service discovery. This implementation supports both peripheral
 * and central roles, enabling the device to act as a BLE server or client. It
 * also includes conditional support for specific BLE characteristics related to
 * the Matter over Tuya project.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include <string.h>
#include "tal_bluetooth_def.h"
#include "tkl_bluetooth_def.h"
#include "tkl_bluetooth.h"
#include "tal_api.h"

#define TAL_COMMON_SERVICE_INDEX (0)

#define TAL_COMMON_CHAR_MAX_NUM (3)

#if (defined(ENABLE_MATTER_TUYA_BLE_CHAR) && (ENABLE_MATTER_TUYA_BLE_CHAR == 1))
#define TAL_COMMON_SERVICE_MAX_NUM    (2)
#define TAL_MATTER_TUYA_SERVICE_INDEX (1)
#define TAL_MATTER_TUYA_CHAR_MAX_NUM  (3)
#else
#define TAL_COMMON_SERVICE_MAX_NUM (1)
#endif

#ifndef TAL_BLE_SERVICE_VERSION
#define TAL_BLE_SERVICE_VERSION (2) // Default Service is version 2: Follow the latest service.
#endif

/**< Use TKL Definitions */
static TKL_BLE_GATTS_PARAMS_T tkl_ble_gatt_service = {0};
static TKL_BLE_SERVICE_PARAMS_T *ptkl_ble_service = NULL;
static TKL_BLE_CHAR_PARAMS_T *ptkl_ble_service_char = NULL;

static uint16_t tkl_ble_common_connect_handle = TKL_BLE_GATT_INVALID_HANDLE;

/**< Use TAL Definitions, TAL Only Support One GATT Link at one time, follow
 * Bluetooth Spec. */
static TAL_BLE_EVT_FUNC_CB tal_ble_event_callback;
#if (TY_HS_BLE_ROLE_CENTRAL)
static TAL_BLE_PEER_INFO_T tal_ble_peer = {0};
#endif

static __attribute__((unused)) uint16_t tal_ble_uuid16_convert(TKL_BLE_UUID_T *p_uuid)
{
    uint16_t uuid16 = 0xFFFF;

    if (p_uuid->uuid_type == TKL_BLE_UUID_TYPE_16) {
        return p_uuid->uuid.uuid16;
    } else if (p_uuid->uuid_type == TKL_BLE_UUID_TYPE_32) {
        /**< Not Support */
        return 0xFFFF;
    } else if (p_uuid->uuid_type == TKL_BLE_UUID_TYPE_128) {
        uuid16 = p_uuid->uuid.uuid128[13] << 8 | p_uuid->uuid.uuid128[12];
        return uuid16;
    }

    return 0xFFFF;
}

static void tkl_ble_kernel_gap_event_callback(TKL_BLE_GAP_PARAMS_EVT_T *p_event)
{
    TAL_BLE_EVT_PARAMS_T tal_event;

    if (p_event == NULL) {
        return;
    }

    memset(&tal_event, 0, sizeof(TAL_BLE_EVT_PARAMS_T));

    switch (p_event->type) {
    case TKL_BLE_EVT_STACK_INIT: {
        tal_event.type = TAL_BLE_STACK_INIT;
        tal_event.ble_event.init = p_event->result;
    } break;

    case TKL_BLE_EVT_STACK_DEINIT: {
        tal_event.type = TAL_BLE_STACK_DEINIT;
        tal_event.ble_event.init = p_event->result;
    } break;

    case TKL_BLE_GAP_EVT_CONNECT: {
        tal_event.ble_event.connect.result = p_event->result;
        if (p_event->gap_event.connect.role == TKL_BLE_ROLE_SERVER) {
            tal_event.type = TAL_BLE_EVT_PERIPHERAL_CONNECT;

            tkl_ble_common_connect_handle = p_event->conn_handle;
            tal_event.ble_event.connect.peer.conn_handle = p_event->conn_handle;
            tal_event.ble_event.connect.peer.peer_addr.type = p_event->gap_event.connect.peer_addr.type;
            memcpy(tal_event.ble_event.connect.peer.peer_addr.addr, p_event->gap_event.connect.peer_addr.addr, 6);

            tal_event.ble_event.connect.peer.char_handle[TAL_COMMON_WRITE_CHAR_INDEX] =
                ptkl_ble_service_char[TAL_COMMON_WRITE_CHAR_INDEX].handle;
            tal_event.ble_event.connect.peer.char_handle[TAL_COMMON_NOTIFY_CHAR_INDEX] =
                ptkl_ble_service_char[TAL_COMMON_NOTIFY_CHAR_INDEX].handle;
            tal_event.ble_event.connect.peer.char_handle[TAL_COMMON_READ_CHAR_INDEX] =
                ptkl_ble_service_char[TAL_COMMON_READ_CHAR_INDEX].handle;
#if (defined(ENABLE_MATTER_TUYA_BLE_CHAR) && (ENABLE_MATTER_TUYA_BLE_CHAR == 1))
            tal_event.ble_event.connect.peer.char_handle[TAL_MATTER_TUYA_T1_CHAR_INDEX] =
                ptkl_ble_service_char[TAL_MATTER_TUYA_T1_CHAR_INDEX].handle;
            tal_event.ble_event.connect.peer.char_handle[TAL_MATTER_TUYA_T2_CHAR_INDEX] =
                ptkl_ble_service_char[TAL_MATTER_TUYA_T2_CHAR_INDEX].handle;
            tal_event.ble_event.connect.peer.char_handle[TAL_MATTER_TUYA_T3_CHAR_INDEX] =
                ptkl_ble_service_char[TAL_MATTER_TUYA_T3_CHAR_INDEX].handle;
#endif

        } else {
#if (TY_HS_BLE_ROLE_CENTRAL)
            tal_ble_peer.conn_handle = p_event->conn_handle;
            if (tkl_ble_gattc_all_service_discovery(p_event->conn_handle) != 0) { // Try To Discovery All Service, And
                                                                                  // Find the correct service
                tal_event.type = TAL_BLE_EVT_CENTRAL_CONNECT_DISCOVERY;
                tal_event.ble_event.connect.result = OPRT_OS_ADAPTER_BLE_SVC_DISC_FAILED;
                memcpy(&tal_event.ble_event.connect.peer, &tal_ble_peer, sizeof(TAL_BLE_PEER_INFO_T));
            } else {
                /* We will still be in service finding procedure. Should not
                 * report any states into TAL Application */
                return;
            }
#endif
        }
    } break;

    case TKL_BLE_GAP_EVT_DISCONNECT: {
        if (p_event->gap_event.disconnect.role == TKL_BLE_ROLE_SERVER &&
            p_event->conn_handle == tkl_ble_common_connect_handle) {
            tkl_ble_common_connect_handle = TKL_BLE_GATT_INVALID_HANDLE;
        }

        tal_event.type = TAL_BLE_EVT_DISCONNECT;
        tal_event.ble_event.disconnect.peer.conn_handle = p_event->conn_handle;
        tal_event.ble_event.disconnect.reason = p_event->gap_event.disconnect.reason;
    } break;

    case TKL_BLE_GAP_EVT_ADV_REPORT: {
        tal_event.type = TAL_BLE_EVT_ADV_REPORT;
        tal_event.ble_event.adv_report.rssi = p_event->gap_event.adv_report.rssi;
        tal_event.ble_event.adv_report.data_len = (uint8_t)p_event->gap_event.adv_report.data.length;
        tal_event.ble_event.adv_report.p_data = p_event->gap_event.adv_report.data.p_data;
        memcpy(tal_event.ble_event.adv_report.peer_addr.addr, p_event->gap_event.adv_report.peer_addr.addr, 6);

        if (p_event->gap_event.adv_report.adv_type == TKL_BLE_ADV_DATA) {
            tal_event.ble_event.adv_report.adv_type = TAL_BLE_ADV_DATA;
        } else if (p_event->gap_event.adv_report.adv_type == TKL_BLE_RSP_DATA) {
            tal_event.ble_event.adv_report.adv_type = TAL_BLE_RSP_DATA;
        } else if (p_event->gap_event.adv_report.adv_type == TKL_BLE_ADV_RSP_DATA) {
            tal_event.ble_event.adv_report.adv_type = TAL_BLE_ADV_RSP_DATA;
        }

        if (p_event->gap_event.adv_report.peer_addr.type == TKL_BLE_GAP_ADDR_TYPE_PUBLIC) {
            tal_event.ble_event.adv_report.peer_addr.type = TAL_BLE_ADDR_TYPE_PUBLIC;
        } else if (p_event->gap_event.adv_report.peer_addr.type == TKL_BLE_GAP_ADDR_TYPE_RANDOM) {
            tal_event.ble_event.adv_report.peer_addr.type = TAL_BLE_ADDR_TYPE_RANDOM;
        } else {
            /**< Currently, we not support other types. And Tuya Bluetooth
             * Device will not use other types. */
            return;
        }
    } break;

    case TKL_BLE_GAP_EVT_CONN_PARAM_REQ: {
        tal_event.type = TAL_BLE_EVT_CONN_PARAM_REQ;
        tal_event.ble_event.conn_param.conn_handle = p_event->conn_handle;
    } break;

    case TKL_BLE_GAP_EVT_CONN_PARAM_UPDATE: {
        tal_event.type = TAL_BLE_EVT_CONN_PARAM_UPDATE;
        tal_event.ble_event.conn_param.conn_handle = p_event->conn_handle;

        tal_event.ble_event.conn_param.conn.conn_sup_timeout = p_event->gap_event.conn_param.conn_sup_timeout;
        tal_event.ble_event.conn_param.conn.max_conn_interval = p_event->gap_event.conn_param.conn_interval_max;
        tal_event.ble_event.conn_param.conn.min_conn_interval = p_event->gap_event.conn_param.conn_interval_min;
        tal_event.ble_event.conn_param.conn.latency = p_event->gap_event.conn_param.conn_latency;
    } break;

    case TKL_BLE_GAP_EVT_CONN_RSSI: {
        tal_event.type = TAL_BLE_EVT_CONN_RSSI;
        tal_event.ble_event.link_rssi.conn_handle = p_event->conn_handle;

        tal_event.ble_event.link_rssi.rssi = p_event->gap_event.link_rssi;
    } break;

    default: {
    }
        return;
    }

    if (tal_ble_event_callback) {
        tal_ble_event_callback(&tal_event);
    }
}

static void tkl_ble_kernel_gatt_event_callback(TKL_BLE_GATT_PARAMS_EVT_T *p_event)
{
    TAL_BLE_EVT_PARAMS_T tal_event;

    if (p_event == NULL) {
        return;
    }

    memset(&tal_event, 0, sizeof(TAL_BLE_EVT_PARAMS_T));

    switch (p_event->type) {
    case TKL_BLE_GATT_EVT_MTU_REQUEST: {
        tal_event.type = TAL_BLE_EVT_MTU_REQUEST;
        tal_event.ble_event.exchange_mtu.conn_handle = p_event->conn_handle;
        tal_event.ble_event.exchange_mtu.mtu = p_event->gatt_event.exchange_mtu;
    } break;
#if (TY_HS_BLE_ROLE_CENTRAL)
    case TKL_BLE_GATT_EVT_MTU_RSP: {
        tal_event.type = TAL_BLE_EVT_MTU_RSP;
        tal_event.ble_event.exchange_mtu.conn_handle = p_event->conn_handle;
        tal_event.ble_event.exchange_mtu.mtu = p_event->gatt_event.exchange_mtu;
    } break;

    case TKL_BLE_GATT_EVT_PRIM_SEV_DISCOVERY: {
        TKL_BLE_GATT_SVC_DISC_TYPE_T *p_svc_disc = &p_event->gatt_event.svc_disc;
        int svc_result = OPRT_INVALID_PARM;

        tal_event.type = TAL_BLE_EVT_CENTRAL_CONNECT_DISCOVERY;
        if (p_event->result == OPRT_OK) {

            /**< Find the All Characteristic. */
            uint8_t i = 0;
            for (i = 0; i < p_svc_disc->svc_num; i++) {
                if (tal_ble_uuid16_convert(&p_svc_disc->services[i].uuid) == TAL_BLE_CMD_SERVICE_UUID_V1 ||
                    tal_ble_uuid16_convert(&p_svc_disc->services[i].uuid) == TAL_BLE_CMD_SERVICE_UUID_V2) {
                    svc_result = tkl_ble_gattc_all_char_discovery(
                        p_event->conn_handle, p_svc_disc->services[i].start_handle, p_svc_disc->services[i].end_handle);
                    if (svc_result == 0) {
                        return;
                    }
                }
            }
            memcpy(&tal_event.ble_event.connect.peer, &tal_ble_peer, sizeof(TAL_BLE_PEER_INFO_T));
            tal_event.ble_event.connect.result = OPRT_OS_ADAPTER_BLE_SVC_DISC_FAILED;
        } else {
            /**< Return Error, Cannot Discovery Service */
            memcpy(&tal_event.ble_event.connect.peer, &tal_ble_peer, sizeof(TAL_BLE_PEER_INFO_T));
            tal_event.ble_event.connect.result = p_event->result;
        }
    } break;

    case TKL_BLE_GATT_EVT_CHAR_DISCOVERY: {

        TKL_BLE_GATT_CHAR_DISC_TYPE_T *p_char_disc = &p_event->gatt_event.char_disc;
        int char_result = OPRT_INVALID_PARM;

        tal_event.type = TAL_BLE_EVT_CENTRAL_CONNECT_DISCOVERY;
        if (p_event->result == OPRT_OK) {
            uint8_t i = 0;
            for (i = 0; i < p_char_disc->char_num; i++) {
                if (tal_ble_uuid16_convert(&p_char_disc->characteristics[i].uuid) == TAL_BLE_CMD_WRITE_CHAR_UUID_V1 ||
                    tal_ble_uuid16_convert(&p_char_disc->characteristics[i].uuid) == TAL_BLE_CMD_WRITE_CHAR_UUID_V2) {

                    tal_ble_peer.char_handle[TAL_COMMON_WRITE_CHAR_INDEX] = p_char_disc->characteristics[i].handle;
                } else if (tal_ble_uuid16_convert(&p_char_disc->characteristics[i].uuid) ==
                               TAL_BLE_CMD_NOTIFY_CHAR_UUID_V1 ||
                           tal_ble_uuid16_convert(&p_char_disc->characteristics[i].uuid) ==
                               TAL_BLE_CMD_NOTIFY_CHAR_UUID_V2) {

                    tal_ble_peer.char_handle[TAL_COMMON_NOTIFY_CHAR_INDEX] = p_char_disc->characteristics[i].handle;
                } else if (tal_ble_uuid16_convert(&p_char_disc->characteristics[i].uuid) ==
                           TAL_BLE_CMD_READ_CHAR_UUID_V2) {

                    tal_ble_peer.char_handle[TAL_COMMON_READ_CHAR_INDEX] = p_char_disc->characteristics[i].handle;
                }
            }

            /**< Find Characteristic Descriptor */
            if (tal_ble_peer.char_handle[TAL_COMMON_NOTIFY_CHAR_INDEX] != TKL_BLE_GATT_INVALID_HANDLE) {
                char_result = tkl_ble_gattc_char_desc_discovery(
                    p_event->conn_handle, tal_ble_peer.char_handle[TAL_COMMON_NOTIFY_CHAR_INDEX], 0xFFFF);
                if (char_result == OPRT_OK) {
                    return;
                }
            }

            memcpy(&tal_event.ble_event.connect.peer, &tal_ble_peer, sizeof(TAL_BLE_PEER_INFO_T));
            tal_event.ble_event.connect.result = OPRT_OS_ADAPTER_BLE_DESC_DISC_FAILED;
        } else {
            /**< Return Error, Cannot Discovery Service */
            memcpy(&tal_event.ble_event.connect.peer, &tal_ble_peer, sizeof(TAL_BLE_PEER_INFO_T));
            tal_event.ble_event.connect.result = p_event->result;
        }

    } break;

    case TKL_BLE_GATT_EVT_CHAR_DESC_DISCOVERY: {
        TKL_BLE_GATT_DESC_DISC_TYPE_T *p_desc_disc = &p_event->gatt_event.desc_disc;
        uint8_t cccd_enable[2] = {0x01, 0x00};
        int desc_result;

        tal_event.type = TAL_BLE_EVT_CENTRAL_CONNECT_DISCOVERY;
        if (p_event->result == OPRT_OK) {
            desc_result = tkl_ble_gattc_write_without_rsp(p_event->conn_handle, p_desc_disc->cccd_handle, cccd_enable,
                                                          sizeof(cccd_enable));
            if (desc_result != 0) {
                tal_event.ble_event.connect.result = OPRT_OS_ADAPTER_BLE_DESC_DISC_FAILED;
            } else if (p_event->conn_handle != tal_ble_peer.conn_handle) {
                tal_event.ble_event.connect.result = OPRT_OS_ADAPTER_BLE_HANDLE_ERROR;
                memcpy(&tal_event.ble_event.connect.peer, &tal_ble_peer, sizeof(TAL_BLE_PEER_INFO_T));
            } else {
                tal_event.ble_event.connect.result = OPRT_OK;
                memcpy(&tal_event.ble_event.connect.peer, &tal_ble_peer, sizeof(TAL_BLE_PEER_INFO_T));
            }
        } else {
            /**< Return Error, Cannot Discovery characteristic descriptor */
            tal_event.ble_event.connect.result = p_event->result;
            memcpy(&tal_event.ble_event.connect.peer, &tal_ble_peer, sizeof(TAL_BLE_PEER_INFO_T));
        }
    } break;
#endif
    case TKL_BLE_GATT_EVT_NOTIFY_TX: {
        tal_event.type = TAL_BLE_EVT_NOTIFY_TX;
        tal_event.ble_event.notify_result.conn_handle = p_event->conn_handle;

        tal_event.ble_event.notify_result.char_handle = p_event->gatt_event.notify_result.char_handle;
        tal_event.ble_event.notify_result.result = p_event->gatt_event.notify_result.result;
    } break;

    case TKL_BLE_GATT_EVT_WRITE_REQ: {
        tal_event.type = TAL_BLE_EVT_WRITE_REQ;
        tal_event.ble_event.write_report.peer.conn_handle = p_event->conn_handle;
        tal_event.ble_event.write_report.peer.char_handle[0] = p_event->gatt_event.write_report.char_handle;
        tal_event.ble_event.write_report.report.len = p_event->gatt_event.write_report.report.length;
        tal_event.ble_event.write_report.report.p_data = p_event->gatt_event.write_report.report.p_data;
    } break;

    case TKL_BLE_GATT_EVT_NOTIFY_INDICATE_RX: {
        tal_event.type = TAL_BLE_EVT_NOTIFY_RX;
        tal_event.ble_event.data_report.peer.conn_handle = p_event->conn_handle;
        tal_event.ble_event.data_report.peer.char_handle[0] = p_event->gatt_event.data_report.char_handle;
        tal_event.ble_event.data_report.report.len = p_event->gatt_event.data_report.report.length;
        tal_event.ble_event.data_report.report.p_data = p_event->gatt_event.data_report.report.p_data;
    } break;
#if (TY_HS_BLE_ROLE_CENTRAL)
    case TKL_BLE_GATT_EVT_READ_RX: {
        tal_event.type = TAL_BLE_EVT_READ_RX;
        tal_event.ble_event.data_read.peer.conn_handle = p_event->conn_handle;
        tal_event.ble_event.data_read.peer.char_handle[0] = p_event->gatt_event.data_read.char_handle;
        tal_event.ble_event.data_read.report.len = p_event->gatt_event.data_read.report.length;
        tal_event.ble_event.data_read.report.p_data = p_event->gatt_event.data_read.report.p_data;
    } break;
#endif
    case TKL_BLE_GATT_EVT_SUBSCRIBE: {
        if (p_event->result == OPRT_OK) {
            tal_event.type = TAL_BLE_EVT_SUBSCRIBE;
            tal_event.ble_event.subscribe.conn_handle = p_event->conn_handle;
            tal_event.ble_event.subscribe.char_handle = p_event->gatt_event.subscribe.char_handle;
            tal_event.ble_event.subscribe.reason = p_event->gatt_event.subscribe.reason;
            tal_event.ble_event.subscribe.prev_notify = p_event->gatt_event.subscribe.prev_notify;
            tal_event.ble_event.subscribe.cur_notify = p_event->gatt_event.subscribe.cur_notify;
            tal_event.ble_event.subscribe.prev_indicate = p_event->gatt_event.subscribe.prev_indicate;
            tal_event.ble_event.subscribe.cur_indicate = p_event->gatt_event.subscribe.cur_indicate;
        }
    } break;

    case TKL_BLE_GATT_EVT_READ_CHAR_VALUE: {
        tal_event.type = TAL_BLE_EVT_READ_CHAR;
        tal_event.ble_event.char_read.conn_handle = p_event->conn_handle;
        tal_event.ble_event.char_read.char_handle = p_event->gatt_event.char_read.char_handle;
        tal_event.ble_event.char_read.offset = p_event->gatt_event.char_read.offset;
    } break;

    default: {
        // Not Support
    }
        return;
    }

    if (tal_ble_event_callback) {
        tal_ble_event_callback(&tal_event);
    }
}

/**
 * @brief   Function for initializing the bluetooth
 * @param   [in] role:      set the ble/bt role
 *          [in] p_event:   Event handler provided by the user.
 * @return  SUCCESS         Initialized successfully.
 *          ERROR
 * */
OPERATE_RET tal_ble_bt_init(TAL_BLE_ROLE_E role, const TAL_BLE_EVT_FUNC_CB ble_event)
{
    uint8_t ble_stack_role = 0; // TKL_BLE_ROLE_SERVER;

    if (ble_event != NULL) {
        /**<  Get the TAL Event Callback. */
        tal_ble_event_callback = ble_event;

        /**< Register GAP And GATT Callback */
        tkl_ble_gap_callback_register(tkl_ble_kernel_gap_event_callback);
        tkl_ble_gatt_callback_register(tkl_ble_kernel_gatt_event_callback);
    }

    /**< Init Bluetooth Stack Role For Ble. */
    if ((role & TAL_BLE_ROLE_PERIPERAL) == TAL_BLE_ROLE_PERIPERAL ||
        (role & TAL_BLE_ROLE_BEACON) == TAL_BLE_ROLE_BEACON) {
        ble_stack_role |= TKL_BLE_ROLE_SERVER;
    }
    if ((role & TAL_BLE_ROLE_CENTRAL) == TAL_BLE_ROLE_CENTRAL) {
        ble_stack_role |= TKL_BLE_ROLE_CLIENT;
    }

    if (!ptkl_ble_service) {
        ptkl_ble_service = tal_malloc(sizeof(TKL_BLE_SERVICE_PARAMS_T) * TKL_BLE_GATT_SERVICE_MAX_NUM);
    }
    if (!ptkl_ble_service_char) {
        ptkl_ble_service_char = tal_malloc(sizeof(TKL_BLE_CHAR_PARAMS_T) * TKL_BLE_GATT_CHAR_MAX_NUM);
    }

    if ((role & TAL_BLE_ROLE_PERIPERAL) == TAL_BLE_ROLE_PERIPERAL) {
        TKL_BLE_GATTS_PARAMS_T *p_ble_services = &tkl_ble_gatt_service;
        *p_ble_services = (TKL_BLE_GATTS_PARAMS_T){
            .svc_num = TAL_COMMON_SERVICE_MAX_NUM,
            .p_service = ptkl_ble_service,
        };

#if (defined(TAL_BLE_SERVICE_VERSION) && (TAL_BLE_SERVICE_VERSION == 2))
        /**< Add Tuya Common Service */
        *(ptkl_ble_service + TAL_COMMON_SERVICE_INDEX) = (TKL_BLE_SERVICE_PARAMS_T){
            .handle = TKL_BLE_GATT_INVALID_HANDLE,
            .svc_uuid =
                {
                    .uuid_type = TKL_BLE_UUID_TYPE_16,
                    .uuid.uuid16 = TAL_BLE_CMD_SERVICE_UUID_V2,
                },
            .type = TKL_BLE_UUID_SERVICE_PRIMARY,
            .char_num = TAL_COMMON_CHAR_MAX_NUM,
            .p_char = ptkl_ble_service_char,
        };

        /**< Add Write Characteristic */
        *(ptkl_ble_service_char + TAL_COMMON_WRITE_CHAR_INDEX) = (TKL_BLE_CHAR_PARAMS_T){
            .handle = TKL_BLE_GATT_INVALID_HANDLE,
            .char_uuid =
                {
                    .uuid_type = TKL_BLE_UUID_TYPE_128,
                    .uuid.uuid128 = TAL_BLE_CMD_WRITE_CHAR_UUID128_V2,
                },
            .property = TKL_BLE_GATT_CHAR_PROP_WRITE | TKL_BLE_GATT_CHAR_PROP_WRITE_NO_RSP,
            .permission = TKL_BLE_GATT_PERM_READ | TKL_BLE_GATT_PERM_WRITE,
            .value_len = 244,
        };

        /**< Add Notify Characteristic */
        *(ptkl_ble_service_char + TAL_COMMON_NOTIFY_CHAR_INDEX) = (TKL_BLE_CHAR_PARAMS_T){
            .handle = TKL_BLE_GATT_INVALID_HANDLE,
            .char_uuid =
                {
                    .uuid_type = TKL_BLE_UUID_TYPE_128,
                    .uuid.uuid128 = TAL_BLE_CMD_NOTIFY_CHAR_UUID128_V2,
                },
            .property = TKL_BLE_GATT_CHAR_PROP_NOTIFY,
            .permission = TKL_BLE_GATT_PERM_READ | TKL_BLE_GATT_PERM_WRITE,
            .value_len = 244,
        };

        /**< Add Read Characteristic */
        *(ptkl_ble_service_char + TAL_COMMON_READ_CHAR_INDEX) = (TKL_BLE_CHAR_PARAMS_T){
            .handle = TKL_BLE_GATT_INVALID_HANDLE,
            .char_uuid =
                {
                    .uuid_type = TKL_BLE_UUID_TYPE_128,
                    .uuid.uuid128 = TAL_BLE_CMD_READ_CHAR_UUID128_V2,
                },
            .property = TKL_BLE_GATT_CHAR_PROP_READ,
            .permission = TKL_BLE_GATT_PERM_READ,
            .value_len = 244,
        };
#else
        /**< Add Tuya Common Service */
        *(ptkl_ble_service + TAL_COMMON_SERVICE_INDEX) = (TKL_BLE_SERVICE_PARAMS_T){
            .handle = TKL_BLE_GATT_INVALID_HANDLE,
            .svc_uuid =
                {
                    .uuid_type = TKL_BLE_UUID_TYPE_16,
                    .uuid.uuid16 = TAL_BLE_CMD_SERVICE_UUID_V1,
                },
            .type = TKL_BLE_UUID_SERVICE_PRIMARY,
            .char_num = 2,
            .p_char = ptkl_ble_service_char,
        };

        /**< Add Write Characteristic */

        *(ptkl_ble_service_char + TAL_COMMON_WRITE_CHAR_INDEX) = (TKL_BLE_CHAR_PARAMS_T){
            .handle = TKL_BLE_GATT_INVALID_HANDLE,
            .char_uuid =
                {
                    .uuid_type = TKL_BLE_UUID_TYPE_16,
                    .uuid.uuid16 = TAL_BLE_CMD_WRITE_CHAR_UUID_V1,
                },
            .property = TKL_BLE_GATT_CHAR_PROP_WRITE | TKL_BLE_GATT_CHAR_PROP_WRITE_NO_RSP,
            .permission = TKL_BLE_GATT_PERM_READ | TKL_BLE_GATT_PERM_WRITE,
            .value_len = 244,
        };

        /**< Add Notify Characteristic */
        *(ptkl_ble_service_char + TAL_COMMON_NOTIFY_CHAR_INDEX) = (TKL_BLE_CHAR_PARAMS_T){
            .handle = TKL_BLE_GATT_INVALID_HANDLE,
            .char_uuid =
                {
                    .uuid_type = TKL_BLE_UUID_TYPE_16,
                    .uuid.uuid16 = TAL_BLE_CMD_NOTIFY_CHAR_UUID_V1,
                },
            .property = TKL_BLE_GATT_CHAR_PROP_NOTIFY,
            .permission = TKL_BLE_GATT_PERM_READ | TKL_BLE_GATT_PERM_WRITE,
            .value_len = 244,
        };

#endif
        if (tkl_ble_gatts_service_add(p_ble_services) != 0) {
            return OPRT_OS_ADAPTER_BLE_INIT_FAILED;
        }
    }

    if (tkl_ble_stack_init(ble_stack_role) != OPRT_OK) {
        return OPRT_OS_ADAPTER_BLE_INIT_FAILED;
    }
    return OPRT_OK;
}

/**
 * @brief   Function for deinitializing the bluetooth, or specify one role
 * @param   [in] role:      set the ble/bt role
 * @return  SUCCESS         Deinitialized successfully.
 *          ERROR
 * */
OPERATE_RET tal_ble_bt_deinit(TAL_BLE_ROLE_E role)
{
    if (ptkl_ble_service_char) {
        tal_free(ptkl_ble_service_char);
    }
    if (ptkl_ble_service) {
        tal_free(ptkl_ble_service);
    }

    return tkl_ble_stack_deinit(role);
}

/**
 * @brief   Set the local Bluetooth identity address.
 * @param   [in] p_addr:    pointer to local address
 * @return  SUCCESS         Set Address successfully.
 *          ERROR
 * */
OPERATE_RET tal_ble_address_set(TAL_BLE_ADDR_T const *p_addr)
{
    TKL_BLE_GAP_ADDR_T ble_addr = {0};

    if (p_addr == NULL) {
        return OPRT_INVALID_PARM;
    }

    ble_addr.type = p_addr->type;
    memcpy(ble_addr.addr, p_addr->addr, 6);
    return tkl_ble_gap_addr_set(&ble_addr);
}

/**
 * @brief   Get the local Bluetooth identity address.
 * @param   [out] p_addr: pointer to local address
 * @return  SUCCESS         Set Address successfully.
 *          ERROR
 * */
OPERATE_RET tal_ble_address_get(TAL_BLE_ADDR_T *p_addr)
{
    TKL_BLE_GAP_ADDR_T tal_addr = {0};

    if (p_addr == NULL) {
        return OPRT_INVALID_PARM;
    }

    tkl_ble_gap_address_get(&tal_addr);

    p_addr->type = tal_addr.type;
    memcpy(p_addr->addr, tal_addr.addr, 6);

    return OPRT_OK;
}

/**
 * @brief   Get Max Link Size
 * @param   [out] max_mtu:  set mtu size.
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tal_ble_bt_link_max(uint16_t *p_maxlink)
{
    if (p_maxlink == NULL) {
        return OPRT_INVALID_PARM;
    }

    tkl_ble_stack_gatt_link(p_maxlink);

    return OPRT_OK;
}

/**
 * @brief   Start advertising
 * @param  [in] p_adv_param : pointer to advertising parameters, see
 * TAL_BLE_ADV_PARAMS_T for details
 * @return  SUCCESS             Successfully started advertising procedure.
 *          ERR_INVALID_STATE   Not in advertising state.
 * */
OPERATE_RET tal_ble_advertising_start(TAL_BLE_ADV_PARAMS_T const *p_adv_param)
{
    TKL_BLE_GAP_ADV_PARAMS_T tal_adv_params = {0};

    if (p_adv_param == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_adv_params.adv_type = p_adv_param->adv_type;
    tal_adv_params.direct_addr.type = p_adv_param->direct_addr.type;
    tal_adv_params.adv_interval_min = p_adv_param->adv_interval_min;
    tal_adv_params.adv_interval_max = p_adv_param->adv_interval_max;
    tal_adv_params.adv_channel_map = 0x01 | 0x02 | 0x04;

    memcpy(tal_adv_params.direct_addr.addr, p_adv_param->direct_addr.addr, 6);

    return tkl_ble_gap_adv_start(&tal_adv_params);
}

/**
 * @brief   Setting advertising data
 * @param   [in] p_adv : Data to be used in advertisement packets
 *          [in] p_rsp : Data to be used in scan response packets.
 * @return  SUCCESS             Successfully set advertising data.
 *          ERR_INVALID_param   Invalid parameter(s) supplied.
 * */
OPERATE_RET tal_ble_advertising_data_set(TAL_BLE_DATA_T *p_adv, TAL_BLE_DATA_T *p_scan_rsp)
{
    TKL_BLE_DATA_T adv_data = {0};
    TKL_BLE_DATA_T scan_rsp_data = {0};

    if (p_adv != NULL) {
        adv_data.length = p_adv->len;
        adv_data.p_data = p_adv->p_data;
    } else {
        adv_data.length = 0;
        adv_data.p_data = NULL;
    }

    if (p_scan_rsp != NULL) {
        scan_rsp_data.length = p_scan_rsp->len;
        scan_rsp_data.p_data = p_scan_rsp->p_data;
    } else {
        scan_rsp_data.length = 0;
        scan_rsp_data.p_data = NULL;
    }

    return tkl_ble_gap_adv_rsp_data_set(&adv_data, &scan_rsp_data);
}

/**
 * @brief   Stop advertising
 * @param   void
 * @return  SUCCESS             Successfully stopped advertising procedure.
 *          ERR_INVALID_STATE   Not in advertising state.
 * */
OPERATE_RET tal_ble_advertising_stop(void)
{
    return tkl_ble_gap_adv_stop();
}

/**
 * @brief   Updating advertising data
 * @param   [in] p_adv :        Data to be used in advertisement packets
 *          [in] p_rsp :        Data to be used in scan response packets.
 * @return  SUCCESS             Successfully update advertising data.
 *          ERR_INVALID_STATE   Invalid parameter(s) supplied.
 * @Note:   If being in advertising, we will do update adv+rsp data.
 *          If not being in advertising, we will only do set adv+rsp data.
 * */
OPERATE_RET tal_ble_advertising_data_update(TAL_BLE_DATA_T *p_adv, TAL_BLE_DATA_T *p_scan_rsp)
{
    TKL_BLE_DATA_T adv_data = {0};
    TKL_BLE_DATA_T scan_rsp_data = {0};

    if (p_adv != NULL) {
        adv_data.length = p_adv->len;
        adv_data.p_data = p_adv->p_data;
    } else {
        adv_data.length = 0;
        adv_data.p_data = NULL;
    }

    if (p_scan_rsp != NULL) {
        scan_rsp_data.length = p_scan_rsp->len;
        scan_rsp_data.p_data = p_scan_rsp->p_data;
    } else {
        scan_rsp_data.length = 0;
        scan_rsp_data.p_data = NULL;
    }

    return tkl_ble_gap_adv_rsp_data_update(&adv_data, &scan_rsp_data);
}

/**
 * @brief   Start scanning
 * @param :
 *          [in] scan_param:    scan parameters including interval, windows
 * and timeout
 * @return  SUCCESS             Successfully initiated scanning procedure.
 *
 * @note    Other default scanning parameters : public address, no
 * whitelist.
 *          The scan response and adv data are given through
 * TAL_BLE_EVT_ADV_REPORT event !!!!!!
 */
OPERATE_RET tal_ble_scan_start(TAL_BLE_SCAN_PARAMS_T const *p_scan_param)
{
    TKL_BLE_GAP_SCAN_PARAMS_T tal_scan_params = {0};

    tal_scan_params.extended = 0;
    tal_scan_params.active = ((p_scan_param->type == TAL_BLE_SCAN_TYPE_ACTIVE) ? 1 : 0);
    tal_scan_params.scan_phys = TKL_BLE_GAP_PHY_1MBPS;
    tal_scan_params.interval = p_scan_param->scan_interval;
    tal_scan_params.window = p_scan_param->scan_window;
    tal_scan_params.timeout = p_scan_param->timeout;
    tal_scan_params.scan_channel_map = 0x01 | 0x02 | 0x04;

    return tkl_ble_gap_scan_start(&tal_scan_params);
}

/**
 * @brief   Stop scanning
 * @param   void
 * @return  SUCCESS         Successfully stopped scanning procedure.
 *          ERROR           Not in scanning state.
 * */
OPERATE_RET tal_ble_scan_stop(void)
{
    return tkl_ble_gap_scan_stop();
}

/**
 * @brief   Get the received signal strength for the last connection event.
 * @param   [in]peer:       conn_handle Connection handle.
 * @return  SUCCESS         Successfully read the RSSI.
 *          ERROR           No sample is available.
 *          The rssi reading is given through TAL_BLE_EVT_LINK_RSSI event !!!!!!
 * */
OPERATE_RET tal_ble_rssi_get(const TAL_BLE_PEER_INFO_T peer)
{
    uint16_t conn_handle = peer.conn_handle;

    return tkl_ble_gap_rssi_get(conn_handle);
}

/**
 * @brief   Update connection parameters.
 * @details In the central role this will initiate a Link Layer connection
 * parameter update procedure, otherwise in the peripheral role, this will send
 * the corresponding L2CAP request and wait for the central to perform the
 * procedure..
 * @param :
 *          [in] conn_param: connect parameters including interval, peer info
 * @return  SUCCESS         Successfully initiated connecting procedure.
 *
 *          The connection parameters updating is given through
 * TAL_BLE_EVT_CONN_PARAM_UPDATE event !!!!!!
 */
OPERATE_RET
tal_ble_conn_param_update(const TAL_BLE_PEER_INFO_T peer, TAL_BLE_CONN_PARAMS_T const *p_conn_params)
{
    uint16_t conn_handle = peer.conn_handle;
    TKL_BLE_GAP_CONN_PARAMS_T param = {0};

    if (p_conn_params == NULL) {
        return OPRT_INVALID_PARM;
    }

    param.conn_interval_min = p_conn_params->min_conn_interval;
    param.conn_interval_max = p_conn_params->max_conn_interval;
    param.conn_latency = p_conn_params->latency;
    param.conn_sup_timeout = p_conn_params->conn_sup_timeout;
    param.connection_timeout = p_conn_params->connection_timeout;

    return tkl_ble_gap_conn_param_update(conn_handle, &param);
}

/**
 * @brief   Start connecting one peer
 * @param : [in] peer info.     include address and address type
 *          [in] conn_param:    connect parameters including interval, peer info
 * \
 * @return  SUCCESS             Successfully initiated connecting procedure.
 *
 *          The connection event is given through TAL_BLE_EVT_CENTRAL_CONNECT
 * event !!!!!!
 */
#if (TY_HS_BLE_ROLE_CENTRAL)
OPERATE_RET
tal_ble_connect_and_discovery(const TAL_BLE_PEER_INFO_T peer, TAL_BLE_CONN_PARAMS_T const *p_conn_params)
{
    TKL_BLE_GAP_ADDR_T tal_conn_addr = {0};
    TKL_BLE_GAP_SCAN_PARAMS_T tal_scan_params = {0};
    TKL_BLE_GAP_CONN_PARAMS_T tal_conn_params = {0};

    if (p_conn_params == NULL) {
        tal_conn_params.conn_interval_max = 0x12;
        tal_conn_params.conn_interval_min = 0x12;
        tal_conn_params.conn_latency = 0;
        tal_conn_params.conn_sup_timeout = 0x100;
    } else {
        tal_conn_params.conn_interval_max = p_conn_params->max_conn_interval;
        tal_conn_params.conn_interval_min = p_conn_params->min_conn_interval;
        tal_conn_params.conn_latency = p_conn_params->latency;
        tal_conn_params.conn_sup_timeout = p_conn_params->conn_sup_timeout;
    }

    tal_scan_params.interval = 0x0010;
    tal_scan_params.window = 0x0010;

    if (peer.peer_addr.type == TAL_BLE_ADDR_TYPE_RANDOM) {
        tal_conn_addr.type = TKL_BLE_GAP_ADDR_TYPE_RANDOM;
    } else {
        tal_conn_addr.type = TKL_BLE_GAP_ADDR_TYPE_PUBLIC;
    }
    memcpy(tal_conn_addr.addr, peer.peer_addr.addr, 6);

    memset(&tal_ble_peer, 0, sizeof(TAL_BLE_PEER_INFO_T));

    tal_ble_peer.char_handle[TAL_COMMON_WRITE_CHAR_INDEX] = TKL_BLE_GATT_INVALID_HANDLE;
    tal_ble_peer.char_handle[TAL_COMMON_NOTIFY_CHAR_INDEX] = TKL_BLE_GATT_INVALID_HANDLE;
    tal_ble_peer.char_handle[TAL_COMMON_READ_CHAR_INDEX] = TKL_BLE_GATT_INVALID_HANDLE;

    /**Assign Peer Info Into TAL Peer Info*/
    memcpy(&tal_ble_peer, &peer, sizeof(TAL_BLE_PEER_INFO_T));

    /**< After do connecting one device, we will discovery all service and char.
     */
    return tkl_ble_gap_connect(&tal_conn_addr, &tal_scan_params, &tal_conn_params);
}
#endif
/**
 * @brief   Disconnect from peer
 * @param   [in] conn_handle: the connection handle
 * @return  SUCCESS         Successfully disconnected.
 *          ERROR STATE     Not in connnection.
 *          ERROR PARAMETER
 * @note    Both of ble central and ble peripheral can call this function.
 *              For ble peripheral, we only need to fill connect handle.
 *               For ble central, we need to fill connect handle and peer addr.
 * */
OPERATE_RET tal_ble_disconnect(const TAL_BLE_PEER_INFO_T peer)
{
    uint16_t conn_handle = peer.conn_handle;
    return tkl_ble_gap_disconnect(conn_handle, TKL_BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
}

/**
 * @brief   [Ble Peripheral] Notify Command characteristic value by
 * characteristic handle
 * @param   [in] p_data: pointer to data, refer to TAL_BLE_DATA_T
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 * */
OPERATE_RET tal_ble_server_common_notify(uint16_t index, TAL_BLE_DATA_T *p_data)
{
    if (p_data == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (tkl_ble_common_connect_handle != TKL_BLE_GATT_INVALID_HANDLE &&
        ptkl_ble_service_char[index].handle != TKL_BLE_GATT_INVALID_HANDLE) {
        return tkl_ble_gatts_value_notify(tkl_ble_common_connect_handle, ptkl_ble_service_char[index].handle,
                                          p_data->p_data, p_data->len);
    }

    return OPRT_OS_ADAPTER_BLE_NOTIFY_FAILED;
}

/**
 * @brief   [Ble Peripheral] Notify Command characteristic value
 * @param   [in] p_data: pointer to data, refer to TAL_BLE_DATA_T
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 * */
OPERATE_RET tal_ble_server_common_send(TAL_BLE_DATA_T *p_data)
{
    if (p_data == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (tkl_ble_common_connect_handle != TKL_BLE_GATT_INVALID_HANDLE &&
        ptkl_ble_service_char[TAL_COMMON_NOTIFY_CHAR_INDEX].handle != TKL_BLE_GATT_INVALID_HANDLE) {

        return tkl_ble_gatts_value_notify(tkl_ble_common_connect_handle,
                                          ptkl_ble_service_char[TAL_COMMON_NOTIFY_CHAR_INDEX].handle, p_data->p_data,
                                          p_data->len);
    }

    return OPRT_OS_ADAPTER_BLE_NOTIFY_FAILED;
}

/**
 * @brief   [Ble Peripheral] Set Read Command characteristic value by
 * characteristic handle
 * @param   [in] index: char index
 * @param   [in] p_data: pointer to data, refer to TAL_BLE_DATA_T
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 * */
OPERATE_RET tal_ble_server_common_read_update_ext(uint16_t index, TAL_BLE_DATA_T *p_data)
{
    if (p_data == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (tkl_ble_common_connect_handle != TKL_BLE_GATT_INVALID_HANDLE &&
        ptkl_ble_service_char[index].handle != TKL_BLE_GATT_INVALID_HANDLE) {

        return tkl_ble_gatts_value_set(tkl_ble_common_connect_handle, ptkl_ble_service_char[index].handle,
                                       p_data->p_data, p_data->len);
    }

    return OPRT_INVALID_PARM;
}

/**
 * @brief   [Ble Peripheral] Set Read Command characteristic value
 * @param   [in] p_data: pointer to data, refer to TAL_BLE_DATA_T
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 * */
OPERATE_RET tal_ble_server_common_read_update(TAL_BLE_DATA_T *p_data)
{
    if (p_data == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (tkl_ble_common_connect_handle != TKL_BLE_GATT_INVALID_HANDLE &&
        ptkl_ble_service_char[TAL_COMMON_READ_CHAR_INDEX].handle != TKL_BLE_GATT_INVALID_HANDLE) {

        return tkl_ble_gatts_value_set(tkl_ble_common_connect_handle,
                                       ptkl_ble_service_char[TAL_COMMON_READ_CHAR_INDEX].handle, p_data->p_data,
                                       p_data->len);
    }

    return OPRT_INVALID_PARM;
}
#if (TY_HS_BLE_ROLE_CENTRAL)
/**
 * @brief   [Ble Central] write data to peer within command channel
 * @param   [in] p_data: pointer to data, refer to TAL_BLE_DATA_T
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 * */
OPERATE_RET tal_ble_client_common_send(const TAL_BLE_PEER_INFO_T peer, TAL_BLE_DATA_T *p_data)
{
    if (p_data == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (peer.conn_handle != TKL_BLE_GATT_INVALID_HANDLE &&
        peer.char_handle[TAL_COMMON_WRITE_CHAR_INDEX] != TKL_BLE_GATT_INVALID_HANDLE) {
        return tkl_ble_gattc_write_without_rsp(peer.conn_handle, peer.char_handle[TAL_COMMON_WRITE_CHAR_INDEX],
                                               p_data->p_data, p_data->len);
    }

    return OPRT_OS_ADAPTER_BLE_WRITE_FAILED;
}

/**
 * @brief   [Ble Central] read data from peer within command channel
 * @param   [in] p_data: pointer to data, refer to TAL_BLE_DATA_T
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 * */
OPERATE_RET tal_ble_client_common_read(const TAL_BLE_PEER_INFO_T peer)
{
    if (peer.conn_handle != TKL_BLE_GATT_INVALID_HANDLE &&
        peer.char_handle[TAL_COMMON_READ_CHAR_INDEX] != TKL_BLE_GATT_INVALID_HANDLE) {
        return tkl_ble_gattc_read(peer.conn_handle, peer.char_handle[TAL_COMMON_READ_CHAR_INDEX]);
    }

    return OPRT_OS_ADAPTER_BLE_READ_FAILED;
}
#endif
/**
 * @brief   [Ble Peripheral] Reply to an ATT_MTU exchange request by sending an
 * Exchange MTU Response to the client.
 * @param   [in] att_mtu: mtu size
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 *          The MTU negotiation is given through TAL_BLE_EVT_MTU_REQUEST event
 * !!!!!!
 * */
OPERATE_RET tal_ble_server_exchange_mtu_reply(const TAL_BLE_PEER_INFO_T peer, uint16_t server_mtu)
{
    uint16_t conn_handle = peer.conn_handle;

    if (server_mtu < 23 || server_mtu > 247) {
        return OPRT_INVALID_PARM;
    }

    return tkl_ble_gatts_exchange_mtu_reply(conn_handle, server_mtu);
}

#if (TY_HS_BLE_ROLE_CENTRAL)
/**
 * @brief   [Ble Central] Start an ATT_MTU exchange by sending an Exchange MTU
 * Request to the server.
 * @param   [in] att_mtu: mtu size
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 *          ERROR_INVALID_CONN_HANDLE Invalid connection handle.
 *          ERROR_INVALID_STATE Invalid connection state or an ATT_MTU exchange
 * was already requested once.
 * */
OPERATE_RET tal_ble_client_exchange_mtu_request(const TAL_BLE_PEER_INFO_T peer, uint16_t client_mtu)
{
    uint16_t conn_handle = peer.conn_handle;

    if (client_mtu < 23 || client_mtu > 247) {
        return OPRT_INVALID_PARM;
    }

    return tkl_ble_gattc_exchange_mtu_request(conn_handle, client_mtu);
}

#endif