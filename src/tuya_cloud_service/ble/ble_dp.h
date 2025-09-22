/**
 * @file ble_dp.h
 * @brief Defines the interface for BLE data point (DP) reporting and processing
 * within the Tuya BLE SDK.
 *
 * This header file provides the declarations for functions involved in
 * reporting and processing BLE data points (DPs). It includes the definition of
 * the interface for reporting DPs from the device to the Tuya cloud and
 * processing incoming DPs from the cloud. The functionalities encapsulated in
 * this file are crucial for enabling communication between Tuya BLE devices and
 * the Tuya cloud platform, facilitating data exchange and control commands.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __BLE_DP__
#define __BLE_DP__

#include "ble_mgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reports the data point (DP) to the Tuya BLE device.
 *
 * This function is used to report the data point (DP) to the Tuya BLE device.
 *
 * @param[in] dpin Pointer to the structure containing the DP data to be
 * reported.
 * @return Returns the status of the DP report operation.
 *     - 0: Success
 *     - Other values: Error codes
 */
int tuya_ble_dp_report(dp_rept_in_t *dpin);

/**
 * @brief Processes the BLE session data point (DP) packet.
 *
 * This function is responsible for processing the BLE session data point
 * packet. It determines the type of the packet and performs the corresponding
 * action.
 *
 * @param packet The BLE packet to process.
 * @param priv_data A pointer to private data associated with the packet.
 */
void ble_session_dp_process(ble_packet_t *packet, void *priv_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
