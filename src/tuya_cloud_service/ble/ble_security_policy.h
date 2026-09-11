/**
 * @file ble_security_policy.h
 * @brief Central authorization policy for inbound BLE commands.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __BLE_SECURITY_POLICY_H__
#define __BLE_SECURITY_POLICY_H__

#include <stdbool.h>
#include <stdint.h>

bool ble_security_packet_allowed(bool is_bound, bool is_paired, uint16_t type, uint8_t encryption_mode);

#endif
