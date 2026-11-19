/**
 * @file tal_bluetooth_mesh_device.h
 * @brief This header file defines the API for Bluetooth Mesh device operations
 * in Tuya's IoT SDK. It includes functions for initializing the mesh network,
 * managing device provisioning, sending and receiving mesh messages, handling
 * group addresses, and managing device elements and models. Additionally, it
 * provides APIs for setting and getting various mesh parameters such as UUID,
 * provision state, and primary element address. This file is crucial for
 * developers looking to integrate Bluetooth Mesh functionality into their Tuya
 * IoT applications.
 *
 * The APIs defined in this file abstract the underlying Bluetooth Mesh protocol
 * details, providing a simplified interface for application development. This
 * enables developers to focus on the application logic rather than the
 * complexities of the mesh network management.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_BLUETOOTH_MESH_DEVICE_H__
#define __TAL_BLUETOOTH_MESH_DEVICE_H__

#include "tal_bluetooth_mesh_def.h"

/**
 * @brief   Register the mesh access app data callback  .
 * @param   [in] access_data_cb: mesh access data receive callback
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tal_mesh_msg_recv_cb_init(tal_mesh_msg_recv_cb access_data_cb);

/**
 * @brief   Register the ble gap & gatt data callback  .
 * @param   [in] ble_event: ble gap & gatt data receive callback
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tal_mesh_ble_recv_cb_init(TAL_BLE_EVT_FUNC_CB ble_event);

/**
 * @brief   Function to send mesh data to unicast/group addr
 * @param   [in] src_addr  unicast addr of node,if use 0,it will use node
 * primary element addr
 * @param   [in] dst_addr  destinat addr
 * @param   [in] opcode  mesh op code
 * @param   [in] data  mesh data
 * @param   [in] data_len  data point
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tal_mesh_data_send(uint16_t src_addr, uint16_t dst_addr, uint32_t opcode, uint8_t *data, uint16_t data_len);

/**
 * @brief   Function to subscribe a group address for a model of the element.
 * @param   [in] opcode  to indicate subscribe or delete
 * @param   [in] ele_index  the index of element
 * @param   [in] group_addr  group address
 * @return  tx result
 * */
OPERATE_RET tal_group_addr_sub_set(uint32_t opcode, uint16_t ele_index, uint16_t group_addr);

/**
 * @brief   Function to get the subscribe addr list of a model of the element.
 * @param   [in] ele_index  the index of element
 * @param   [in] group_addr  group address
 * @return  subscribe addr list
 * */
uint16_t *tal_group_addr_sub_list_get(uint16_t ele_idx, uint16_t model_id);

/**
 * @brief   Function to register a elemennt.
 * @param   [in] element_index  index of element
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tal_element_register(uint16_t element_index);

/**
 * @brief   Function to register a model into the element.
 * @param   [in] element_index  index of element
 * @param   [in] model_id  model id such as: 0x1000 = generic on off model
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tal_model_register(uint16_t element_index, uint32_t model_id);

/**
 * @brief   Function to enable/disable the provision.
 *          If node is unprovision state,using disable param will close the GATT
 * unprovision adv and mesh unprovision beacon, and mesh fast provision will be
 * disabled.
 * @param   [in] enable  enable/disable
 * @return  SUCCESS
 *          ERROR
 * */
void tal_mesh_node_provision_enable(MESH_PROVISION_TYPE_T enable);

/**
 * @brief   Init the mesh node uuid.
 * @param   [in] uuid  mesh universally unique identifier
 * @return  NULL
 * */
void tal_mesh_uuid_set(uint8_t *uuid);

/**
 * @brief   Get the mesh node provision state.
 * @return  the provision state
 * */
uint8_t tal_get_if_prov_success(void);

/**
 * @brief   Get the mesh node primary element address.
 * @return  the primary element address
 * */
uint16_t tal_primary_ele_addr_get(void);

/**
 * @brief   Set the mesh node primary element address.
 * @param   [in] addr  unicast address
 * @param   [in] flash_save_en  save in flash or only update in ram
 * @return  NULL
 * */
void tal_primary_ele_addr_set(uint16_t addr, int flash_save_en);

/**
 * @brief   Set the mesh node network state.
 * @param   [in] net_state  network state
 * @return  SUCCESS
 *          ERROR
 * */
OPERATE_RET tal_mesh_network_state_set(MESH_NETWORK_STATE_SET_T net_state);

/**
 * @brief   The callback to recieve the network state.
 * @param   [in] state  network state
 * @return  SUCCESS
 *          ERROR
 * */
TUYA_WEAK_ATTRIBUTE void tal_mesh_state_callback(TAL_MESH_NET_STATE_T state);

/**
 * @brief   Set the firmware information which mesh provision use.
 * @param   [in] is_key  use product key or product id
 * @param   [in] product_id  product id
 * @param   [in] product_key  product key
 * @param   [in] version  firmware version
 * @param   [in] mesh_category  mesh category define by tuya
 * @param   [in] need_publish_addr  if the device need a publish addr
 * distributed by tuya cloud
 * @return  NULL
 * */
void tal_firmware_infor_set(uint8_t is_key, uint8_t *product_id, uint8_t *product_key, uint32_t version,
                            uint32_t mesh_category, uint8_t need_publish_addr);

/**
 * @brief   Enable/Disable mesh fast provision.
 * @param   [in] enable  0:disable 1:enable
 * @return  SUCCESS
 *          ERROR
 * */
void tal_mesh_fast_prov_enable(uint8_t enable);

#endif
