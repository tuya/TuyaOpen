/**
 * @file tal_bluetooth.h
 * @brief This file contains the declarations of the Bluetooth API functions for
 * Tuya's IoT SDK. It provides functionalities for initializing and
 * deinitializing Bluetooth, setting and getting the local Bluetooth address,
 * managing advertising and scanning processes, handling connections and
 * disconnections, and managing data transmission and reception over Bluetooth.
 * It supports both BLE (Bluetooth Low Energy) and classic Bluetooth roles and
 * operations, making it a comprehensive solution for Bluetooth communication in
 * IoT applications.
 *
 * The API functions defined in this file allow for a wide range of operations
 * including setting advertising parameters and data, starting and stopping
 * advertising, starting and stopping scanning, connecting to and disconnecting
 * from peers, and exchanging data with connected devices. It also includes
 * functions for managing the Bluetooth stack's connection parameters and
 * reading the received signal strength indicator (RSSI) values. This file is
 * essential for developers looking to integrate Bluetooth functionalities into
 * their IoT applications using Tuya's SDK.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_BLUETOOTH_H__
#define __TAL_BLUETOOTH_H__

#include "tal_bluetooth_def.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Function for initializing the bluetooth
 * @param   [in] role:      set the ble/bt role
 *          [in] p_event:   Event handler provided by the user.
 * @return  SUCCESS         Initialized successfully.
 *          ERROR
 * */
OPERATE_RET tal_ble_bt_init(TAL_BLE_ROLE_E role, const TAL_BLE_EVT_FUNC_CB ble_event);

/**
 * @brief   Function for deinitializing the bluetooth, or specify one role
 * @param   [in] role:      set the ble/bt role
 * @return  SUCCESS         Deinitialized successfully.
 *          ERROR
 * */
OPERATE_RET tal_ble_bt_deinit(TAL_BLE_ROLE_E role);

/**
 * @brief   Set the local Bluetooth identity address.
 * @param   [in] p_addr:    pointer to local address
 * @return  SUCCESS         Set Address successfully.
 *          ERROR
 * */
OPERATE_RET tal_ble_address_set(TAL_BLE_ADDR_T const *p_addr);

/**
 * @brief   Get the local Bluetooth identity address.
 * @param   [out] p_addr: pointer to local address
 * @return  SUCCESS         Set Address successfully.
 *          ERROR
 * */
OPERATE_RET tal_ble_address_get(TAL_BLE_ADDR_T *p_addr);

/**
 * @brief   Get Max Link Size
 * @param   [out] max_mtu:  set mtu size.
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tal_ble_bt_link_max(uint16_t *p_maxlink);

/**
 * @brief   Start advertising
 * @param  [in] p_adv_param : pointer to advertising parameters, see
 * TAL_BLE_ADV_PARAMS_T for details
 * @return  SUCCESS             Successfully started advertising procedure.
 *          ERR_INVALID_STATE   Not in advertising state.
 * */
OPERATE_RET tal_ble_advertising_start(TAL_BLE_ADV_PARAMS_T const *p_adv_param);

/**
 * @brief   Setting advertising data
 * @param   [in] p_adv : Data to be used in advertisement packets
 *          [in] p_rsp : Data to be used in scan response packets.
 * @return  SUCCESS             Successfully set advertising data.
 *          ERR_INVALID_param   Invalid parameter(s) supplied.
 * */
OPERATE_RET tal_ble_advertising_data_set(TAL_BLE_DATA_T *p_adv, TAL_BLE_DATA_T *p_scan_rsp);

/**
 * @brief   Stop advertising
 * @param   void
 * @return  SUCCESS             Successfully stopped advertising procedure.
 *          ERR_INVALID_STATE   Not in advertising state.
 * */
OPERATE_RET tal_ble_advertising_stop(void);

/**
 * @brief   Updating advertising data
 * @param   [in] p_adv :        Data to be used in advertisement packets
 *          [in] p_rsp :        Data to be used in scan response packets.
 * @return  SUCCESS             Successfully update advertising data.
 *          ERR_INVALID_STATE   Invalid parameter(s) supplied.
 * @Note:   If being in advertising, we will do update adv+rsp data.
 *          If not being in advertising, we will only do set adv+rsp data.
 * */
OPERATE_RET tal_ble_advertising_data_update(TAL_BLE_DATA_T *p_adv, TAL_BLE_DATA_T *p_scan_rsp);

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
OPERATE_RET tal_ble_scan_start(TAL_BLE_SCAN_PARAMS_T const *p_scan_param);

/**
 * @brief   Stop scanning
 * @param   void
 * @return  SUCCESS         Successfully stopped scanning procedure.
 *          ERROR           Not in scanning state.
 * */
OPERATE_RET tal_ble_scan_stop(void);

/**
 * @brief   Get the received signal strength for the last connection event.
 * @param   [in]peer:       conn_handle Connection handle.
 * @return  SUCCESS         Successfully read the RSSI.
 *          ERROR           No sample is available.
 *          The rssi reading is given through TAL_BLE_EVT_LINK_RSSI event !!!!!!
 * */
OPERATE_RET tal_ble_rssi_get(const TAL_BLE_PEER_INFO_T peer);

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
tal_ble_conn_param_update(const TAL_BLE_PEER_INFO_T peer, TAL_BLE_CONN_PARAMS_T const *p_conn_params);

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
OPERATE_RET
tal_ble_connect_and_discovery(const TAL_BLE_PEER_INFO_T peer, TAL_BLE_CONN_PARAMS_T const *p_conn_params);

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
OPERATE_RET tal_ble_disconnect(const TAL_BLE_PEER_INFO_T peer);

/**
 * @brief   [Ble Peripheral] Notify Command characteristic value
 * @param   [in] p_data: pointer to data, refer to TAL_BLE_DATA_T
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 * */
OPERATE_RET tal_ble_server_common_send(TAL_BLE_DATA_T *p_data);

/**
 * @brief   [Ble Peripheral] Notify Command characteristic value by handle
 * @param   [in] p_data: pointer to data, refer to TAL_BLE_DATA_T
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 * */
OPERATE_RET tal_ble_server_common_notify(uint16_t index, TAL_BLE_DATA_T *p_data);

/**
 * @brief   [Ble Peripheral] Set Read Command characteristic value by
 * characteristic handle
 * @param   [in] index: char index
 * @param   [in] p_data: pointer to data, refer to TAL_BLE_DATA_T
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 * */
OPERATE_RET tal_ble_server_common_read_update_ext(uint16_t index, TAL_BLE_DATA_T *p_data);

/**
 * @brief   [Ble Peripheral] Set Read Command characteristic value
 * @param   [in] p_data: pointer to data, refer to TAL_BLE_DATA_T
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 * */
OPERATE_RET tal_ble_server_common_read_update(TAL_BLE_DATA_T *p_data);

/**
 * @brief   [Ble Central] write data to peer within command channel
 * @param   [in] p_data: pointer to data, refer to TAL_BLE_DATA_T
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 * */
OPERATE_RET tal_ble_client_common_send(const TAL_BLE_PEER_INFO_T peer, TAL_BLE_DATA_T *p_data);

/**
 * @brief   [Ble Central] read data from peer within command channel
 * @param   [in] p_data: pointer to data, refer to TAL_BLE_DATA_T
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 * */
OPERATE_RET tal_ble_client_common_read(const TAL_BLE_PEER_INFO_T peer);

/**
 * @brief   [Ble Peripheral] Reply to an ATT_MTU exchange request by sending an
 * Exchange MTU Response to the client.
 * @param   [in] att_mtu: mtu size
 * @return  SUCCESS
 *          ERROR Refer to platform error code
 *          The MTU negotiation is given through TAL_BLE_EVT_MTU_REQUEST event
 * !!!!!!
 * */
OPERATE_RET tal_ble_server_exchange_mtu_reply(const TAL_BLE_PEER_INFO_T peer, uint16_t server_mtu);

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
OPERATE_RET tal_ble_client_exchange_mtu_request(const TAL_BLE_PEER_INFO_T peer, uint16_t client_mtu);

#ifdef __cplusplus
}
#endif

#endif
