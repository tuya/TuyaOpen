/**
 * @file ble_security_policy.c
 * @brief Central authorization policy for inbound BLE commands.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "ble_security_policy.h"

#include "ble_cryption.h"
#include "ble_protocol.h"

bool ble_security_packet_allowed(bool is_bound, bool is_paired, uint16_t type, uint8_t encryption_mode)
{
    if (is_bound && encryption_mode == ENCRYPTION_MODE_NONE) {
        return false;
    }
    if (!is_paired && type != FRM_QRY_DEV_INFO_REQ && type != FRM_PAIR_REQ) {
        return false;
    }
    return true;
}
